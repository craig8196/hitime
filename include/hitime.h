/********************************************************************************
 * Copyright (c) 2019 Craig Jacobson
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
    struct hitime_node_s *next;
    struct hitime_node_s *prev;
} hitime_node_t;

typedef struct timeout_s
{
    hitime_node_t node;
    uint32_t index;
    int type;
    uint64_t when;
    void *data;
} timeout_t;

timeout_t *
timeout_new(void);
void
timeout_reset(timeout_t *);
void
timeout_set(timeout_t *, uint64_t, void *, int);
uint64_t
timeout_when(timeout_t *);
void *
timeout_data(timeout_t *);
int
timeout_type(timeout_t *);
void
timeout_free(timeout_t **);

#define HITIME_BINS (33)

typedef struct hitime_s
{
    /* Internal */
    uint64_t last;
#if 0
    uint32_t lapsed;
#endif
    uint32_t binset; // which bins have timeouts
    hitime_node_t bins[HITIME_BINS]; // last bin is expiry
} hitime_t;


hitime_t *
hitime_new(void);
void
hitime_free(hitime_t * *);

void
hitime_init(hitime_t *);
void
hitime_destroy(hitime_t *);

void
hitime_start(hitime_t *, timeout_t *);
void
hitime_stop(hitime_t *, timeout_t *);

int
hitime_get_wait(hitime_t *);
int
hitime_get_wait_with(hitime_t *, uint64_t);
bool
hitime_timeout(hitime_t *, uint64_t);

void
hitime_expire_all(hitime_t *);
timeout_t *
hitime_get_next(hitime_t *);

/* Exports for testing. */
uint64_t
hitime_max_wait(void);
uint64_t
hitime_get_last(hitime_t *);


#ifdef __cplusplus
}
#endif
#endif /* HITIME_H_ */

