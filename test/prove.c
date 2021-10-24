
#include "bdd.h"
#include "hitime.h"

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
    describe("timeout")
    {
        it("should create, set, reset, and free")
        {
            timeout_t *t = timeout_new();
            check(0 == timeout_when(t));
            check(NULL == timeout_data(t));
            check(0 == timeout_type(t));
            timeout_set(t, 1, (void *)1, 1);
            check(1 == timeout_when(t));
            check((void *)1 == timeout_data(t));
            check(1 == timeout_type(t));
            timeout_reset(t);
            check(0 == timeout_when(t));
            check(NULL == timeout_data(t));
            check(0 == timeout_type(t));
            timeout_free(&t);
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

        it("should return max wait when no timeouts (white-box)")
        {
            check(hitime_max_wait() == hitime_get_wait(ht));
        }

        it("should do nothing on expire all")
        {
            hitime_expire_all(ht);
            check(NULL == hitime_get_next(ht));
        }

        it("should do nothing on timeout while empty")
        {
            hitime_timeout(ht, 1);
            check(NULL == hitime_get_next(ht));
        }

        it("should start and be placed in expiry")
        {
            timeout_t *t = timeout_new();
            hitime_start(ht, t);
            check(hitime_max_wait() == hitime_get_wait(ht));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            timeout_free(&t);
        }

        it("should start and stop")
        {
            timeout_t *t = timeout_new();
            timeout_set(t, 1, NULL, 1);

            hitime_start(ht, t);

            /* Check waits are correct. */
            check(1 == hitime_get_wait(ht));
            check(NULL == hitime_get_next(ht));

            /* Check that we have an expired item on timeout. */
            check(hitime_timeout(ht, 1));

            /* Check outgoing. */
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));

            timeout_free(&t);
        }

        it("should handle double start with no issue")
        {
            timeout_t *t = timeout_new();
            timeout_set(t, 1, NULL, 1);

            hitime_start(ht, t);
            hitime_start(ht, t);

            check(1 == hitime_get_wait(ht));
            check(hitime_timeout(ht, 1));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));
            timeout_free(&t);
        }

        it("should expire all (white-box, order of expiry known)")
        {
            timeout_t *t1 = timeout_new();
            timeout_t *t2 = timeout_new();

            timeout_set(t1, 20, NULL, 0);
            timeout_set(t2, 20, NULL, 0);

            hitime_start(ht, t1);
            hitime_start(ht, t2);

            hitime_expire_all(ht);

            check(t1 == hitime_get_next(ht));
            check(t2 == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            timeout_free(&t1);
            timeout_free(&t2);
        }

        it("should remove the timeout from the datastructure")
        {
            timeout_t *t = timeout_new();

            timeout_set(t, 20, NULL, 0);

            hitime_start(ht, t);
            hitime_stop(ht, t);

            check(!hitime_timeout(ht, 30));

            check(NULL == hitime_get_next(ht));

            timeout_free(&t);
        }

        it("should stop an expired timeout")
        {
            timeout_t *t = timeout_new();

            check(!hitime_timeout(ht, 30));
            /* Timeout should be immediately expired. */
            timeout_set(t, 20, NULL, 0);
            hitime_start(ht, t);
            hitime_stop(ht, t);
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));

            timeout_free(&t);
        }

        it("should update the internal time stamp")
        {
            check(!hitime_timeout(ht, 30));
            check(30 == hitime_get_last(ht));
        }

        it("should work with wait past recommended range (white-box)")
        {
            uint64_t max = hitime_max_wait();

            timeout_t *t = timeout_new();

            timeout_set(t, max + 1, NULL, 0);

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

            timeout_free(&t);
        }
        
        it("should bulk expire (code-coverage)")
        {
            timeout_t *t = timeout_new();

            timeout_set(t, 4, NULL, 0);

            hitime_start(ht, t);

            hitime_timeout(ht, 16);

            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            timeout_free(&t);
        }

        it("should get proper wait time for wait with (white-box)")
        {
            timeout_t *t = timeout_new();

            timeout_set(t, 4, NULL, 0);

            hitime_start(ht, t);

            hitime_timeout(ht, 1);

            int wait;
            wait = hitime_get_wait_with(ht, 2);
            check(2 == wait);
            wait = hitime_get_wait_with(ht, 4);
            check(0 == wait);

            timeout_free(&t);
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

        it("should bubble up timeout (white-box)")
        {
            timeout_t *t = timeout_new();
            timeout_set(t, 0x0F, NULL, 2);

            hitime_start(ht, t);

            uint64_t now = 0;

            int wait;

            wait = hitime_get_wait(ht);
            check(0x08 == wait, "wait was %d", wait);
            now += wait;
            check(!hitime_timeout(ht, now));
            check(NULL == hitime_get_next(ht));

            wait = hitime_get_wait(ht);
            check(0x04 == wait, "wait was %d", wait);
            now += wait;
            check(!hitime_timeout(ht, now));
            check(NULL == hitime_get_next(ht));

            wait = hitime_get_wait(ht);
            check(0x02 == wait, "wait was %d", wait);
            now += wait;
            check(!hitime_timeout(ht, now));
            check(NULL == hitime_get_next(ht));

            wait = hitime_get_wait(ht);
            check(0x01 == wait, "wait was %d", wait);
            now += wait;
            check(hitime_timeout(ht, now));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));
            check(hitime_max_wait() == hitime_get_wait(ht));

            timeout_free(&t);
        }

        it("should expire timeouts in order when added in order (white-box)")
        {
            int low = 0x001;
            int high = 0x0FF;
            int count = (high - low) + 1;
            int i;
            for (i = 0; i < count; ++i)
            {
                uint64_t when = low + i;
                timeout_t *t = timeout_new();
                timeout_set(t, when, NULL, 0);
                hitime_start(ht, t);
            }

            uint64_t now = 0;
            int wait;
            while (hitime_max_wait() != (wait = hitime_get_wait(ht)))
            {
                now += wait;
                /* The wait should be 1 because the timeouts are evenly
                 * spaced.
                 */
                check(1 == wait);
                check(hitime_timeout(ht, now));
            }

            for (i = 0; i < count; ++i)
            {
                uint64_t when = low + i;
                timeout_t *t = hitime_get_next(ht);
                check(t);
                check(when == timeout_when(t));
                timeout_free(&t);
            }
        }

        it("should expire timeouts in order when added in reverse order (white-box)")
        {
            int low = 0x001;
            int high = 0x0FF;
            int count = (high - low) + 1;
            int i;
            for (i = count; i > 0; --i)
            {
                uint64_t when = low + (i - 1);
                timeout_t *t = timeout_new();
                timeout_set(t, when, NULL, 0);
                hitime_start(ht, t);
            }

            uint64_t now = 0;
            int wait;
            while (hitime_max_wait() != (wait = hitime_get_wait(ht)))
            {
                now += wait;
                /* The wait should be 1 because the timeouts are evenly
                 * spaced.
                 */
                check(1 == wait);
                check(hitime_timeout(ht, now));
            }

            for (i = 0; i < count; ++i)
            {
                uint64_t when = low + i;
                timeout_t *t = hitime_get_next(ht);
                check(t);
                check(when == timeout_when(t));
                timeout_free(&t);
            }
        }

        it("should expire timeouts in order when the start time varies (white-box)")
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
                    timeout_t *t = timeout_new();
                    timeout_set(t, when, NULL, 0);
                    hitime_start(ht, t);
                }

                int wait;
                while (hitime_max_wait() != (wait = hitime_get_wait(ht)))
                {
                    now += wait;
                    /* The wait should be 1 because the timeouts are evenly
                     * spaced.
                     */
                    check(1 == wait);
                    check(hitime_timeout(ht, now));
                }

                for (i = 0; i < count; ++i)
                {
                    uint64_t when = low + i + seednow;
                    timeout_t *t = hitime_get_next(ht);
                    check(t);
                    check(when == timeout_when(t));
                    timeout_free(&t);
                }

                hitime_destroy(ht);
            }
        }

        it("should expire even largest of timeouts")
        {
            uint64_t end = (uint64_t)0xFFFFFFFFFFFFFFFFLL;

            timeout_t *t = timeout_new();
            timeout_set(t, end, NULL, 0);

            hitime_start(ht, t);
            check(hitime_timeout(ht, end));
            check(t == hitime_get_next(ht));
            check(NULL == hitime_get_next(ht));

            timeout_free(&t);
        }
    }

    describe("advanced tests")
    {
        it("should expire properly at or after random timestamp")
        {
            int maxiter = 1000;
            timeout_t *t = timeout_new();

            int i;
            for (i = 0; i < maxiter; ++i)
            {
                uint64_t start_time = rand64();
                uint64_t timeout_time = rand64();
                timeout_time = timeout_time ? timeout_time : 1;
                uint64_t over_time = rand64() & 0x00FFFFFF;
                over_time = over_time + timeout_time;

                hitime_init(ht);
                hitime_timeout(ht, start_time);

                timeout_set(t, timeout_time, NULL, 0);

                hitime_start(ht, t);

                if (timeout_time <= start_time)
                {
                    check(t == hitime_get_next(ht));
                    check(NULL == hitime_get_next(ht));
                }
                else
                {
                    check(!hitime_timeout(ht, timeout_time - 1));
                    check(hitime_timeout(ht, timeout_time));
                    check(t == hitime_get_next(ht));
                    check(NULL == hitime_get_next(ht));
                }

                hitime_destroy(ht);
                hitime_init(ht);

                timeout_set(t, timeout_time, NULL, 0);

                hitime_start(ht, t);

                if (over_time >= timeout_time)
                {
                    check(hitime_timeout(ht, over_time));
                    check(t == hitime_get_next(ht));
                    check(NULL == hitime_get_next(ht));
                }

                hitime_destroy(ht);
            }

            timeout_free(&t);
        }
    }
}

