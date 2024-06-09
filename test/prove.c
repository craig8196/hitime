/*******************************************************************************
 * Copyright (c) 2021 Craig Jacobson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/
/**
 * @file prove.c
 * @author Craig Jacobson
 * @brief Demonstrate correctness of timeout manager.
 */

#include "bdd.h"
#include "hitime.h"
#include "hitime_extra.h"

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>


#ifndef FORCESEED
#define FORCESEED (0)
#endif


uint64_t
rand64(void)
{
    uint32_t arr[2];
    arr[0] = (uint32_t)random();
    arr[1] = (uint32_t)random();
    return ((uint64_t)arr[0] << 32) ^ (uint64_t)arr[1];
}

uint64_t
rand64_limited(void)
{
    return (uint32_t)random() & (uint32_t)0x7FFFFFFF;
}

static int
_sort_timeouts(const void *_a, const void *_b)
{
    hitimeout_t *a = *(hitimeout_t **)_a;
    hitimeout_t *b = *(hitimeout_t **)_b;

    if (a->when < b->when) { return -1; }
    else if (a->when > b->when) { return 1; }
    else if (a->data < b->data) { return -1; }
    else if (a->data > b->data) { return 1; }
    else { return 0; }
}

void
sort_timeouts(hitimeout_t **t, int len)
{
    qsort(t, len, sizeof(*t), _sort_timeouts);
}

static hitime_t _ht;
static hitime_t *ht = &_ht;

static const int TSLEN = 128;
static hitimeout_t *ts = NULL;
static hitimeout_t **tss = NULL;

static int randseed = 0;

