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


#if 0
/*******************************************************************************
 * INVARIANT FUNCTIONS
*******************************************************************************/
static bool
check_list_integrity(hitime_node_t *l)
{
    if (NULL == l) { return false; }
    if (NULL == l->next || NULL == l->prev) { return false; }

    hitime_node_t *prev = l;
    hitime_node_t *curr = l->next;

    while (l != curr)
    {
        hitime_node_t *next = curr->next;
        if (NULL == next) { return false; }
        if (!(curr->prev == prev)) { return false; }
        prev = curr;
        curr = next;
    }

    return true;
}

static bool
check_all_lists(hitime_t *h)
{
    return check_list_integrity(&h->expired);
}
#endif


/*******************************************************************************
 * TIMEOUT FUNCTIONS
*******************************************************************************/

void
hitimeout_init(hitimeout_t *t)
{
    (*t) = (const hitimeout_t){ 0 };
}

void
hitimeout_reset(hitimeout_t *t)
{
    hitimeout_init(t);
}

void
hitimeout_destroy(hitimeout_t * t)
{
    (*t) = (const hitimeout_t){ 0 };
}

void
hitimeout_set(hitimeout_t *t, uint64_t when, void *data)
{
    t->when = when;
    t->data = data;
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


/*******************************************************************************
 * HELPER FUNCTIONS
*******************************************************************************/

static const uint64_t WAITMAX = UINT64_MAX;

INLINE static int
is_expired(hitime_t *h, hitimeout_t *t)
{
    return (t->when <= h->last);
}

#if !(defined __GNUC__)
static const int8_t bits_to_log2[] =
{
    63, 0, 1, 52, 2, 6, 53, 26,
    3, 37, 40, 7, 33, 54, 47, 27,
    61, 4, 38, 45, 43, 41, 21, 8,
    23, 34, 58, 55, 48, 17, 28, 10,
    62, 51, 5, 25, 36, 39, 32, 46,
    60, 44, 42, 20, 22, 57, 16, 9,
    50, 24, 35, 31, 59, 19, 56, 15,
    49, 30, 18, 14, 29, 13, 12, 11,
};
static const uint64_t bits_to_log2_multi = 0x022fdd63cc95386dULL;
#endif

/**
 * @warn Do NOT input zero.
 * @return The index of the highest set bit.
 */
INLINE static int
get_high_index64(uint64_t n)
{
#if !(defined __GNUC__)
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return bits_to_log2[(n * bits_to_log2_multi) >> 58];
#else
    return 63 - __builtin_clzl(n);
#endif
}

INLINE static uint64_t
get_elpased(uint64_t now, uint64_t last)
{
    return now - last;
}

INLINE static hitime_node_t *
ht_get_expired(hitime_t *h)
{
    return &h->expired;
}

INLINE static hitime_node_t *
ht_get_processing(hitime_t *h)
{
    return &h->processing;
}

/*******************************************************************************
 * TIMEOUT FUNCTIONS
*******************************************************************************/

#ifndef recover_ptr
#define recover_ptr(p, type, field) \
    ((type *)((char *)(p) - offsetof(type, field)))
#endif

INLINE static hitime_node_t *
to_node(hitimeout_t *t)
{
    return &t->node;
}

INLINE static hitimeout_t *
to_timeout(hitime_node_t *n)
{
    return recover_ptr(n, hitimeout_t, node);
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
list_is_empty(hitime_node_t *n)
{
    return (n == n->next);
}

INLINE static bool
list_has(hitime_node_t *n)
{
    return (n != n->next);
}

INLINE static void
list_clear(hitime_node_t *n)
{
    n->next = n;
    n->prev = n;
}

INLINE static void
lists_clear(hitime_node_t *l, size_t num)
{
    size_t i;
    for (i = 0; i < num; ++i)
    {
        list_clear(l + i);
    }
}

/**
 * @brief Append items from l2 to l1.
 */
INLINE static void
list_append(hitime_node_t *l1, hitime_node_t *l2)
{
    if (list_has(l2))
    {
        l2->next->prev = l1->prev;
        l2->prev->next = l1;
        l1->prev->next = l2->next;
        l1->prev = l2->prev;
        list_clear(l2);
    }
}

INLINE static int
list_count(hitime_node_t *l)
{
    int count = 0;
    hitime_node_t *next = l->next;

    while (next != l)
    {
        ++count;
        next = next->next;
    }

    return count;
}

/*******************************************************************************
 * HIGHTIME FUNCTIONS
*******************************************************************************/

INLINE static void
ht_nq(hitime_t *h, hitimeout_t *t)
{
    /* Find which list to add the hitimeout to. */
    uint64_t bits = t->when ^ h->last;
    int index = get_high_index64(bits);
    list_nq(&h->bins[index], to_node(t));
}

/**
 * @brief Initialize embedded struct.
 */
void
hitime_init(hitime_t *h)
{
    h->last = 0;
    list_clear(&h->expired);
    list_clear(&h->processing);
    lists_clear(h->bins, HITIME_BINS);
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
    (*h) = (const hitime_t){ 0 };
}

/**
 * @brief Add the hitimeout to the manager.
 * @warn Remember to maintain referential stability! 'hitimeout_t' is a node internally!
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
        list_nq(ht_get_expired(h), to_node(t));
    }
    else
    {
        ht_nq(h, t);
    }
}

/**
 * Set the timeout to be between the two values.
 * The objective is to minimize the number of times the
 * timeout gets handled internally.
 * @param h
 * @param t - Timeout to update.
 * @param min - The minimum expired time.
 * @param max - The maximum expired time.
 */
void
hitime_start_range(hitime_t *h, hitimeout_t *t, uint64_t min, uint64_t max)
{
    // TODO does this still work?? I'm not sure I implemented this correctly.
    // TODO do some randomized testing
    uint64_t bits = max ^ min; // TODO why am I not taking the difference at this step?
    int index = get_high_index64(bits);
    uint64_t mask = ~((((uint64_t)1) << index) - 1);
    uint64_t newwhen = max & mask;
    t->when = newwhen;
    hitime_start(h, t);
}

/**
 * @param h
 * @param t - The hitimeout to stop.
 * @brief Stop the timer by removing it from the datastructure.
 */
void
hitime_stop(hitime_t *h, hitimeout_t *t)
{
    // Eradicate unused value message
    h = h;

    if (node_in_list(to_node(t)))
    {
        /* Unlink must happen or list is never empty. */
        node_unlink(to_node(t));
    }
}

/**
 * @param h
 * @param t - Timeout to update.
 * @param when - The new expired timestamp.
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
        list_nq(ht_get_expired(h), to_node(t));
    }
    else
    {
        ht_nq(h, t);
    }
}

INLINE static uint64_t
ht_get_wait(hitime_t *h)
{
    uint64_t wait = WAITMAX;

    int index = 0;
    for (; index < HITIME_BINS; ++index)
    {
        if (list_has((h->bins) + index))
        {
            uint64_t msb = ((uint64_t)1) << index;
            uint64_t mask = msb - 1;
            wait = (mask - (mask & h->last)) + 1;
            break;
        }
    }

    return wait;
}

/**
 * @param h
 * @return The time to wait.
 */
uint64_t
hitime_get_wait(hitime_t *h)
{
    return ht_get_wait(h);
}

/**
 * @param h
 * @param now - The current time.
 * @return The time to wait using the current now without updating.
 */
uint64_t
hitime_get_wait_with(hitime_t *h, uint64_t now)
{
    uint64_t diff = now - h->last;
    uint64_t w = ht_get_wait(h);
    return diff < w ? (w - diff) : 0;
}

/**
 * @brief First bin always expires since there is a guaranteed hitimeout
 *        of granularity 1.
 */
INLINE static void
ht_expire_first(hitime_t *h)
{
    list_append(ht_get_expired(h), h->bins);
}

/**
 * @brief Expire bins in bulk below a certain threshold.
 * @return Index of final bin processed.
 */
INLINE static int
ht_expire_bulk(hitime_t *h, uint64_t now)
{
    int index = 1;
    uint64_t elapsed = get_elpased(now, h->last);

    /* NOTE:
     * If the elapsed time is less than the needed (wait time)
     * then we check bins needlessly.
     * However, storing the wait time and checking it will likely consume as much
     * time so we don't perform that optimization.
     * Also, juggling one more item of state doesn't seem worthwhile.
     */

    /* Get index of guaranteed expires. */
    int index_max = get_high_index64(elapsed);
    for (; index < index_max; ++index)
    {
        hitime_node_t *l = h->bins + index;
        list_append(ht_get_expired(h), l);
    }

    return index;
}

/**
 * @return The max index of bins to review.
 */
INLINE static void
ht_process_setup(hitime_t *h, int index, uint64_t now)
{
    uint64_t bits = now ^ h->last;
    int max_index = get_high_index64(bits);

    for (; index <= max_index; ++index)
    {
        hitime_node_t *l = h->bins + index;
        list_append(ht_get_processing(h), l);
    }
}

/**
 * @brief Check timeouts one-by-one and reassign to bins.
 */
INLINE static int
ht_process(hitime_t *h, int maxops)
{
    int ops = 0;

    hitime_node_t *l = ht_get_processing(h);
    if (list_has(l) && ops < maxops)
    {
        hitime_node_t *curr = l->next;
        while (curr != l && ops < maxops)
        {
            hitime_node_t *next = curr->next;

            hitimeout_t *t = to_timeout(curr);
            //node_unlink_only(curr); // We don't need to null out pointers, they'll get set again.
            node_unlink(curr);
            if (UNLIKELY(is_expired(h, t)))
            {
                list_nq(ht_get_expired(h), curr);
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
ht_process_all(hitime_t *h)
{
    hitime_node_t *l = ht_get_processing(h);
    hitime_node_t *curr = l->next;
    while (curr != l)
    {
        hitime_node_t *next = curr->next;

        hitimeout_t *t = to_timeout(curr);
        if (UNLIKELY(is_expired(h, t)))
        {
            list_nq(ht_get_expired(h), curr);
        }
        else
        {
            ht_nq(h, t);
        }

        curr = next;
    }
    list_clear(l);
}

INLINE static void
ht_update_last(hitime_t *h, uint64_t now)
{
    h->last = now;
}

/**
 * @brief Move any expired hitimeouts to expired list.
 * @param h
 * @param delta - The amount of time elapsed since the last time.
 * @return False if nothing expired; true otherwise.
 */
bool
hitime_timedelta(hitime_t *h, uint64_t delta)
{
    uint64_t now = h->last + delta;
    if (now < h->last) { now = UINT64_MAX; }
    return hitime_timeout(h, now);
}

/**
 * @brief Move any expired hitimeouts to expired list.
 * @param h
 * @param now - The current time.
 * @return False if nothing expired (or invalid 'now' given); true otherwise.
 */
bool
hitime_timeout(hitime_t *h, uint64_t now)
{
    if (UNLIKELY(now <= h->last)) { return false; }

    ht_expire_first(h);
    int index = ht_expire_bulk(h, now);
    ht_process_setup(h, index, now);
    ht_update_last(h, now);
    ht_process_all(h);
    return !list_is_empty(ht_get_expired(h));
}

/**
 * @brief Only re-process a certain amount of timers at a time.
 *        This can help mitigate any concerns of the 'embarassing pause' problem.
 *        However, the recommendation is to use 'hitime_timeout'.
 * @warn Do NOT let significant amounts of time pass between iterations.
 * @param now - The current time.
 * @param max_ops - The maximum number of timers to place back into the queue.
 * @return True if there are still items to process; false otherwise.
 */
bool
hitime_timeout_partial(hitime_t *h, uint64_t now, int max_ops)
{
    if (now > h->last)
    {
        ht_expire_first(h);
        int index = ht_expire_bulk(h, now);
        ht_process_setup(h, index, now);
        ht_update_last(h, now);
    }
    ht_process(h, max_ops);
    return !list_is_empty(ht_get_processing(h));
}

/**
 * @brief Take all timers and put into expired.
 * @param h
 */
void
hitime_expire_all(hitime_t * h)
{
    hitime_node_t *ls = h->bins;
    // Iterate through core bins/lists.
    int i;
    for (i = 0; i < HITIME_BINS; ++i)
    {
        hitime_node_t *l = ls + i;
        list_append(ht_get_expired(h), l);
    }

    // Move everything from the process list to expired.
    list_append(ht_get_expired(h), ht_get_processing(h));
}

/**
 * @param h
 * @return The next expired hitimeout; NULL if none.
 */
hitimeout_t *
hitime_get_next(hitime_t *h)
{
    hitime_node_t *n = list_dq(ht_get_expired(h));
    return n ? to_timeout(n) : NULL;
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

/**
 * @brief Expire everything in the given bin. Mostly used in testing.
 * @param h
 * @param index - The index of the bin to expire.
 */
void
hitime_expire_bin(hitime_t *h, int index)
{
    if (UNLIKELY(index < 0 || index >= HITIME_BINS))
    {
        return;
    }

    hitime_node_t *l = (h->bins) + index;
    list_append(ht_get_expired(h), l);
}

/**
 * @brief Counts the number of timeouts in a bin. Mostly used for testing.
 * @param h
 * @param index - The index of the bin to count.
 * @return The count of items in the specified bin; zero on invalid bin.
 */
int
hitime_count_bin(hitime_t *h, int index)
{
    if (UNLIKELY(index < 0 || index >= HITIME_BINS))
    {
        return 0;
    }

    return list_count((h->bins) + index);
}

/**
 * @return The count of all timeouts in the datastructure, excluding expired.
 */
int
hitime_count_all(hitime_t *h)
{
    int count = 0;

    int i;
    for (i = 0; i < HITIME_BINS; ++i)
    {
        count += list_count((h->bins) + i);
    }

    return count;
}

/**
 * @return The count of all timeouts in the expired list.
 */
int
hitime_count_expired(hitime_t *h)
{
    return list_count(ht_get_expired(h));
}

/**
 * Dumps the bin counts to stdout.
 */
void
hitime_dump_stats(hitime_t *h)
{
    printf("NOW: %lu\nEXPIRED: %d\nPROCESSING: %d\nBINS:\n",
           h->last, list_count(ht_get_expired(h)), list_count(ht_get_processing(h)));

    int i;
    for (i = 0; i < HITIME_BINS; ++i)
    {
        printf("%d: %d\n", i, list_count((h->bins) + i));
    }
}

