#pragma once

#include <windows.h>
#include <queue>

#define DLLEXPORT extern "C" __declspec( dllexport )
#define DLLIMPORT extern "C" __declspec( dllimport )

// If we're building the display driver, export these routines, else import them.
//
#ifdef BUILDING_DISPDRV
#define DISPDRVAPI DLLEXPORT
#else
#define DISPDRVAPI DLLIMPORT
#endif

#define MAX_RENDER_CTXT_PARAM "maxRenderContext"

struct Bucket
{
    int Width;
    int Height;
    int XPos;
    int YPos;
    int SamplesPerPixel;
    float* Samples;
};

DISPDRVAPI void FreeBucket(Bucket* bucket);

// This structure is shared between the Pixie display driver and 3ds max.
//
struct MaxRenderContext
{
    MaxRenderContext() :
        QueueNotEmpty(NULL),
        AllBucketsSent(false),
        ShouldAbort(false)
    {
        InitializeCriticalSection(&CritSec);
    }
    ~MaxRenderContext()
    {
        DeleteCriticalSection(&CritSec);
    }

    CRITICAL_SECTION CritSec;

    // QueueNotEmpty - signalled when the queue has buckets to consume.
    //
    HANDLE QueueNotEmpty;

    // Buckets - buckets that have yet to be drawn.
    //
    std::queue<Bucket*> Buckets;

    // DefunctBuckets - Buckets are moved onto this queue to be freed by the
    // display driver.
    //
    std::queue<Bucket*> DefunctBuckets;

    volatile bool AllBucketsSent;

    volatile bool ShouldAbort;
};
