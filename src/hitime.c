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
hitimeout_reset(hitimeout_t *t)
{
    hitime_memzero(t, sizeof(*t));
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
static int MAXINDEX = 31;

INLINE static int
_pop32(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

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
    uint32_t m = n ^ (n - 1);
    return _pop32(m) - 1;
}

/**
 * @warn Do NOT call with zero.
 */
INLINE static int
get_high_index(uint32_t n)
{
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return _pop32(n) - 1;
}

INLINE static uint32_t
get_elpased(uint64_t now, uint64_t last)
{
    uint64_t d = now - last;
    return d > DELTMAX ? UPPERBIT : (uint32_t)d;
}

INLINE static hitime_node_t *
expiry(hitime_t *h)
{
    return &h->bins[EXPIRYINDEX];
}

INLINE static void
binset_set(hitime_t *h, int index)
{
    h->binset = h->binset | (1 << index);
}

INLINE static void
binset_clear(hitime_t *h, int index)
{
    h->binset = h->binset & ~(1 << index);
}

INLINE static void
binset_clear_all(hitime_t *h)
{
    h->binset = 0;
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

INLINE static int
hitimeout_index(hitimeout_t *t)
{
    return t->index;
}

INLINE static void
hitimeout_set_index(hitimeout_t *t, int index)
{
    t->index = index;
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
node_unlink(hitime_node_t *n)
{
    n->next->prev = n->prev;
    n->prev->next = n->next;
    node_clear(n);
}

/*******************************************************************************
 * LIST FUNCTIONS
*******************************************************************************/

INLINE static hitime_node_t *
list_dq(hitime_node_t *l)
{
    if (l != l->next)
    {
        hitime_node_t *n = l->next;
        node_unlink(n);
        return n;
    }
    else
    {
        return NULL;
    }
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

INLINE static int
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

    binset_set(h, index);
    hitimeout_set_index(t, index);
    list_nq(&h->bins[index], to_node(t));
}

/**
 * @brief Initialize embedded struct.
 */
void
hitime_init(hitime_t *h)
{
    hitime_memzero(h, sizeof(*h));
    list_clear_all(h->bins, 33);
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
        hitimeout_set_index(t, EXPIRYINDEX);
        list_nq(expiry(h), to_node(t));
    }
    else
    {
        ht_nq(h, t);
    }

    /* Done */
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
        int index = hitimeout_index(t);
        
        /* Unlink must happen or list is never empty. */
        node_unlink(to_node(t));

        if (list_empty(&h->bins[index]))
        {
            binset_clear(h, index);
        }
    }
}

INLINE static int
ht_get_wait(hitime_t *h)
{
    if (LIKELY(h->binset))
    {
        /* The time to wait is the distance to the lowest trigger point.
         * So we take the lowest bit set in binset and find the distance
         * from elapsed time to trigger that bin.
         */
        int index = get_low_index(h->binset);
        if (LIKELY(index != MAXINDEX))
        {
            uint32_t msb = 1 << index;
            uint64_t mask = msb - 1;
            uint32_t w = msb - (uint32_t)(mask & h->last);
            return (int)w;
        }
        else
        {
            return (int)WAITMAX;
        }
    }
    else
    {
        /* There are no hitimeouts. Set to maximum. */
        return (int)WAITMAX;
    }
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

INLINE static void
ht_process(hitime_t *h, hitime_node_t *l)
{
    hitime_node_t *curr = l->next;
    while (curr != l)
    {
        hitime_node_t *next = curr->next;

        hitimeout_t *t = to_timeout(curr);
        node_unlink(curr);
        if (UNLIKELY(is_expired(h, t)))
        {
            list_nq(expiry(h), curr);
        }
        else
        {
            ht_nq(h, t);
        }

        curr = next;
    }
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
    /* Make sure we're advancing time-wise. */
    if (now > h->last)
    {
        uint32_t elapsed = get_elpased(now, h->last);
        /* If the elapsed time is less than the needed then we check bins
         * needlessly.
         */
        // TODO check that elapsed >= wait

        /* Save last. */
        uint64_t last = h->last;
        /* Update concept of now. */
        h->last = now;

        /* First bin always expires since there is a guaranteed hitimeout
         * of granularity 1.
         */
        if(list_has(h->bins))
        {
            list_append(expiry(h), h->bins);
            list_clear(h->bins);
            binset_clear(h, 0);
        }

        int index0 = 1;

        /* Get index of guaranteed expires. */
        int index1 = get_high_index(elapsed);
        for (; index0 < index1; ++index0)
        {
            hitime_node_t *l = h->bins + index0;
            if (list_has(l))
            {
                list_append(expiry(h), l);
                list_clear(l);
                binset_clear(h, index0);
            }
        }

        uint64_t bits = now ^ last;
        int index2;
        if (UNLIKELY(bits > WAITMAX))
        {
            index2 = MAXINDEX;
        }
        else
        {
            index2 = get_high_index((uint32_t)bits);
        }
#if 0
        uint32_t newlapsed = h->lapsed + lapsed;
        int index2 = get_high_index(newlapsed ^ h->lapsed);
        h->lapsed = newlapsed;
#endif

        bool overflow = false;
        if (MAXINDEX == index2)
        {
            --index2;
            overflow = true;
        }
        
        // TODO we may be able to short-circuit the checks...
        //      this is because everything between the triggered bin and the
        //      lapsed time bins aren't necessarily triggered, but they
        //      do need to be checked
        //      We could check the index against the binset.
        /* Get index of expires to re-check. */
        for (; index0 <= index2; ++index0)
        {
            hitime_node_t *l = h->bins + index0;
            ht_process(h, l);
            if (list_empty(l))
            {
                binset_clear(h, index0);
            }
        }

        if (UNLIKELY(overflow && list_has(h->bins + MAXINDEX)))
        {
            /* Process the final bin.
             * All nodes must be processed.
             * Some nodes may be placed back into the same bin.
             * The list must be moved so we can reinsert without issue.
             */
            hitime_node_t list;
            hitime_node_t *l = &list;
            list_move(l, h->bins + MAXINDEX);
            list_clear(h->bins + MAXINDEX);
            ht_process(h, l);

            if (list_empty(h->bins + MAXINDEX))
            {
                binset_clear(h, MAXINDEX);
            }
        }
    }

    return !list_empty(expiry(h));
}

/**
 * @brief Take all timers and put into expired.
 * @param h
 */
void
hitime_expire_all(hitime_t * h)
{
    if (h->binset)
    {
        hitime_node_t *ls = h->bins;

        int i;
        for (i = 0; i < 32; ++i)
        {
            hitime_node_t *l = ls + i;
            if (list_has(l))
            {
                list_append(expiry(h), l);
            }
        }

        binset_clear_all(h);
        list_clear_all(ls, 32);
    }
}

/**
 * @param h
 * @return The next expired hitimeout; NULL if none.
 */
hitimeout_t *
hitime_get_next(hitime_t *h)
{
    return to_timeout(list_dq(expiry(h)));
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

