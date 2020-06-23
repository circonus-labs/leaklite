/*
 * Copyright (c) 2020, Circonus, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name Circonus, Inc. nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UTILS_LEAKLITE_H
#define _UTILS_LEAKLITE_H

#include "ck_pr.h"
#include "pointer_hash.h"
#include <stdbool.h>

typedef enum { NOT_SET, MALLOC, CALLOC, NEW, NEW_ARR, ALIGN_NEW, ALIGN_NEW_ARR } leaklite_type;
static const char *leaklite_type_str[] = {"not set", "malloc", "calloc", "new", "new[]",
                                          "al new", "al new[]"};

struct leaklite_alloc_tracker;
typedef struct leaklite_alloc_tracker {
  const char *fname;
  const char *srcfile;
  uint32_t linenum;
  leaklite_type type;
  uint64_t active_allocs;
  uint64_t active_memsize;
  uint64_t num_frees;
  bool was_linked;
  struct leaklite_alloc_tracker *next;
} leaklite_alloc_tracker_t;

typedef struct {
  uint64_t guard;
  uint64_t size;
  leaklite_alloc_tracker_t *tracker;
} leaklite_trailer_t;

extern leaklite_alloc_tracker_t *tracker_head;
static pthread_mutex_t tracker_head_mutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef NO_LAMBDA_LEAKLITE
static inline void *leaklite_alloc(size_t size, size_t *align, leaklite_alloc_tracker_t *tracker,
                                   leaklite_type type)
#else
static inline void *leaklite_alloc(size_t size, size_t *align,
                                   leaklite_alloc_tracker_t *(*get_tracker)(), leaklite_type type,
                                   const char *fname)
#endif
{
  void *ret = NULL;
  if (align) {
    size_t addsize = (sizeof(leaklite_trailer_t) / *align) * *align + *align;
    ret = aligned_alloc(size + addsize, *align);
  }
  else {
    ret = malloc(size + sizeof(leaklite_trailer_t));
  }
  if (ret) {
    leaklite_trailer_t *trailer = (leaklite_trailer_t *)((char *)ret + size);
    if (!pointer_hash_insert((char *)ret, (uint64_t)trailer))
    {
//      log_error("ERROR - Leaklite pointer hash collision\n");
    }
    trailer->size = size;
#ifndef NO_LAMBDA_LEAKLITE
    leaklite_alloc_tracker_t *tracker = get_tracker();
#endif
    trailer->tracker = tracker;
    ck_pr_inc_64(&tracker->active_allocs);
    ck_pr_add_64(&tracker->active_memsize, size);
    if (!tracker->was_linked) {
      pthread_mutex_lock(&tracker_head_mutex);
      tracker->type = type;
#ifndef NO_LAMBDA_LEAKLITE
      tracker->fname = fname;
#endif
      tracker->next = tracker_head;
      tracker_head = tracker;
      tracker->was_linked = true;
      pthread_mutex_unlock(&tracker_head_mutex);
    }
  }
  return ret;
}

#ifdef NO_LAMBDA_LEAKLITE
static inline void *leaklite_malloc(size_t size, size_t *align, leaklite_alloc_tracker_t *tracker)
#else
static inline void *leaklite_malloc(size_t size, size_t *align, const char *fname,
                                    leaklite_alloc_tracker_t *(*get_tracker)())
#endif
{
#ifdef NO_LAMBDA_LEAKLITE
  return leaklite_alloc(size, align, tracker, MALLOC);
#else
  return leaklite_alloc(size, align, get_tracker, MALLOC, fname);
#endif
}

#ifdef NO_LAMBDA_LEAKLITE
static inline void *leaklite_calloc(size_t count, size_t size, size_t *align,
                                    leaklite_alloc_tracker_t *tracker)
#else
static inline void *leaklite_calloc(size_t count, size_t size, size_t *align, const char *fname,
                                    leaklite_alloc_tracker_t *(*get_tracker)())
#endif
{
#ifdef NO_LAMBDA_LEAKLITE
  void *ret = leaklite_alloc(count * size, align, tracker, CALLOC);
#else
  void *ret = leaklite_alloc(count * size, align, get_tracker, CALLOC, fname);
#endif
  memset(ret, 0, count * size);
  return ret;
}

static inline void leaklite_free(void *ptr, const char *fname, const char *srcfile,
                                 uint32_t linenum)
{
  if (!ptr) {
//    log_error("Attempt to free a null pointer in %s at line %u of %s\n", fname,
//              linenum, srcfile);
  }
  else {   
    leaklite_trailer_t **trailer = (leaklite_trailer_t **)pointer_hash_get(ptr);
    if (trailer) {
      uint64_t size = (char *)*trailer - (char *)ptr;
      if ((*trailer)->size != size) {
//        log_error("Buffer overflow detected in %s at line %u of %s\n", fname, linenum,
//                  srcfile);
      }
      leaklite_alloc_tracker_t *tracker = (*trailer)->tracker;
      if (!tracker) {
//        log_error("Double free detected in %s at line %u of %s\n", fname, linenum,
//                  srcfile);
      }
      else {
        ck_pr_dec_64(&tracker->active_allocs);
        ck_pr_sub_64(&tracker->active_memsize, size);
        ck_pr_inc_64(&tracker->num_frees);
        (*trailer)->tracker = NULL;
        pointer_hash_remove(ptr);
      }
    }
    free(ptr);
  }
}

#define CONCAT(first, second) CONCAT_SIMPLE(first, second)
#define CONCAT_SIMPLE(first, second) first ## second

#ifdef DISABLE_LEAKLITE
#define __LEAKLITE__
#else
#ifdef NO_LAMBDA_LEAKLITE
#define __LEAKLITE__ \
  static leaklite_alloc_tracker_t CONCAT(leaklite_alloc_tracker,__LINE__) = \
    {__FUNCTION__, __FILE__, __LINE__, NOT_SET, 0, 0, 0, false, NULL};

#define malloc(size) \
  leaklite_malloc(size, NULL, &CONCAT(leaklite_alloc_tracker,__LINE__))

#define calloc(count, size) \
  leaklite_calloc(count, size, NULL, &CONCAT(leaklite_alloc_tracker,__LINE__))
#else
#define __LEAKLITE__
#define malloc(size) \
    leaklite_malloc(size, NULL, __FUNCTION__, [] () -> leaklite_alloc_tracker_t * { \
      static leaklite_alloc_tracker_t CONCAT(leaklite_malloc_tracker,__LINE__) = \
        {__FUNCTION__, __FILE__, __LINE__, MALLOC, 0, 0, 0, false, NULL}; \
      return &CONCAT(leaklite_malloc_tracker,__LINE__); \
      })
      /* log_error("Create/access leaklite_malloc_tracker%u %p (%" PRIu64 " unfreed, %" PRIu64 " freed, %" PRIu64 " bytes) %p\n", \
                   __LINE__, &CONCAT(leaklite_malloc_tracker,__LINE__), \
                   CONCAT(leaklite_malloc_tracker,__LINE__).active_allocs, \
                   CONCAT(leaklite_malloc_tracker,__LINE__).num_frees, \
                   CONCAT(leaklite_malloc_tracker,__LINE__).active_memsize, \
                   CONCAT(leaklite_malloc_tracker,__LINE__).next); \ */

