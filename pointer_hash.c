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

#include <stdlib.h>
#include <pthread.h>
#include "util/pointer_hash.h"

static bool initialized = false;

static ck_ht_t pointer_hash;
static pthread_mutex_t ht_mutex =  PTHREAD_MUTEX_INITIALIZER;

void *pointer_hash_malloc(size_t s) {
  return malloc(s);
}

void *pointer_hash_realloc(void *p, size_t s_old, size_t s_new, bool d) {
  return realloc(p, s_new);
}

void pointer_hash_free(void *p, size_t s, bool d) {
  free(p);
  return;
}

struct ck_malloc pointer_hash_allocator = {
  .malloc = pointer_hash_malloc,
  .realloc = pointer_hash_realloc,
  .free = pointer_hash_free
};

void pointer_hash_function(ck_ht_hash_t *object, const void *key, size_t key_len, uint64_t seed) { 
  *(uint64_t *)object = (uint64_t)(*(unsigned long *)(key) >> 3);
}

bool pointer_hash_init() {
  if (initialized) { return true; }
  initialized = true;
  return ck_ht_init(&pointer_hash, CK_HT_MODE_DIRECT | CK_HT_WORKLOAD_DELETE, pointer_hash_function,
                    &pointer_hash_allocator, 50, 23498279);
}

bool pointer_hash_insert(const void *key, const uint64_t value) {
  if (!initialized) { pointer_hash_init(); }
  uint64_t *val_ptr = (uint64_t *)malloc(sizeof(uint64_t));
  *val_ptr = value;
  ck_ht_hash_t hash_value; 
  ck_ht_hash_direct(&hash_value, &pointer_hash, (uintptr_t)key);
  struct ck_ht_entry entry;
  ck_ht_entry_set_direct(&entry, hash_value, (uintptr_t)key, (uintptr_t)val_ptr);
  // ck_ht is SPMC, so needs lock on writing
  pthread_mutex_lock(&ht_mutex);
  bool result = ck_ht_put_spmc(&pointer_hash, hash_value, &entry);
  // this shouldn't happen but we want to try to recover, but still return false
  if (!result) {
    ck_ht_set_spmc(&pointer_hash, hash_value, &entry);
  }
  pthread_mutex_unlock(&ht_mutex);
  return result;
}

uint64_t *pointer_hash_get(const void *key) {
  if (!initialized) { pointer_hash_init(); }
  ck_ht_hash_t hash_value; 
  ck_ht_hash_direct(&hash_value, &pointer_hash, (uintptr_t)key);
  struct ck_ht_entry entry;
  ck_ht_entry_key_set_direct(&entry, (uintptr_t)key);
  if (!ck_ht_get_spmc(&pointer_hash, hash_value, &entry)) {
    return NULL;
  }
  return (uint64_t *)ck_ht_entry_value_direct(&entry);
}

bool pointer_hash_remove(const void *key) {
  ck_ht_hash_t hash_value; 
  ck_ht_hash_direct(&hash_value, &pointer_hash, (uintptr_t)key);
  struct ck_ht_entry entry;
  ck_ht_entry_key_set_direct(&entry, (uintptr_t)key);
  // ck_ht is SPMC, so needs lock on writing
  pthread_mutex_lock(&ht_mutex);
  bool result = ck_ht_remove_spmc(&pointer_hash, hash_value, &entry);
  pthread_mutex_unlock(&ht_mutex);
  if (result == true) {
    free((void *)ck_ht_entry_value_direct(&entry));
  }
  return result;
}

void pointer_hash_destroy() {
  ck_ht_destroy(&pointer_hash);
}
