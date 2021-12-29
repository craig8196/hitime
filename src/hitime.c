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
 * @file hitime.c
 * @author Craig Jacobson
 * @brief Hierarchical hitimeout manager implementation.
 */

#include "hitime_util.h"
#include "hitime.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


/*******************************************************************************
 * TIMEOUT FUNCTIONS
*******************************************************************************/

void
hitimeout_init(hitimeout_t *t)
{
    hitime_memzero(t, sizeof(*t));
}

void
hitimeout_reset(hitimeout_t *t)
{
    hitimeout_init(t);
}

void
hitimeout_destroy(hitimeout_t * UNUSED(t))
{
}

void
hitimeout_set(hitimeout_t *t, uint64_t when, void *data, int type)
{
    t->when = when;
    t->data = data;
    t->type = type;
}

uint64_t
hitimeout_when(hitimeout_t *t)
{
    return t->when;
}

void *
hitimeout_data(hitimeout_t *t)
{
    return t->data;
}

int
hitimeout_type(hitimeout_t *t)
{
    return t->type;
}


/*******************************************************************************
 * HELPER FUNCTIONS
*******************************************************************************/

static uint64_t WAITMAX = INT_MAX;
static uint64_t DELTMAX = INT_MAX;
static uint32_t UPPERBIT = 0x80000000;
static int EXPIRYINDEX = 32;
static int PROCESSINDEX = 33;
static int MAXINDEX = 31;

#if !(defined __GNUC__ && WORD_BIT == 32)
INLINE static int
_pop32(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}
#endif

INLINE static int
is_expired(hitime_t *h, hitimeout_t *t)
{
    return (t->when <= h->last);
}

/**
 * @warn Do NOT call with zero.
 */
INLINE static int
get_low_index(uint32_t n)
{
#if defined __GNU__ && WORD_BIT == 32
    return __builtin_ctz(n);
#else
    uint32_t m = n ^ (n - 1);
    return _pop32(m) - 1;
#endif
}

/**
 * @warn Do NOT call with zero.
 */
INLINE static int
get_high_index(uint32_t n)
{
#if defined __GNU__ && WORD_BIT == 32
    return 31 - __builtin_clz(n);
#else
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return _pop32(n) - 1;
#endif
}

INLINE static uint32_t
get_elpased(uint64_t now, uint64_t last)
{
    uint64_t d = now - last;
    return d > DELTMAX ? UPPERBIT : (uint32_t)d;
}

INLINE static hitime_node_t *
ht_expiry(hitime_t *h)
{
    return &h->bins[EXPIRYINDEX];
}

INLINE static hitime_node_t *
ht_process(hitime_t *h)
{
    return &h->bins[PROCESSINDEX];
}

/*******************************************************************************
 * TIMEOUT FUNCTIONS
*******************************************************************************/

#ifndef recover_ptr
#define recover_ptr(p, type, field) \
    ({ \
        const typeof(((type *)0)->field) *__p = (p); \
        (type *)((char *)__p - offsetof(type, field)); \
    })
#endif

INLINE static hitime_node_t *
to_node(hitimeout_t *t)
{
    return &t->node;
}

INLINE static hitimeout_t *
to_timeout(hitime_node_t *n)
{
    return n ? recover_ptr(n, hitimeout_t, node): NULL;
}

/*******************************************************************************
 * NODE FUNCTIONS
*******************************************************************************/

INLINE static bool
node_in_list(hitime_node_t *n)
{
    return !!n->next;
}

INLINE static void
node_clear(hitime_node_t *n)
{
    n->next = NULL;
    n->prev = NULL;
}

INLINE static void
node_unlink_only(hitime_node_t *n)
{
    n->next->prev = n->prev;
    n->prev->next = n->next;
}

INLINE static void
node_unlink(hitime_node_t *n)
{
    node_unlink_only(n);
    node_clear(n);
}

/*******************************************************************************
 * LIST FUNCTIONS
*******************************************************************************/

INLINE static hitime_node_t *
list_dq(hitime_node_t *l)
{
    hitime_node_t *n = NULL;

    if (l != l->next)
    {
        n = l->next;
        node_unlink(n);
    }

    return n;
}

INLINE static void
list_nq(hitime_node_t *l, hitime_node_t *n)
{
    n->next = l;
    n->prev = l->prev;
    l->prev->next = n;
    l->prev = n;
}

INLINE static bool
list_empty(hitime_node_t *n)
{
    return (n == n->next);
}

INLINE static bool
list_has(hitime_node_t *n)
{
    return (n != n->next);
}

INLINE static void
list_move(hitime_node_t *dst, hitime_node_t *src)
{
    dst->next = src->next;
    dst->prev = src->prev;
    src->next->prev = dst;
    src->prev->next = dst;
}

INLINE static void
list_clear(hitime_node_t *n)
{
    n->next = n;
    n->prev = n;
}

INLINE static void
list_clear_all(hitime_node_t *l, size_t num)
{
    int i;
    for (i = 0; i < num; ++i)
    {
        list_clear(l + i);
    }
}


