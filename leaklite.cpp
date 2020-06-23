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

#include <new>
#include "util/leaklite.hpp"

leaklite_alloc_tracker_t *tracker_head = NULL;

#undef new
#undef delete

#if NO_LAMBDA_LEAKLITE
void * operator new(size_t size, const char *fname, leaklite_alloc_tracker_t *tracker)
#else
void * operator new(size_t size, const char *fname, leaklite_alloc_tracker_t *(*get_tracker)())
#endif
{
#if NO_LAMBDA_LEAKLITE
    return leaklite_new(size, NULL, tracker, NEW); 
#else
    return leaklite_new(size, NULL, get_tracker, NEW, fname);
#endif 
}

#if NO_LAMBDA_LEAKLITE
void * operator new[](size_t size, const char *fname, leaklite_alloc_tracker_t *tracker) 
#else
void * operator new[](size_t size, const char *fname, leaklite_alloc_tracker_t *(*get_tracker)())
#endif
{
#if NO_LAMBDA_LEAKLITE
    return leaklite_new(size, NULL, tracker, NEW_ARR); 
#else
    return leaklite_new(size, NULL, get_tracker, NEW_ARR, fname);
#endif 
}

#if NO_LAMBDA_LEAKLITE
void * operator new(size_t size, std::align_val_t al, const char *fname,
                    leaklite_alloc_tracker_t *tracker) 
#else
void * operator new(size_t size, std::align_val_t al, const char *fname,
                    leaklite_alloc_tracker_t *(*get_tracker)()) 
#endif
{
#if NO_LAMBDA_LEAKLITE
  return leaklite_new(size, &al, tracker, ALIGN_NEW); 
#else
  return leaklite_new(size, &al, get_tracker, ALIGN_NEW, fname);
#endif 
}

#if NO_LAMBDA_LEAKLITE
void * operator new[](size_t size, std::align_val_t al, const char *fname,
                      leaklite_alloc_tracker_t *tracker) 
#else
void * operator new[](size_t size, std::align_val_t al, const char *fname,
                      leaklite_alloc_tracker_t *(*get_tracker)()) 
#endif
{
#if NO_LAMBDA_LEAKLITE
  return leaklite_new(size, &al, tracker, ALIGN_NEW_ARR);
#else
  return leaklite_new(size, &al, get_tracker, ALIGN_NEW_ARR, fname);
#endif 
}

void operator delete(void *ptr, const char *fname, const char *srcfile, uint32_t linenum) noexcept
{
  leaklite_delete(ptr, fname, srcfile, linenum, NEW); 
}

void operator delete[](void *ptr, const char *fname, const char *srcfile, uint32_t linenum) noexcept
{
  leaklite_delete(ptr, fname, srcfile, linenum, NEW_ARR); 
}

void operator delete(void * ptr) noexcept
{
  leaklite_delete(ptr, NULL, NULL, 0, NEW); 
}

void operator delete[](void * ptr) noexcept
{
  leaklite_delete(ptr, NULL, NULL, 0, NEW_ARR); 
}
