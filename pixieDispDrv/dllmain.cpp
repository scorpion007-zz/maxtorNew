#include <windows.h>
#include <assert.h>
#include <dsply.h>  // Pixie display driver header.
#include "../common/common.h"

static HINSTANCE s_hInstance = NULL;
static const int NUM_SAMPLES = 4;

struct RenderContext
{
    int Width;
    int Height;
    int SamplesPerPixel;

    MaxRenderContext* MaxContext;
};

DISPDRVAPI void 
FreeBucket(Bucket* bucket)
{
    delete [] bucket->Samples;
    delete bucket;
}

BOOL WINAPI 
DllMain(
    HINSTANCE hinstDLL,
    ULONG fdwReason,
    LPVOID /*lpvReserved*/)
{
    if( fdwReason == DLL_PROCESS_ATTACH )
    {
        // Hang on to this DLL's instance handle.
        //
        s_hInstance = hinstDLL;

        // Don't call us with THREAD_ATTACH et al.
        //
        DisableThreadLibraryCalls(s_hInstance);
    }
    return TRUE;
}

DLLEXPORT void*
displayStart(
    const char * name,
    int          width,
    int          height,
    int          samplesPerPixel,
    const char * samples,
    TDisplayParameterFunction findParam)
{
    RenderContext* rc = new RenderContext;
    
    rc->Width = width;
    rc->Height = height;
    rc->SamplesPerPixel = samplesPerPixel;

    // TODO: Add more flexibility.
    //
    assert(samplesPerPixel == NUM_SAMPLES);

    MaxRenderContext* mrc = (MaxRenderContext*)findParam(
        MAX_RENDER_CTXT_PARAM,
        POINTER_PARAMETER,
        1);

    assert(mrc);

    rc->MaxContext = mrc;

    return rc;
}

// Called once per bucket.
//
DLLEXPORT int 
displayData(
    void  * context,
    int     bucketXPos,
    int     bucketYPos,
    int     bucketWidth,
    int     bucketHeight,
    float * data)
{
    RenderContext* rc = (RenderContext*)context;
    
    assert(rc->Width > 0);
    assert(rc->Height > 0);

    // This function fills a queue that will be consumed by the UI thread
    // in max.
    //
    Bucket* bucket = new Bucket;

    // Set bucket dimensions.
    //
    bucket->Width = bucketWidth;
    bucket->Height = bucketHeight;
    bucket->XPos = bucketXPos;
    bucket->YPos = bucketYPos;
    bucket->SamplesPerPixel = rc->SamplesPerPixel;

    // Allocate and fill our bucket pixel buffer.
    //
    const int totalSamples = rc->SamplesPerPixel * bucketWidth * bucketHeight;
    bucket->Samples = new float[totalSamples];

    memcpy(bucket->Samples, data, totalSamples * sizeof(float));

    EnterCriticalSection(&rc->MaxContext->CritSec);

    // Push the bucket onto the queue.
    //
    rc->MaxContext->Buckets.push(bucket);

    // Notify Max that there's something on the queue.
    //
    SetEvent(rc->MaxContext->QueueNotEmpty);

    LeaveCriticalSection(&rc->MaxContext->CritSec);

    // Support aborts.
    //
    return rc->MaxContext->ShouldAbort ? 0 : 1;
}


DLLEXPORT void 
displayFinish(
    void  * context)
{
    // Free all the buckets we allocated here after max is done.
    //
    RenderContext* rc = (RenderContext*)context;

    // Tell Max we're not going to be sending any more buckets.
    //
    rc->MaxContext->AllBucketsSent = true;

    // Wake up Max one final time. It will be able to distinguish this from
    // the regular wakeups because AllBucketsSent is true, which means we are
    // terminating.
    //
    SetEvent(rc->MaxContext->QueueNotEmpty);

    // Delete our private render context. This does not delete the shared
    // MaxRenderContext, which we don't own.
    //
    delete rc;
}