/**
 * @brief Append items from l2 to l1.
 * @warn l2 cannot be empty.
 * @warn l2 is left in invalid state.
 */
INLINE static void
list_append(hitime_node_t *l1, hitime_node_t *l2)
{
    l2->next->prev = l1->prev;
    l2->prev->next = l1;
    l1->prev->next = l2->next;
    l1->prev = l2->prev;
}

/*******************************************************************************
 * HIGHTIME FUNCTIONS
*******************************************************************************/

INLINE static void
ht_nq(hitime_t *h, hitimeout_t *t)
{
    int index;

    /* Find which list to add the hitimeout to. */
    uint64_t bits = t->when ^ h->last;
    if (UNLIKELY(bits > WAITMAX))
    {
        index = MAXINDEX;
    }
    else
    {
        index = get_high_index((uint32_t)bits);
    }

    //binset_set(h, index);
    list_nq(&h->bins[index], to_node(t));
}

/**
 * @brief Initialize embedded struct.
 */
void
hitime_init(hitime_t *h)
{
    hitime_memzero(h, sizeof(*h));
    list_clear_all(h->bins, HITIME_BINS);
}

/**
 * @brief Cleanup embedded struct that was previously initialized.
 * @warn You must cleanup all hitimeouts before calling this;
 *       recommended to first handle expired hitimeouts,
 *       second call hitime_expire_all and handle remaining expired hitimeouts.
 */
void
hitime_destroy(hitime_t *h)
{
    hitime_memzero(h, sizeof(*h));
}

/**
 * @brief Add the hitimeout to the manager.
 * @param h
 * @param t - The hitimeout to add.
 *
 * If the hitimeout appears to be a part of a list already, do nothing.
 * If the hitimeout has already expired then added to expired list.
 * Otherwise, the hitimeout to the internal data structure for future processing.
 */
void
hitime_start(hitime_t * h, hitimeout_t *t)
{
    /* Timeouts should not be in a list already. */
    if (UNLIKELY(node_in_list(to_node(t))))
    {
        return;
    }

    /* If this already expired, then add to expired.
     * Ensures delta in next step is non-zero.
     */
    if (UNLIKELY(is_expired(h, t)))
    {
        list_nq(ht_expiry(h), to_node(t));
    }
    else
    {
        ht_nq(h, t);
    }
}

/**
 * @param h
 * @param t - Timeout to update.
 * @param min - The minimum expiry time.
 * @param max - The maximum expiry time.
 */
void
hitime_start_range(hitime_t *h, hitimeout_t *t, uint64_t min, uint64_t max)
{
}

/**
 * @param h
 * @param t - The hitimeout to stop.
 * @brief Stop the timer by removing it from the datastructure.
 */
void
hitime_stop(hitime_t *h, hitimeout_t *t)
{
    if (node_in_list(to_node(t)))
    {
        /* Unlink must happen or list is never empty. */
        node_unlink(to_node(t));
    }
}

/**
 * @param h
 * @param t - Timeout to update.
 * @param when - The new expiry timestamp.
 * @brief Stop the timeout, if started, restart timeout.
 */
void
hitime_touch(hitime_t *h, hitimeout_t *t, uint64_t when)
{
    t->when = when;

    if (node_in_list(to_node(t)))
    {
        node_unlink_only(to_node(t));
    }

    if (UNLIKELY(is_expired(h, t)))
    {
        list_nq(ht_expiry(h), to_node(t));
    }
    else
    {
        ht_nq(h, t);
    }
}

INLINE static int
ht_get_wait(hitime_t *h)
{
    int wait = WAITMAX;

    int index = 0;
    for (; index < MAXINDEX; ++index)
    {
        if (list_has(h->bins + index))
        {
            uint32_t msb = 1 << index;
            uint64_t mask = msb - 1;
            uint32_t w = msb - (uint32_t)(mask & h->last);
            wait = (int)w; // CAST: Okay because we restrict the size of index.

            break;
        }
    }

    return wait;
}

/**
 * @param h
 * @return The time to wait.
 */
int
hitime_get_wait(hitime_t *h)
{
    return ht_get_wait(h);
}

/**
 * @param h
 * @param now - The current time.
 * @return The time to wait using the current now without updating.
 */
int
hitime_get_wait_with(hitime_t *h, uint64_t now)
{
    uint64_t diff = now - h->last;
    uint64_t w = ht_get_wait(h);
    /* Note that w is <= INT_MAX, so the difference is in range(int). */
    return diff < w ? (int)(w - diff) : 0;
}

/**
 * @brief First bin always expires since there is a guaranteed hitimeout
 *        of granularity 1.
 */
INLINE static void
ht_expire_first(hitime_t *h)
{
    if(list_has(h->bins))
    {
        list_append(ht_expiry(h), h->bins);
        list_clear(h->bins);
    }
}

/**
 * @brief Expire bins in bulk below a certain threshold.
 * @return Index of final bin processed.
 */
