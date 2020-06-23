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

#ifndef _UTILS_LEAKLITE_HPP
#define _UTILS_LEAKLITE_HPP

extern "C" {
#include "util/leaklite.h"
}
#include <new>

namespace std {
enum class align_val_t: size_t {};
}

#ifdef NO_LAMBDA_LEAKLITE
static inline void *leaklite_new(size_t size, size_t *align, leaklite_alloc_tracker_t *tracker,
                                 leaklite_type type)
#else
static inline void *leaklite_new(size_t size, std::align_val_t *align,
                                 leaklite_alloc_tracker_t *(*get_tracker)(), leaklite_type type,
                                 const char *fname)
#endif
{
  void *ret = NULL;
// Align is not yet working...
/*  if (align) {
    size_t addsize = (sizeof(leaklite_trailer_t) / *(size_t *)align) * *(size_t *)*align + *(size_t *)*align;
    if (type == NEW) ret = ::operator new(size + addsize, *align);
    else ret = ::operator new[](size + addsize, *align);
  }
  else {
*/    if (type == NEW) ret = ::operator new(size + sizeof(leaklite_trailer_t));
    else ret = ::operator new[](size + sizeof(leaklite_trailer_t));
//  }
  if (ret) {
    leaklite_trailer_t *trailer = (leaklite_trailer_t *)((char *)ret + size);
    if (!pointer_hash_insert((char *)ret, (uint64_t)trailer))
    {
//      log_error("ERROR - Leaklite memblock collision\n");
    }
    trailer->size = size;
#ifndef NO_LAMBDA_LEAKLITE
    leaklite_alloc_tracker_t *tracker = get_tracker();
#endif
    if (!pointer_hash_insert((char *)tracker, 0))
    {
//      log_error("ERROR - Leaklite tracker collision\n");
    }
    trailer->tracker = tracker;
//    log_error("Alloc'ed mem, tracker is %p\n", tracker);
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

static inline void leaklite_delete(void *ptr, const char *fname, const char *srcfile,
                                   uint32_t linenum, leaklite_type type)
{
  if (!ptr) {
//    log_error("Attempt to delete a null pointer in %s at line %u of %s\n", fname,
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
//        log_error("Double delete detected in %s at line %u of %s\n", fname, linenum,
//                  srcfile);
      }
      else {
        uint64_t *value = pointer_hash_get(tracker);
        if (!value || *value != 0) {
//          log_error("Not a valid tracker %p (value = %" PRIu64 ")\n", tracker, value ? *value : 0);
        }
        else {
//          log_error("Deleting block allocated at %s %u %s\n", tracker->fname,
//                    tracker->linenum, tracker->srcfile);
          ck_pr_dec_64(&tracker->active_allocs);
          ck_pr_sub_64(&tracker->active_memsize, size);
          ck_pr_inc_64(&tracker->num_frees);
          (*trailer)->tracker = NULL;
        }
        pointer_hash_remove(ptr);
      }
    }
    // will this work for arrays too?
    free(ptr);
  }
}

#ifndef DISABLE_LEAKLITE
#ifdef NO_LAMBDA_LEAKLITE
void *operator new(size_t size, const char *fname, leaklite_alloc_tracker_t *tracker);
void *operator new(size_t size, std::align_val_t al, const char *fname,
                   leaklite_alloc_tracker_t *tracker);
void *operator new[](size_t size, const char *fname, leaklite_alloc_tracker_t *tracker);
void *operator new[](size_t size, std::align_val_t al, const char *fname,
                     leaklite_alloc_tracker_t *tracker);

#define new new(__FUNCTION__, &CONCAT(leaklite_alloc_tracker,__LINE__))
#else
void *operator new(size_t size, const char *fname, leaklite_alloc_tracker_t *(*get_tracker)());
void *operator new(size_t size, std::align_val_t al, const char *fname,
                   leaklite_alloc_tracker_t *(*get_tracker)());
void *operator new[](size_t size, const char *fname, leaklite_alloc_tracker_t *(*get_tracker)());
void *operator new[](size_t size, std::align_val_t al, const char *fname,
                     leaklite_alloc_tracker_t *(*get_tracker)());

#define new new(__FUNCTION__, [] () -> leaklite_alloc_tracker_t * { \
      static leaklite_alloc_tracker_t CONCAT(leaklite_new_tracker,__LINE__) = \
        {__FUNCTION__, __FILE__, __LINE__, NOT_SET, 0, 0, 0, false, NULL}; \
      return &CONCAT(leaklite_new_tracker,__LINE__); \
      })
      /* log_error("Create/access leaklite_new_tracker%u %p (%" PRIu64 " unfreed, %" PRIu64 " freed, %" PRIu64 " bytes) %p\n", \
                   __LINE__, &CONCAT(leaklite_new_tracker,__LINE__), \
                   CONCAT(leaklite_new_tracker,__LINE__).active_allocs, \
                   CONCAT(leaklite_new_tracker,__LINE__).num_frees, \
                   CONCAT(leaklite_new_tracker,__LINE__).active_memsize, \
                   CONCAT(leaklite_new_tracker,__LINE__).next); \ */
#endif

void operator delete(void *ptr, const char *fname, const char *srcfile, uint32_t linenum) noexcept;
void operator delete[](void *ptr, const char *fname, const char *srcfile, uint32_t linenum) noexcept;
void operator delete(void *ptr) noexcept;
void operator delete[](void *ptr) noexcept;

#endif
#endif
