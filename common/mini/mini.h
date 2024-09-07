/**
 ** This file is part of the minic project.
 ** Copyright 2023 univrsal <uni@vrsal.xyz>.
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are
 ** met:
 **
 ** 1. Redistributions of source code must retain the above copyright notice,
 **    this list of conditions and the following disclaimer.
 **
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 ** EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 ** DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 ** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 ** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 ** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 ** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 ** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 ** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 ** SUCH DAMAGE.
 **/

#ifndef MINI_C_H
#define MINI_C_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#include <stdio.h>

#ifndef MINI_CHUNK_SIZE
#define MINI_CHUNK_SIZE 1024
#endif

enum mini_result {
    MINI_OK,
    MINI_INVALID_ARG,
    MINI_INVALID_PATH,
    MINI_INVALID_TYPE,
    MINI_INVALID_ID,
    MINI_DUPLICATE_ID,
    MINI_FILE_NOT_FOUND,
    MINI_GROUP_NOT_FOUND,
    MINI_VALUE_NOT_FOUND,
    MINI_ACCESS_DENIED,
    MINI_READ_ERROR,
    MINI_CONVERSION_ERROR,
    /* Flag errors, will occur independently of the above errors */
    MINI_INVALID_GROUP = 1 << 4,
    MINI_UNKNOWN
};

enum mini_flags {
    MINI_FLAGS_NONE = 0,
    MINI_FLAGS_SKIP_EMPTY_GROUPS = 1 << 0,
};

typedef struct mini_value_s {
    char *id;                  /* The id of this item                  */
    char *val;                 /* The value for this item              */
    struct mini_value_s *next; /* The next value in this group         */
    struct mini_value_s *prev;
} mini_value_t;

typedef struct mini_group_s {
    char *id;                  /* The id of this group                 */
    struct mini_group_s *next; /* The next group on the same level     */
    struct mini_group_s *prev;
    mini_value_t *head;        /* The first value for this group       */
    mini_value_t *tail;
} mini_group_t;

typedef struct mini_s {
    char *path;
    mini_group_t *head;
    mini_group_t *tail;
} mini_t;

EXPORT mini_t *mini_create(const char *path);

/* Load from FILE instance, you will have to set path in the returned struct
 * manually otherwise mini_save will not work */
/* Loading with optional error code, can be NULL,
 * will contain either MINI_OK, or any MINI_* error*/
EXPORT mini_t *mini_load_ex(const char *path, int *err);

EXPORT mini_t *mini_loadf_ex(FILE *f, int *err);

EXPORT mini_t *mini_try_load_ex(const char *path, int *err);

#if WIN32
#include <Windows.h>
#include <tchar.h>
EXPORT wchar_t *mini_utf8_to_wide_char(const char *utf8);
EXPORT char *mini_utf8_from_wide_char(const wchar_t *ws);
EXPORT mini_t *mini_wcreate(const wchar_t *path);
EXPORT mini_t *mini_wload_ex(const wchar_t *path, int *err);
EXPORT mini_t *mini_wloadf_ex(FILE *f, int *err);
EXPORT mini_t *mini_wtry_load_ex(const wchar_t *path, int *err);
EXPORT int mini_set_wstring(mini_t *mini, const char *group, const char *id, const wchar_t *val);

#endif

/* Load or return an empty file if it doesn't exist */
static inline mini_t *mini_try_load(const char *path) {
    return mini_try_load_ex(path, NULL);
}

/* Load from path */
static inline mini_t *mini_load(const char *path) {
    return mini_load_ex(path, NULL);
}

static inline mini_t *mini_loadf(FILE *f) {
    return mini_loadf_ex(f, NULL);
}

EXPORT int mini_save(const mini_t *mini, int flags);

EXPORT int mini_savef(const mini_t *mini, FILE *f, int flags);

EXPORT void mini_free(mini_t *mini);

EXPORT int mini_value_exists(mini_t *mini, const char *group, const char *id);

static inline int mini_empty(const mini_t *mini) {
    return !mini || !mini->head || !(mini->head->head || mini->head->next);
}

/* Data creation/retrival/deleting
 * For all set/get methods group can be NULL
 * which will then use the root group.
 */

EXPORT int mini_delete_value(mini_t *mini, const char *group, const char *id);

EXPORT int mini_delete_group(mini_t *mini, const char *group);

EXPORT int mini_set_string(mini_t *mini, const char *group, const char *id, const char *val);

EXPORT int mini_set_int(mini_t *mini, const char *group, const char *id, long long val);

EXPORT int mini_set_double(mini_t *mini, const char *group, const char *id, double val);

#define mini_set_bool(m, g, i, v) mini_set_int(m, g, i, v)

EXPORT const char *mini_get_string_ex(mini_t *mini, const char *group, const char *id, const char *fallback, int *err);

EXPORT long long mini_get_int_ex(mini_t *mini, const char *group, const char *id, long long fallback, int *err);

EXPORT double mini_get_double_ex(mini_t *mini, const char *group, const char *id, double fallback, int *err);

#define mini_get_bool(m, g, i, v) mini_get_int(m, g, i, v)
#define mini_get_bool_ex(m, g, i, v, e) mini_get_int_ex(m, g, i, v, e)

static inline const char *mini_get_string(mini_t *mini, const char *group, const char *id, const char *fallback) {
    return mini_get_string_ex(mini, group, id, fallback, NULL);
}

static inline long long mini_get_int(mini_t *mini, const char *group, const char *id, long long fallback) {
    return mini_get_int_ex(mini, group, id, fallback, NULL);
}

static inline double mini_get_double(mini_t *mini, const char *group, const char *id, double fallback) {
    return mini_get_double_ex(mini, group, id, fallback, NULL);
}

#ifdef __cplusplus
}
#endif

#endif