#define calloc(count, size) \
    leaklite_calloc(count, size, NULL, __FUNCTION__, [] () -> leaklite_alloc_tracker_t * { \
      static leaklite_alloc_tracker_t CONCAT(leaklite_calloc_tracker,__LINE__) = \
        {__FUNCTION__, __FILE__, __LINE__, CALLOC, 0, 0, 0, false, NULL}; \
      return &CONCAT(leaklite_calloc_tracker,__LINE__); \
      })
      /* log_error("Create/access leaklite_calloc_tracker%u %p (%" PRIu64 " unfreed, %" PRIu64 " freed, %" PRIu64 " bytes) %p\n", \
                   __LINE__, &CONCAT(leaklite_calloc_tracker,__LINE__), \
                   CONCAT(leaklite_calloc_tracker,__LINE__).active_allocs, \
                   CONCAT(leaklite_calloc_tracker,__LINE__).num_frees, \
                   CONCAT(leaklite_calloc_tracker,__LINE__).active_memsize, \
                   CONCAT(leaklite_calloc_tracker,__LINE__).next); \ */
#endif
#endif

#define free(ptr) \
  leaklite_free(ptr, __FUNCTION__, __FILE__, __LINE__)

static inline void leaklite_dump()
{
  leaklite_alloc_tracker_t *curr = tracker_head;
  printf("LEAKLITE MEMORY DUMP:\n");
  uint64_t total = 0;
  while (curr) {
    printf("%" PRIu64 " bytes (%" PRIu64 " unfreed, %" PRIu64 " freed) %s %s:%u (%s)\n",
           curr->active_memsize, curr->active_allocs, curr->num_frees, leaklite_type_str[curr->type],
           curr->fname, curr->linenum, curr->srcfile);
    total = total + curr->active_memsize;
    curr = curr->next;
  }
  printf("%" PRIu64 " total monitored allocated memory\n", total);
}

#endif
