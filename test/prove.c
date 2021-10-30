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
#include "hitime_all.h"

#include <limits.h>
#include <stdlib.h>


uint64_t
rand64(void)
{
    uint32_t arr[2];
    arr[0] = (uint32_t)random();
    arr[1] = (uint32_t)random();
    return ((uint64_t)arr[0] << 32) ^ (uint64_t)arr[1];
}

static hitime_t _ht;
static hitime_t *ht = &_ht;

spec("hitime library")
{
    describe("hitimeout")
    {
        it("should create, set, reset, and free")
        {
            hitimeout_t *t = hitimeout_new();
            check(0 == hitimeout_when(t));
            check(NULL == hitimeout_data(t));
            check(0 == hitimeout_type(t));
            hitimeout_set(t, 1, (void *)1, 1);
            check(1 == hitimeout_when(t));
            check((void *)1 == hitimeout_data(t));
            check(1 == hitimeout_type(t));
            hitimeout_reset(t);
            check(0 == hitimeout_when(t));
            check(NULL == hitimeout_data(t));
            check(0 == hitimeout_type(t));
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
            check((uint64_t)INT_MAX == hitime_max_wait());
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
            hitimeout_set(t, 1, NULL, 1);

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

        it("should handle double start with no issue")
        {
            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 1, NULL, 1);

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

            hitimeout_set(t1, 20, NULL, 0);
            hitimeout_set(t2, 20, NULL, 0);

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

            hitimeout_set(t, 20, NULL, 0);

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
            hitimeout_set(t, 20, NULL, 0);
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

        it("should work with wait past recommended range (white-box)")
        {
            uint64_t max = hitime_max_wait();

            hitimeout_t *t = hitimeout_new();

            hitimeout_set(t, max + 1, NULL, 0);

            hitime_start(ht, t);

            check(max == (uint64_t)hitime_get_wait(ht));

            uint64_t now = 0;

            /* Using a for loop to avoid any infinite loop should the test fail.
             */
            int maxiter = 32;
            int i;
            bool done = false;
            for (i = 0; i < maxiter && !done; ++i)
            {
                now += hitime_get_wait(ht);
                done = hitime_timeout(ht, now);
            }

            check(done);
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            hitimeout_free(&t);
        }
        
        it("should bulk expire (code-coverage)")
        {
            hitimeout_t *t = hitimeout_new();

            hitimeout_set(t, 4, NULL, 0);

            hitime_start(ht, t);

            hitime_timeout(ht, 16);

            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            hitimeout_free(&t);
        }

        it("should get proper wait time for wait with (white-box)")
        {
            hitimeout_t *t = hitimeout_new();

            hitimeout_set(t, 4, NULL, 0);

            hitime_start(ht, t);

            hitime_timeout(ht, 1);

            int wait;
            wait = hitime_get_wait_with(ht, 2);
            check(2 == wait);
            wait = hitime_get_wait_with(ht, 4);
            check(0 == wait);

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
            hitimeout_set(t, 0x0F, NULL, 2);

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
                hitimeout_set(t, when, NULL, 0);
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
                hitimeout_set(t, when, NULL, 0);
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
                    hitimeout_set(t, when, NULL, 0);
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
            hitimeout_set(t, end, NULL, 0);

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

                hitimeout_set(t, hitimeout_time, NULL, 0);

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

                hitimeout_set(t, hitimeout_time, NULL, 0);

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

    describe("re-entrant timeout with state")
    {
        before_each()
        {
            hitime_init(ht);
        }

        after_each()
        {
            hitime_destroy(ht);
        }

        it("should switch to done with invalid state value (code-coverage)")
        {
            hitimestate_t state;
            state.state = 20;

            check(hitime_timeout_r(ht, &state, 0));
        }

        it("should switch to done with invalid time sequence (code-coverage)")
        {
            uint64_t now = 2;

            hitime_timeout(ht, now);

            now = 1;

            hitimestate_t state;
            hitimestate_init(&state, now);
            check(hitime_timeout_r(ht, &state, 0));
        }

        it("should move unfinished timeout update items to expiry")
        {
            hitime_timeout(ht, 1);

            hitimeout_t *t = hitimeout_new();
            hitimeout_set(t, 2, NULL, 0);
            hitime_start(ht, t);

            hitimestate_t state;
            hitimestate_init(&state, 2);
            check(!hitime_timeout_r(ht, &state, 0));

            hitime_expire_all(ht);

            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            hitimeout_free(&t);
        }
    }
}

