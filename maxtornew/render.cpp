#include "render.h"
#include "paramdlg.h"
#include "resource.h"
#include "common.h"
#include <ri.h>
#include "../common/common.h"
#include <process.h>
#include <fcntl.h>
#include <io.h>
#include <bitmap.h>

HINSTANCE
GetMaxtorNewHinstance();

unsigned WINAPI 
RendermanWorkerEntry(void* context);

static void
displayBucket(
    const Bucket* bucket,
    Bitmap* bm);

static int
processBuckets(
    std::queue<Bucket*>* queue,
    std::queue<Bucket*>* defunctQueue,
    Bitmap* targetBmp);

struct RendermanThreadContext
{
    MaxRenderContext* MaxContext;
};

RendParamDlg *MaxtorNewRenderer::CreateParamDialog(
    IRendParams *rp,
    BOOL /* inProgressWindow */)
{
    rp->AddRollupPage(
        GetMaxtorNewHinstance(),
        MAKEINTRESOURCE(IDD_RENDPARAMS),
        RendParamsDlgProc,
        PRODUCT_NAME " Params");

    return new MaxtorNewRendParamDlg;
}
int 
MaxtorNewRenderer::Open(INode *,INode *,ViewParams *,RendParams &,HWND,DefaultLight *,int,RendProgressCallback *)
{
    AllocConsole();
    stdout->_file = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    stderr->_file = _open_osfhandle((long)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    return TRUE;
}

int 
MaxtorNewRenderer::Render(
    TimeValue timeVal,
    Bitmap *targetBmp,
    FrameRendParams &frameRendParams,
    HWND hwndOwner,
    RendProgressCallback *progressCb,
    ViewParams *viewParams)
{
    MaxRenderContext maxRenderCtxt;
    int bucketsProcessed = 0;

    // Assume buckets are 32x32 pixels big for estimation. We can control this
    // via a param to the renderer, but this is the default for pixie.
    //
    const int typicalBucketSize = 32;

    // We use this to update the rendering progress in the UI.
    //
    int totalBucketEstimate = 
        (targetBmp->Width() * targetBmp->Height()) /
        (typicalBucketSize * typicalBucketSize);

    RendermanThreadContext thdCtxt = {0};
    thdCtxt.MaxContext = &maxRenderCtxt;
    maxRenderCtxt.QueueNotEmpty = CreateEvent(NULL, FALSE, FALSE, NULL);

    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL, 0, RendermanWorkerEntry, &thdCtxt, 0, NULL);

    if (!hThread)
    {
        int lastError;
        errno_t err = _get_errno(&lastError);
        assert(!err);

        // Failed to make thread.
        return FALSE;
    }

    std::queue<Bucket*> localBuckets;

    // Consume buckets produced by display driver.
    //
    while (!maxRenderCtxt.AllBucketsSent)
    {
        // There is a race here between when the above bool is checked, and
        // the below event is waited on, so we need to check this flag again
        // after we wake up.
        //

        // Wait for something to come onto the queue.
        //
        WaitForSingleObject(maxRenderCtxt.QueueNotEmpty, INFINITE);

        // Check if we're terminating. 'AllBucketsSent' means the display driver's
        // displayData routine will never be called any more, which means
        // the event will never be set again. We'll process any remaining buckets
        // outside the loop.
        //
        if (maxRenderCtxt.AllBucketsSent)
        {
            break;
        }

        // It doesn't matter if the other thread pre-empted us here and pushed 5
        // more things onto the queue before we took the lock, because once we
        // take the lock, we're going to consume the entire queue anyway.
        // So the fact that the event is unsignalled after we leave the CS is fine.
        //
        EnterCriticalSection(&maxRenderCtxt.CritSec);
    
        // Fetch the buckets from the queue into local queue. We do this so we
        // hold the lock for the shortest amount of time. We don't want to execute
        // potentially slow drawing code with the lock held.
        //
        while (!maxRenderCtxt.Buckets.empty())
        {
            localBuckets.push(maxRenderCtxt.Buckets.front());
            maxRenderCtxt.Buckets.pop();
        }

        // No one could have added any more buckets to the queue until we leave the
        // CS. At this point, the queue must be empty, which agrees with the state
        // of the event. And the event only gets set after a successful add, but
        // the other thread can't add anything until I release the lock.
        //
        LeaveCriticalSection(&maxRenderCtxt.CritSec);

        // We could have a case where the queue is empty here, because after
        // we woke up from the event, but before we entered the critical section,
        // the display driver added a new bucket and set the event again.
        //
        // Then we process the bucket list until it's empty: so we have the event
        // still set, but the bucket list is empty. It just means we processed
        // the buckets last time around. Not a big deal.
        //
        if (!localBuckets.empty())
        {
            bucketsProcessed += processBuckets(
                &localBuckets,
                &maxRenderCtxt.DefunctBuckets,
                targetBmp);

            // Update progress UI.
            //
            int res = progressCb->Progress(bucketsProcessed, totalBucketEstimate);
            if (res == RENDPROG_ABORT)
            {
                maxRenderCtxt.ShouldAbort = true;
            }
        }

        // Wait for more buckets.
        //
    }

    // Driver told us it won't be sending any more buckets, which means we
    // shouldn't be waiting on the event anymore. However, check if there
    // is anything left on the queue to process.
    //
    EnterCriticalSection(&maxRenderCtxt.CritSec);
    
    if (!maxRenderCtxt.Buckets.empty())
    {
        // This time, we can use the queue directly, without copying locally,
        // since we're not contending with the display driver thread.
        //
        bucketsProcessed += processBuckets(
            &maxRenderCtxt.Buckets,
            &maxRenderCtxt.DefunctBuckets,
            targetBmp);
    }

    LeaveCriticalSection(&maxRenderCtxt.CritSec);

    // Update progress UI.
    // No need to check for aborts here, since the display driver has already
    // exited.
    //
    progressCb->Progress(bucketsProcessed, totalBucketEstimate);

    // Wait for worker to finish.
    //
    DWORD res = WaitForSingleObject(hThread, INFINITE);
    assert(res == WAIT_OBJECT_0);

    CloseHandle(hThread);
    CloseHandle(maxRenderCtxt.QueueNotEmpty);

    // Free all the buckets the display driver allocated.
    //
    Bucket* bucket = NULL;
    do
    {
        bucket = maxRenderCtxt.DefunctBuckets.front();
        maxRenderCtxt.DefunctBuckets.pop();

        FreeBucket(bucket);
    }
    while (!maxRenderCtxt.DefunctBuckets.empty());

    // Indicate success.
    //
    return TRUE;
}
//------------------------------------------------------------------------------
// Function: processBuckets
//
// Description:
//
//  Displays the list of buckets in 'queue' and retires them to 'defunctQueue'.
//
// Parameters:
//
// Returns:
//
//  Number of buckets displayed (and hence retired).
//
// Notes:
//
//  Does not perform synchronization.
//
static int
processBuckets(
    std::queue<Bucket*>* queue,
    std::queue<Bucket*>* defunctQueue,
    Bitmap* targetBmp)
{
    assert(!queue->empty());

    int cBuckets = 0;

    // Display the buckets we found.
    //
    Bucket* bucket = NULL;
    do
    {
        bucket = queue->front();
        queue->pop();

        // Display this bucket.
        //
        displayBucket(bucket, targetBmp);

        // Push it onto the defunct queue, so that it gets released later.
        //
        defunctQueue->push(bucket);

        ++cBuckets;
    }
    while (!queue->empty());

    return cBuckets;
}

