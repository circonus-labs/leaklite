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

extern "C" {
// NOTE: This source file is dependent on the Mt. Everest library (https://github.com/circonus-labs/libmtev)
// You can use it in your project or modify this file to fit your REST API or other mechanism to access real-time allocation data
#include <mtev_rest.h>
#include <mtev_http.h>
#include "util/circ_util.h"
}
#include "util/leaklite.hpp"

static int rest_get_leaklite_dump(mtev_http_rest_closure_t *restc, int npats, char **pats)
{
  mtev_http_session_ctx *ctx = restc->http_ctx;
  mtev_http_response_ok(ctx, "text/html");
  mtev_http_response_append(ctx, CIRC_STR_THEN_STRSIZE("<html><head><meta http-equiv=\"refresh\" content=\"5\"></head><body><h3>IRONDB LEAKLITE MEMORY DUMP<h3><table><tr><th>Bytes</th><th>Unfreed</th><th>Freed</th><th>Type</th><th>Function</th><th>Source File/Line</th></tr>\n"));
  leaklite_alloc_tracker_t *curr = tracker_head;
  uint64_t total = 0;
  while (curr) {
    if (curr->active_memsize > 1048576) {
      mtev_http_response_appendf(ctx,
                                "<tr><code><td align=\"right\">%10" PRIu64 "</td><td align=\"right\">%10" PRIu64 "</td><td align=\"right\">%10" PRIu64
                                "</td><td align=\"center\">%s</td><td align=\"center\">%s</td><td align=\"center\">%s:%u</td></code></tr>",
                                curr->active_memsize, curr->active_allocs, curr->num_frees,
                                leaklite_type_str[curr->type], curr->fname,
                                curr->srcfile, curr->linenum);
    }
    total = total + curr->active_memsize;
    curr = curr->next;
  }
  mtev_http_response_appendf(ctx, "<tr><code><td colspan=\"6\">%.10" PRIu64 " total monitored allocated memory</td></code></tr>", total);
  mtev_http_response_appendf(ctx, "<tr><td colspan=\"6\"></td></tr>");
  curr = tracker_head;
  while (curr) {
    if (curr->active_memsize <= 1048576) {
      mtev_http_response_appendf(ctx,
                                "<tr><code><td align=\"right\">%10" PRIu64 "</td><td align=\"right\">%10" PRIu64 "</td><td align=\"right\">%10" PRIu64
                                "</td><td align=\"center\">%s</td><td align=\"center\">%s</td><td align=\"center\">%s:%u</td></code></tr>",
                                curr->active_memsize, curr->active_allocs, curr->num_frees,
                                leaklite_type_str[curr->type], curr->fname,
                                curr->srcfile, curr->linenum);
    }
    curr = curr->next;
  }
  mtev_http_response_appendf(ctx, "</table></body></html>");

  mtev_http_response_end(ctx);
  return 0;
}

extern "C" {
void rest_leaklite_init()
{
  mtevAssert(mtev_http_rest_register("GET", "/", "^leaklite$", rest_get_leaklite_dump) == 0);
}
}
