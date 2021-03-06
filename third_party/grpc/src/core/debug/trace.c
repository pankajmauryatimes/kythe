/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
 *
 */

#include "src/core/debug/trace.h"

#include <string.h>

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include "src/core/support/env.h"

typedef struct tracer {
  const char *name;
  int *flag;
  struct tracer *next;
} tracer;
static tracer *tracers;

void grpc_register_tracer(const char *name, int *flag) {
  tracer *t = gpr_malloc(sizeof(*t));
  t->name = name;
  t->flag = flag;
  t->next = tracers;
  *flag = 0;
  tracers = t;
}

static void add(const char *beg, const char *end, char ***ss, size_t *ns) {
  size_t n = *ns;
  size_t np = n + 1;
  char *s = gpr_malloc(end - beg + 1);
  memcpy(s, beg, end - beg);
  s[end-beg] = 0;
  *ss = gpr_realloc(*ss, sizeof(char**) * np);
  (*ss)[n] = s;
  *ns = np;
}

static void split(const char *s, char ***ss, size_t *ns) {
  const char *c = strchr(s, ',');
  if (c == NULL) {
    add(s, s + strlen(s), ss, ns);
  } else {
    add(s, c, ss, ns);
    split(c+1, ss, ns);
  }
}

static void parse(const char *s) {
  char **strings = NULL;
  size_t nstrings = 0;
  size_t i;
  tracer *t;
  split(s, &strings, &nstrings);

  for (i = 0; i < nstrings; i++) {
    const char *s = strings[i];
    if (0 == strcmp(s, "all")) {
      for (t = tracers; t; t = t->next) {
        *t->flag = 1;
      }
    } else {
      int found = 0;
      for (t = tracers; t; t = t->next) {
        if (0 == strcmp(s, t->name)) {
          *t->flag = 1;
          found = 1;
        }
      }
      if (!found) {
        gpr_log(GPR_ERROR, "Unknown trace var: '%s'", s);
      }
    }
  }

  for (i = 0; i < nstrings; i++) {
    gpr_free(strings[i]);
  }
  gpr_free(strings);
}

void grpc_tracer_init(const char *env_var) {
  char *e = gpr_getenv(env_var);
  if (e != NULL) {
    parse(e);
    gpr_free(e);
  }
  while (tracers) {
    tracer *t = tracers;
    tracers = t->next;
    gpr_free(t);
  }
}