static void
displayBucket(
    const Bucket* bucket,
    Bitmap* bm)
{
    const int numPixels = bucket->Width * bucket->Height;
    BMM_Color_fl* pixels = new BMM_Color_fl[numPixels];

    // Max is scanline-based, so we need to put pixels one scanline at a time.
    //
    for (int y = 0; y < bucket->Height; ++y)
    {
        // Fill in one scanline of the bucket into our local buffer.
        //
        for (int x = 0; x < bucket->Width; ++x)
        {
            // The pixels are layed out as follows:
            //
            // Suppose there are 3 channels, "rgba", then the data array will 
            // look like:
            //
            //  /-- px 0--\ /--px 1 --\     /--px N --\
            //  [ r g b a ] [ r g b a ] ... [ r g b a ]
            //

            const int baseSampleIdx = 
                // First, we seek to the row.
                //
                (y * bucket->Width * bucket->SamplesPerPixel) +

                // Then, we add the horizontal pixel offset.
                //
                (x * bucket->SamplesPerPixel);

            const int pixelIdx = x + y * bucket->Width;

            pixels[pixelIdx].r = bucket->Samples[baseSampleIdx + 0];
            pixels[pixelIdx].g = bucket->Samples[baseSampleIdx + 1];
            pixels[pixelIdx].b = bucket->Samples[baseSampleIdx + 2];
            pixels[pixelIdx].a = bucket->Samples[baseSampleIdx + 3];
        }

        // Send it off to Max.
        //
        bm->PutPixels(
            bucket->XPos,
            bucket->YPos + y,
            bucket->Width,
            pixels + y * bucket->Width);

        // Repeat for the next scanline.
        //
    }

    // Max makes a copy of our pixels, so we can free the buffer now.
    //
    delete [] pixels;

    // Update part of the window.
    //
    RECT rc = {0};
    rc.left = bucket->XPos;
    rc.right = bucket->XPos + bucket->Width;
    rc.top = bucket->YPos;
    rc.bottom = bucket->YPos + bucket->Height;
    bm->RefreshWindow(&rc);
}

unsigned WINAPI 
RendermanWorkerEntry(void* context)
{
    RendermanThreadContext* thdCtxt = (RendermanThreadContext*)context;
    // Initialize the renderer.
    //
    RiBegin(RI_NULL);

    // Use Pixie display driver for MaxtorNew.
    //
    RiDisplay(
        "MaxtorNew Render",
        "pixieDispDrv",
        "rgba",

        // Pass context structure to driver. "pointer" is an extension added
        // to Pixie.
        //
        "constant pointer " MAX_RENDER_CTXT_PARAM, thdCtxt->MaxContext,
        RI_NULL);

    RiWorldBegin();

    RiSphere(1, 0, 1, 360.0);

    RiWorldEnd();
    // Finalize the renderer.
    //
    RiEnd();

    return 0;
}