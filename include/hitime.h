/********************************************************************************
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
 * @file hitime.h
 * @author Craig Jacobson
 * @brief Timeout manager interface.
 */
#ifndef HITIME_H_
#define HITIME_H_
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stdint.h>

typedef struct hitime_node_s
{
    struct hitime_node_s * next;
    struct hitime_node_s * prev;
} hitime_node_t;

typedef struct
{
    hitime_node_t node;
    uint32_t      index;
    int           type;
    uint64_t      when;
    void *        data;
} hitimeout_t;

void
hitimeout_init(hitimeout_t *);
void
hitimeout_reset(hitimeout_t *);
void
hitimeout_destroy(hitimeout_t *);
void
hitimeout_set(hitimeout_t *, uint64_t, void *, int);
uint64_t
hitimeout_when(hitimeout_t *);
void *
hitimeout_data(hitimeout_t *);
int
hitimeout_type(hitimeout_t *);

/* sizeof(uint32_t)*8 + 1 for expiry + 1 for processing */
#define HITIME_BINS (34)

typedef struct
{
    /* Internal */
    uint64_t      last;
    uint32_t      binset; // which bins have hitimeouts, used for wait
    hitime_node_t bins[HITIME_BINS];
} hitime_t;


void
hitime_init(hitime_t *);
void
hitime_destroy(hitime_t *);

void
hitime_start(hitime_t *, hitimeout_t *);
void
hitime_stop(hitime_t *, hitimeout_t *);

int
hitime_get_wait(hitime_t *);
int
hitime_get_wait_with(hitime_t *, uint64_t);
bool
hitime_timeout(hitime_t *, uint64_t);

void
hitime_expire_all(hitime_t *);
hitimeout_t *
hitime_get_next(hitime_t *);

/* Exports for testing. */
uint64_t
hitime_max_wait(void);
uint64_t
hitime_get_last(hitime_t *);

/* State management for partial timeout calls. */
typedef struct
{
    int      state;
    uint64_t now;
} hitimestate_t;

void
hitimestate_init(hitimestate_t *, uint64_t);
bool
hitime_timeout_r(hitime_t *, hitimestate_t *, int);


#ifdef __cplusplus
}
#endif
#endif /* HITIME_H_ */