spec("hitime library")
{
    describe("hitimeout")
    {
        it("should create, set, reset, and free")
        {
            hitimeout_t *t = hitimeout_new();
            check(0 == hitimeout_when(t));
            check(NULL == hitimeout_data(t));
            hitimeout_set(t, 1, (void *)1);
            check(1 == hitimeout_when(t));
            check((void *)1 == hitimeout_data(t));
            hitimeout_reset(t);
            check(0 == hitimeout_when(t));
            check(NULL == hitimeout_data(t));
            hitimeout_free(&t);
            check(NULL == t);
        }
    }

    describe("make and cleanup")
    {
        it("should allocate and free")
        {
            hitime_t *ht = hitime_new();
            check(NULL != ht);
            hitime_free(&ht);
            check(NULL == ht);
        }
    }

    describe("basic tests")
    {
        before_each()
        {
            hitime_init(ht);
        }

        after_each()
        {
            hitime_destroy(ht);
        }

        it("should return NULL when none are expired")
        {
            check(NULL == hitime_get_next(ht));
        }

        it("should return max wait (white-box)")
        {
            check(UINT64_MAX == hitime_max_wait());
        }

        it("should return max wait when no hitimeouts (white-box)")
        {
            check(hitime_max_wait() == hitime_get_wait(ht));
        }

        it("should do nothing on expire all")
        {
            hitime_expire_all(ht);
            check(NULL == hitime_get_next(ht));
        }

        it("should do nothing on hitimeout while empty")
        {
            hitime_timeout(ht, 1);
            check(NULL == hitime_get_next(ht));
        }

        it("should start and be placed in expiry")
        {
            hitimeout_t *t = hitimeout_new();
            hitime_start(ht, t);
            check(hitime_max_wait() == hitime_get_wait(ht));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            hitimeout_free(&t);
        }

        it("should start and stop")
        {
            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 1, NULL);

            hitime_start(ht, t);

            /* Check waits are correct. */
            check(1 == hitime_get_wait(ht));
            check(NULL == hitime_get_next(ht));

            /* Check that we have an expired item on hitimeout. */
            check(hitime_timeout(ht, 1));

            /* Check outgoing. */
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));

            hitimeout_free(&t);
        }

        it("updates the timeout")
        {
            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 5, NULL);

            hitime_start(ht, t);
            check(!hitime_timeout(ht, 4));
            hitime_touch(ht, t, 6);
            check(NULL == hitime_get_next(ht));
            check(!hitime_timeout(ht, 5));
            check(hitime_timeout(ht, 6));
            check(t == hitime_get_next(ht));

            hitimeout_free(&t);
        }

        it("updates the timeout when expired")
        {
            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 5, NULL);

            hitime_start(ht, t);
            check(hitime_timeout(ht, 5));
            hitime_touch(ht, t, 6);
            check(NULL == hitime_get_next(ht));
            check(hitime_timeout(ht, 6));
            check(t == hitime_get_next(ht));

            hitimeout_free(&t);
        }

        it("places timeout into expiry when updated with expired timestamp")
        {
            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 5, NULL);

            hitime_start(ht, t);
            check(!hitime_timeout(ht, 4));
            hitime_touch(ht, t, 4);
            check(t == hitime_get_next(ht));

            hitimeout_free(&t);
        }

        it("should handle double start with no issue")
        {
            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 1, NULL);

            hitime_start(ht, t);
            hitime_start(ht, t);

            check(1 == hitime_get_wait(ht));
            check(hitime_timeout(ht, 1));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));
            hitimeout_free(&t);
        }

        it("should expire all (white-box, order of expiry known)")
        {
            hitimeout_t *t1 = hitimeout_new();
            hitimeout_t *t2 = hitimeout_new();

            hitimeout_set(t1, 20, NULL);
            hitimeout_set(t2, 20, NULL);

            hitime_start(ht, t1);
            hitime_start(ht, t2);

            hitime_expire_all(ht);

            check(t1 == hitime_get_next(ht));
            check(t2 == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            hitimeout_free(&t1);
            hitimeout_free(&t2);
        }

        it("should remove the hitimeout from the datastructure")
        {
            hitimeout_t *t = hitimeout_new();

            hitimeout_set(t, 20, NULL);

            hitime_start(ht, t);
            hitime_stop(ht, t);

            check(!hitime_timeout(ht, 30));

            check(NULL == hitime_get_next(ht));

            hitimeout_free(&t);
        }

        it("should stop an expired hitimeout")
        {
            hitimeout_t *t = hitimeout_new();

            check(!hitime_timeout(ht, 30));
            /* Timeout should be immediately expired. */
            hitimeout_set(t, 20, NULL);
            hitime_start(ht, t);
            hitime_stop(ht, t);
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));

            hitimeout_free(&t);
        }

        it("should update the internal time stamp")
        {
            check(!hitime_timeout(ht, 30));
            check(30 == hitime_get_last(ht));
        }
        
        it("should bulk expire (code-coverage)")
        {
            hitimeout_t *t = hitimeout_new();

            hitimeout_set(t, 4, NULL);

            hitime_start(ht, t);

            hitime_timeout(ht, 16);

            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            hitimeout_free(&t);
        }

        it("should get proper wait time for get_wait_with (white-box)")
        {
            hitimeout_t *t = hitimeout_new();

            hitimeout_set(t, 4, NULL);

            hitime_start(ht, t);

            hitime_timeout(ht, 1);

            int wait;
            wait = hitime_get_wait_with(ht, 2);
            check(2 == wait);
            wait = hitime_get_wait_with(ht, 4);
            check(0 == wait);

            hitimeout_free(&t);
        }

        it("starts a timer in the given range")
        {
            hitimeout_t *t = hitimeout_new();

            hitime_start_range(ht, t, 0, 1);
            check(NULL == hitime_get_next(ht));
            hitime_timeout(ht, 1);
            check(t == hitime_get_next(ht));

            hitime_start_range(ht, t, 0x0F, 0x10);
            check(NULL == hitime_get_next(ht));
            hitime_timeout(ht, 0x0F);
            check(NULL == hitime_get_next(ht));
            hitime_timeout(ht, 0x10);
            check(t == hitime_get_next(ht));

            hitimeout_free(&t);
        }
    }

    describe("intermediate tests")
    {
        before_each()
        {
            hitime_init(ht);
        }

        after_each()
        {
            hitime_destroy(ht);
        }

        it("should bubble up hitimeout (white-box)")
        {
            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 0x0F, NULL);

            hitime_start(ht, t);

            uint64_t now = 0;

            int wait;

            wait = hitime_get_wait(ht);
            int expected = 0x08;
            check(expected == wait, "wait was %d, expected %d", wait, expected);
            now += wait;
            check(!hitime_timeout(ht, now));
            check(NULL == hitime_get_next(ht));

            wait = hitime_get_wait(ht);
            expected = 0x04;
            check(expected == wait, "wait was %d, expected %d", wait, expected);
            now += wait;
            check(!hitime_timeout(ht, now));
            check(NULL == hitime_get_next(ht));

            wait = hitime_get_wait(ht);
            expected = 0x02;
            check(expected == wait, "wait was %d, expected %d", wait, expected);
            now += wait;
            check(!hitime_timeout(ht, now));
            check(NULL == hitime_get_next(ht));

            wait = hitime_get_wait(ht);
            expected = 0x01;
            check(expected == wait, "wait was %d, expected %d", wait, expected);
            now += wait;
            check(hitime_timeout(ht, now));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));

            hitimeout_free(&t);
        }

        it("should expire hitimeouts in order when added in order (white-box)")
        {
            int low = 0x001;
            int high = 0x0FF;
            int count = (high - low) + 1;
            int i;
            for (i = 0; i < count; ++i)
            {
                uint64_t when = low + i;
                hitimeout_t *t = hitimeout_new();
                hitimeout_set(t, when, NULL);
                hitime_start(ht, t);
            }

            uint64_t now = 0;
            int wait;
            while (hitime_max_wait() != (wait = hitime_get_wait(ht)))
            {
                now += wait;
                /* The wait should be 1 because the hitimeouts are evenly
                 * spaced.
                 */
                check(1 == wait);
                check(hitime_timeout(ht, now));
            }

            for (i = 0; i < count; ++i)
            {
                uint64_t when = low + i;
                hitimeout_t *t = hitime_get_next(ht);
                check(t);
                check(when == hitimeout_when(t));
                hitimeout_free(&t);
            }
        }

        it("should expire hitimeouts in order when added in reverse order (white-box)")
        {
            int low = 0x001;
            int high = 0x0FF;
            int count = (high - low) + 1;
            int i;
            for (i = count; i > 0; --i)
            {
                uint64_t when = low + (i - 1);
                hitimeout_t *t = hitimeout_new();
                hitimeout_set(t, when, NULL);
                hitime_start(ht, t);
            }

            uint64_t now = 0;
            int wait;
            while (hitime_max_wait() != (wait = hitime_get_wait(ht)))
            {
                now += wait;
                /* The wait should be 1 because the hitimeouts are evenly
                 * spaced.
                 */
                check(1 == wait);
                check(hitime_timeout(ht, now));
            }

            for (i = 0; i < count; ++i)
            {
                uint64_t when = low + i;
                hitimeout_t *t = hitime_get_next(ht);
                check(t);
                check(when == hitimeout_when(t));
                hitimeout_free(&t);
            }
        }

        it("should expire hitimeouts in order when the start time varies (white-box)")
        {
            uint64_t startnow = 0x001;
            uint64_t endnow = 0x0FF;
            uint64_t countnow = (endnow - startnow) + 1;
            uint64_t i;

            for (i = startnow; i < countnow; ++i)
            {
                uint64_t now = i;
                uint64_t seednow = now;

                hitime_init(ht);
                hitime_timeout(ht, seednow);

                int low = 0x001;
                int high = 0x0FF;
                int count = (high - low) + 1;
                int i;
                for (i = count; i > 0; --i)
                {
                    uint64_t when = low + (i - 1) + seednow;
                    hitimeout_t *t = hitimeout_new();
                    hitimeout_set(t, when, NULL);
                    hitime_start(ht, t);
                }

                int wait;
                while (hitime_max_wait() != (wait = hitime_get_wait(ht)))
                {
                    now += wait;
                    /* The wait should be 1 because the hitimeouts are evenly
                     * spaced.
                     */
                    check(1 == wait);
                    check(hitime_timeout(ht, now));
                }

                for (i = 0; i < count; ++i)
                {
                    uint64_t when = low + i + seednow;
                    hitimeout_t *t = hitime_get_next(ht);
                    check(t);
                    check(when == hitimeout_when(t));
                    hitimeout_free(&t);
                }

                hitime_destroy(ht);
            }
        }

        it("should expire even largest of hitimeouts")
        {
            uint64_t end = (uint64_t)0xFFFFFFFFFFFFFFFFLL;

            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, end, NULL);

            hitime_start(ht, t);
            check(hitime_timeout(ht, end));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            hitimeout_free(&t);
        }
    }

    describe("advanced tests")
    {
        it("should expire properly at or after random timestamp")
        {
            int maxiter = 1000;
            hitimeout_t *t = hitimeout_new();

            int i;
            for (i = 0; i < maxiter; ++i)
            {
                uint64_t start_time = rand64();
                uint64_t hitimeout_time = rand64();
                hitimeout_time = hitimeout_time ? hitimeout_time : 1;
                uint64_t over_time = rand64() & 0x00FFFFFF;
                over_time = over_time + hitimeout_time;

                hitime_init(ht);
                hitime_timeout(ht, start_time);

                hitimeout_set(t, hitimeout_time, NULL);

                hitime_start(ht, t);

                if (hitimeout_time <= start_time)
                {
                    check(t == hitime_get_next(ht));
                    check(NULL == hitime_get_next(ht));
                }
                else
                {
                    check(!hitime_timeout(ht, hitimeout_time - 1));
                    check(hitime_timeout(ht, hitimeout_time));
                    check(t == hitime_get_next(ht));
                    check(NULL == hitime_get_next(ht));
                }

                hitime_destroy(ht);
                hitime_init(ht);

                hitimeout_set(t, hitimeout_time, NULL);

                hitime_start(ht, t);

                if (over_time >= hitimeout_time)
                {
                    check(hitime_timeout(ht, over_time));
                    check(t == hitime_get_next(ht));
                    check(NULL == hitime_get_next(ht));
                }

                hitime_destroy(ht);
            }

            hitimeout_free(&t);
        }
    }

    describe("getting time")
    {
        it("should get the current time in seconds")
        {
            check(hitime_now());
        }

        it("should get the current time in milliseconds seconds")
        {
            check(hitime_now_ms());
        }
    }

    describe("randomized tests")
    {
        before_each()
        {
            randseed = FORCESEED;
            if (!randseed)
            {
                randseed = (int)time(0);
            }
            srand(randseed);

            hitime_init(ht);
            uint64_t now = rand64_limited();
            hitime_timeout(ht, now);

            // Create the list of random timeouts
            ts = calloc(TSLEN, sizeof(hitimeout_t));
            tss = calloc(TSLEN, sizeof(hitimeout_t *));
            int i;
            for (i = 0; i < TSLEN; ++i)
            {
                ts[i].when = now + rand64_limited();
                ts[i].data = (void *)(intptr_t)i;
                tss[i] = ts + i;
            }
        }

        after_each()
        {
            hitime_destroy(ht);
            free(ts);
            free(tss);
            ts = NULL;
            tss = NULL;
        }

        it("should keep the order of timeouts when following the recommended timeout")
        {
            hitimeout_t *t;
            int count;

            // Set now to a random value
            // Create list of timeouts less than ~2 billion time units in the future

            // Add all of the timeouts to the manager
            int i;
            for (i = 0; i < TSLEN; ++i)
            {
                hitime_start(ht, tss[i]);
            }
            count = hitime_count_all(ht);
            check(TSLEN == count, "Start Count - Actual: %d, Expected: %d", count, TSLEN);

            // Follow the recommended timeout interval
            int max_calls = TSLEN * 64;
            i = 0;
            uint64_t max = hitime_max_wait();
            uint64_t wait = hitime_get_wait(ht);
            //printf("Max: %lu, %lx\n", max, max);
            while (i < max_calls && wait < max)
            {
                hitime_timedelta(ht, wait);
                ++i;
                wait = hitime_get_wait(ht);
            }
            check(i < max_calls, "Too many iterations; Iterations: %d, Max: %d", i, max_calls);
            count = hitime_count_expired(ht);
            check(TSLEN == count, "Expired Count - Actual: %d, Expected: %d", count, TSLEN);

            // Sort timeouts, check that the last timeout is the last time
            // WARN: Stable sort required!
            sort_timeouts(tss, TSLEN);
            t = tss[0];
            //printf("Min: %lu, %lx\n", t->when, t->when);
            t = tss[TSLEN - 1];
            //printf("Max: %lu, %lx\n", t->when, t->when);
            check(hitime_get_last(ht) == t->when);

            // Iterate through the expiries and assert that they are in the same order
            i = 0;
            for (i = 0; i < TSLEN; ++i)
            {
                t = hitime_get_next(ht);
                check(NULL != t);
                check(t == (tss[i]), "INDEX: %d, SEED: %d", i, randseed);
            }
            check(NULL == hitime_get_next(ht));
        }

        it("should timeout values correctly given reasonable increments")
        {
            hitimeout_t *t, *actual, *expected;
            int count;

            // Set now to a random value
            // Create list of timeouts less than ~2 billion time units in the future

            // Add all of the timeouts to the manager
            int i;
            for (i = 0; i < TSLEN; ++i)
            {
                hitime_start(ht, tss[i]);
            }
            count = hitime_count_all(ht);
            check(TSLEN == count, "Start Count - Actual: %d, Expected: %d", count, TSLEN);

            // Create a reasonable interval
            sort_timeouts(tss, TSLEN);
            t = tss[TSLEN - 1];
            uint64_t interval = (t->when - tss[0]->when) / 10;
            if (!interval) { interval = 1; }
            hitimeout_t **actuals = calloc(sizeof(hitimeout_t *), TSLEN);
            int actuals_len = 0;
            int expected_index = 0;

            // Timeout every set interval and check that the items timeout once and correctly
            int interval_count = 0;
            int timed_out = 0;
            for (; interval_count < 12; ++interval_count)
            {
                actuals_len = 0;

                // Record the timeouts that actually timed out
                hitime_timedelta(ht, interval);
                while (NULL != (t = hitime_get_next(ht)))
                {
                    actuals[actuals_len++] = t;
                    ++timed_out;
                }

                // Sort them and compare against the actuals
                sort_timeouts(actuals, actuals_len);
                i = 0;
                for (i = 0; i < actuals_len; ++i)
                {
                    expected = tss[expected_index];
                    actual = actuals[i];

                    check(actual == expected);

                    ++expected_index;
                }
            }

            // Verify that all timeouts where hit
            check(timed_out == TSLEN);

            free(actuals);
        }

        it("should timeout values correctly given monotonicity isn\'t violated")
        {
            hitimeout_t *t;

            // Set now to a random value
            // Create list of timeouts less than 2 billion time units in the future
            uint64_t now = hitime_get_last(ht);

            // Timeout one unit prior, verify no timeout occurred, timeout one unit next, and verify timeout
            int i;
            for (i = 0; i < TSLEN; ++i)
            {
                t = tss[i];
                hitime_start(ht, t);
                hitime_timeout(ht, t->when - 1);
                check(hitime_count_expired(ht) == 0);
                hitime_timedelta(ht, 1);
                check(hitime_count_expired(ht) == 1);
                hitime_destroy(ht);
                hitime_init(ht);
                hitime_timeout(ht, now);
            }
        }

        it("should place timeouts into the correct bins the correct number of times until expiry (white-box)")
        {
            hitimeout_t *t;

            // For a set amount of iterations, do the following:
            // Reset everything

            // Set now to a random value
            uint64_t now = hitime_get_last(ht);

            int i;
            for (i = 0; i < TSLEN; ++i)
            {
                // Create a timeout value randomly
                t = tss[i];

                // Get a bit vector from now
                uint64_t bits = t->when ^ now;

                // Add the timeout
                hitime_start(ht, t);

                int bit_index = 63 - __builtin_clzll(bits);

                bool is_first_iter = true;

                //printf("Index: %d, Now: %lx, Timeout: %lx\n", i, now, t->when);
                // Check that it is in the correct bin at each timeout until expiry
                while (bits)
                {
                    check(hitime_count_bin(ht, bit_index) == 1,
                          "Bit check failed; Index: %d, Bits: %lx, Bit Index: %d", i, bits, bit_index);

                    // Advance timeout.
                    hitime_timedelta(ht, hitime_get_wait(ht));

                    // All "now" bits to the right of the triggered will be zero.
                    if (is_first_iter)
                    {
                        bits = hitime_get_last(ht) ^ t->when;
                        is_first_iter = false;
                    }

                    // Clear bit
                    bits = bits & ~(((uint64_t)1) << bit_index);
                    bit_index = 63 - __builtin_clzll(bits);
                }
                //check(false);
                check(hitime_count_expired(ht) == 1);

                hitime_destroy(ht);
                hitime_init(ht);
                hitime_timeout(ht, now);
            }
        }
    }
}