INLINE static int
ht_expire_bulk(hitime_t *h, uint64_t now)
{
    int index = 1;
    uint32_t elapsed = get_elpased(now, h->last);

    /* NOTE:
     * If the elapsed time is less than the needed (wait time)
     * then we check bins needlessly.
     * However, storing the wait time and checking it will likely consume as much
     * time so we don't perform that optimization.
     * Also, juggling one more item of state doesn't seem worthwhile.
     */

    /* Get index of guaranteed expires. */
    int index_max = get_high_index(elapsed);
    for (; index < index_max; ++index)
    {
        hitime_node_t *l = h->bins + index;
        if (list_has(l))
        {
            list_append(ht_expiry(h), l);
            list_clear(l);
        }
    }

    return index;
}

/**
 * @return The max index of bins to review.
 */
INLINE static void
ht_expire_individually_setup(hitime_t *h, int index, uint64_t now)
{
    uint64_t bits = now ^ h->last;
    int max_index;
    if (UNLIKELY(bits > WAITMAX))
    {
        max_index = MAXINDEX;
    }
    else
    {
        max_index = get_high_index((uint32_t)bits);
    }

    for (; index <= max_index; ++index)
    {
        hitime_node_t *l = h->bins + index;
        if (list_has(l))
        {
            list_append(ht_process(h), l);
            list_clear(l);
        }
    }
}

/**
 * @brief Check timeouts one-by-one and reassign to bins.
 */
INLINE static int
ht_expire_individually(hitime_t *h, int maxops)
{
    int ops = 0;

    hitime_node_t *l = ht_process(h);
    if (list_has(l) && ops < maxops)
    {
        hitime_node_t *curr = l->next;
        while (curr != l && ops < maxops)
        {
            hitime_node_t *next = curr->next;

            hitimeout_t *t = to_timeout(curr);
            node_unlink(curr);
            if (UNLIKELY(is_expired(h, t)))
            {
                list_nq(ht_expiry(h), curr);
            }
            else
            {
                ht_nq(h, t);
            }

            ++ops;
            curr = next;
        }
    }

    return ops;
}

INLINE static void
ht_update_last(hitime_t *h, uint64_t now)
{
    h->last = now;
}

/**
 * @brief Move any expired hitimeouts to expired list.
 * @param h
 * @param now - The current time.
 * @return True if there are expired timers.
 */
bool
hitime_timeout(hitime_t *h, uint64_t now)
{
    hitimestate_t state;
    hitimestate_init(&state, now);
    while (!hitime_timeout_r(h, &state, INT_MAX));

    return !list_empty(ht_expiry(h));
}

/**
 * @brief Take all timers and put into expired.
 * @param h
 */
void
hitime_expire_all(hitime_t * h)
{
    hitime_node_t *ls = h->bins;

    int i;
    for (i = 0; i < 32; ++i)
    {
        hitime_node_t *l = ls + i;
        if (list_has(l))
        {
            list_append(ht_expiry(h), l);
        }
    }

    // Clear all lists.
    list_clear_all(ls, 32);

    // Move everything from the process list to expiry.
    if (list_has(ht_process(h)))
    {
        list_append(ht_expiry(h), ht_process(h));
        list_clear(ht_process(h));
    }
}

/**
 * @param h
 * @return The next expired hitimeout; NULL if none.
 */
hitimeout_t *
hitime_get_next(hitime_t *h)
{
    return to_timeout(list_dq(ht_expiry(h)));
}

uint64_t
hitime_max_wait(void)
{
    return WAITMAX;
}

uint64_t
hitime_get_last(hitime_t *h)
{
    return h->last;
}

enum hitimestate
{
    HITIMESTATE_START = 0,
    HITIMESTATE_EXPIRE,
    HITIMESTATE_DONE,
};

void
hitimestate_init(hitimestate_t *hts, uint64_t now)
{
    hts->state = HITIMESTATE_START;
    hts->now = now;
}

/**
 * @brief Process up to maxops of data.
 * @warn If the overflow bin is being processed it cannot be interrupted.
 */
bool
hitime_timeout_r(hitime_t *h, hitimestate_t *hts, int maxops)
{
    int ops = 0;

    do
    {
        switch (hts->state)
        {
            case HITIMESTATE_START:
                if (hts->now > h->last)
                {

                    ht_expire_first(h);
                    ++ops;
                    int index = ht_expire_bulk(h, hts->now);
                    ++ops;
                    ht_expire_individually_setup(h, index, hts->now);
                    ++ops;
                    hts->state = HITIMESTATE_EXPIRE;
                }
                else
                {
                    hts->state = HITIMESTATE_DONE;
                }
                ht_update_last(h, hts->now);
            break;

            case HITIMESTATE_EXPIRE:
            {
                int ops_done = ht_expire_individually(h, maxops - ops);
                ops += ops_done;
                if (!ops_done && ops < maxops)
                {
                    hts->state = HITIMESTATE_DONE;
                }
            }
            break;

            default:
                hts->state = HITIMESTATE_DONE;
            break;
        }
    } while (ops < maxops && !(hts->state == HITIMESTATE_DONE));

    return (hts->state == HITIMESTATE_DONE);
}

