#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hitime_all.h"

#ifndef FORCESEED
#define FORCESEED (0)
#endif

#ifndef MAXITER
#define MAXITER (2)
#endif

#ifndef MAXLEN
#define MAXLEN (1024*1024 * 256)
#endif


typedef struct
{
    struct timespec start;
    struct timespec end;
} stopwatch_t;

void
stopwatch_reset(stopwatch_t *sw)
{
    sw->start = (struct timespec){ 0 };
    sw->end = (struct timespec){ 0 };
}

void
stopwatch_start(stopwatch_t *sw)
{
    if (clock_gettime(CLOCK_REALTIME, &sw->start))
    {
        printf("Error getting start time: %d, %s\n", errno, strerror(errno));
        abort();
    }
}

void
stopwatch_stop(stopwatch_t *sw)
{
    if (clock_gettime(CLOCK_REALTIME, &sw->end))
    {
        printf("Error getting end time: %d, %s\n", errno, strerror(errno));
        abort();
    }
}

double
stopwatch_elapsed(stopwatch_t *sw)
{
    double seconds =
        (double)(sw->end.tv_sec - sw->start.tv_sec)
        + ((double)sw->end.tv_nsec - (double)sw->start.tv_nsec)/1000000000.0;
    return seconds;
}

uint64_t
rand64(void)
{
    uint32_t arr[2];
    arr[0] = (uint32_t)random();
    arr[1] = (uint32_t)random();
    return ((uint64_t)arr[0] << 32) ^ (uint64_t)arr[1];
}

int
main(void)
{
    int seed = FORCESEED;
    const int maxiter = MAXITER;
    const int maxlen = MAXLEN;
    stopwatch_t sw;

    if (!seed)
    {
        seed = (int)time(0);
    }

    printf("Seed: %d\n", seed);
    srand(seed);

    int iter = 0;
    for (iter = 0; iter < maxiter; ++iter)
    {
        stopwatch_reset(&sw);

        hitime_t ht;
        hitime_init(&ht);

        hitimeout_t *tos = malloc(maxlen * sizeof(hitimeout_t));

        // Initialize data
        int toindex = 0;
        for (toindex = 0; toindex < maxlen; ++toindex)
        {
            hitimeout_init(tos + toindex);
            hitimeout_set(tos + toindex, rand64(), NULL, 0);
        }

        // Time starts
        stopwatch_start(&sw);

        // Start all timeouts
        for (toindex = 0; toindex < maxlen; ++toindex)
        {
            hitime_start(&ht, tos + toindex);
        }

        // Time stops
        stopwatch_stop(&sw);

        // Print stats
        printf("START STATS\n");
        printf("Iteration: %d (of %d)\n", iter, maxiter);
        double seconds = stopwatch_elapsed(&sw);
        printf("Seconds: %f\n", seconds);
        double ops_per_second = (double)maxlen / seconds;
        printf("Ops/second: %f\n", ops_per_second);

        // Reset and start stopwatch
        stopwatch_reset(&sw);
        stopwatch_start(&sw);

        // Stop all timeouts
        for (toindex = 0; toindex < maxlen; ++toindex)
        {
            hitime_stop(&ht, tos + toindex);
        }

        // Time stops
        stopwatch_stop(&sw);

        // Print stats
        printf("STOP STATS\n");
        printf("Iteration: %d (of %d)\n", iter, maxiter);
        seconds = stopwatch_elapsed(&sw);
        printf("Seconds: %f\n", seconds);
        ops_per_second = (double)maxlen / seconds;
        printf("Ops/second: %f\n", ops_per_second);

        // Destroy data
        for (toindex = 0; toindex < maxlen; ++toindex)
        {
            hitimeout_destroy(tos + toindex);
        }
        free(tos);
    }

    return 0;
}
