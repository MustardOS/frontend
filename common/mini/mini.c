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

#include "mini.h"
#include <malloc.h>
#include <sys/stat.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

/* === UTF8 <-> Wchar === */
#ifdef _WIN32

static char *mini_strdup(const char *str)
{
    return _strdup(str);
}

#define mini_strtok strtok_s

wchar_t *mini_utf8_to_wide_char(const char *utf8)
{
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (!len) {
        return NULL;
    }

    wchar_t *wide = malloc(len * sizeof(wchar_t));
    if (!wide) {
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

char *mini_utf8_from_wide_char(const wchar_t *ws)
{
    const int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (!len) {
        return NULL;
    }

    char *utf8 = malloc(len);
    if (!utf8) {
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, ws, -1, utf8, len, NULL, NULL);
    return utf8;
}

int mini_set_wstring(mini_t *mini, const char *group, const char *id, const wchar_t *val)
{
    const char *utf8 = mini_utf8_from_wide_char(val);
    const int ret = mini_set_string(mini, group, id, utf8);
    free(utf8);
    return ret;
}

#else

static inline char *mini_strdup(const char *str) {
    return strdup(str);
}

#define mini_strtok strtok_r
#endif

/* === Utilities === */

mini_value_t *make_value() {
    mini_value_t *val = malloc(sizeof(mini_value_t));
    val->id = NULL;
    val->val = NULL;
    val->next = NULL;
    val->prev = NULL;
    return val;
}

void free_value(mini_value_t *v) {
    if (v) {
        free(v->id);
        free(v->val);
        v->id = NULL;
        v->val = NULL;
        v->next = NULL;
        v->prev = NULL;
        free(v);
    }
}

mini_group_t *make_group(const char *name) {
    mini_group_t *g = malloc(sizeof(mini_group_t));
    memset(g, 0, sizeof(mini_group_t));
    if (name)
        g->id = mini_strdup(name);
    return g;
}

void free_group(mini_group_t *g) {
    if (g) {
        free(g->id);
        g->next = NULL;
        g->prev = NULL;
        g->id = NULL;
        g->head = NULL;
        free(g);
    }
}

void free_group_children(mini_group_t *g) {
    if (!g)
        return;

    mini_group_t *cgrp = g, *ngrp = NULL;
    mini_value_t *cval = NULL, *nval = NULL;

    while (cgrp) {
        cval = cgrp->head;
        while (cval) {
            nval = cval->next;
            free_value(cval);
            cval = nval;
        }

        ngrp = cgrp->next;
        free_group(cgrp);
        cgrp = ngrp;
    }
}

mini_value_t *get_group_value(mini_group_t *grp, const char *id) {
    mini_value_t *result = grp->head;
    while (result) {
        if (strcmp(result->id, id) == 0)
            return result;
        result = result->next;
    }
    return NULL;
}

int add_value(mini_group_t *group, const char *id, const char *val) {
    if (get_group_value(group, id))
        return MINI_DUPLICATE_ID;

    mini_value_t *n = make_value();
    n->id = mini_strdup(id);
    n->val = mini_strdup(val);
    n->next = group->head;

    /* If this is the first value added to this group
     * we set the tail pointer to this first value */
    if (group->head == NULL)
        group->tail = n;
    if (group->head)
        group->head->prev = n;
    group->head = n;

    return MINI_OK;
}

int parse_value(mini_group_t *group, char *line) {
    char *ctx1 = NULL, *ctx2 = NULL;
    char *id = mini_strtok(line, "=", &ctx1);

    if (strlen(id) < 1)
        return MINI_INVALID_ID;
    if (get_group_value(group, id))
        return MINI_DUPLICATE_ID;

    char *val = mini_strtok(ctx1, "", &ctx2);
    if (val && strlen(val) > 0)
        val[strlen(val) - 1] = '\0'; /* Get rid of new line */
    return add_value(group, id, val);
}

void add_group(mini_t *mini, mini_group_t *grp) {
    if (grp->id == NULL) { /* The root group should always be first */
        grp->next = mini->head;
        grp->prev = NULL;
        if (mini->head)
            mini->head->prev = grp;
        mini->head = grp;
    } else {
        grp->next = NULL;
        grp->prev = mini->tail;
        if (mini->tail)
            mini->tail->next = grp;
        mini->tail = grp;
    }
}

mini_group_t *create_group(mini_t *mini, const char *name) {
    mini_group_t *n = NULL;
    n = make_group(name);
    add_group(mini, n);
    return n;
}

mini_group_t *get_group(mini_t *mini, char *id, int create) {
    if (!id)
        return mini->head;

    mini_group_t *c = mini->head->next;

    /* Go over all groups on this level */
    while (c) {
        if (strcmp(c->id, id) == 0)
            break;
        c = c->next;
    }

    /* Didn't find any group */
    if (!c && create)
        return create_group(mini, id);
    return c;
}

mini_value_t *get_value(mini_t *mini, const char *group, const char *id, int *err, mini_group_t **group_ptr) {
    char *tmp = NULL;
    mini_value_t *result = NULL;
    if (group)
        tmp = mini_strdup(group);
    mini_group_t *grp = get_group(mini, tmp, 0);

    if (grp) {
        if (group_ptr)
            *group_ptr = grp;
        result = get_group_value(grp, id);
        if (!result && err)
            *err = MINI_VALUE_NOT_FOUND;
    } else if (err) {
        *err = MINI_GROUP_NOT_FOUND;
    }

    free(tmp);
    return result;
}

int write_group(const mini_group_t *g, FILE *f, int flags) {
    int wrote_something = 0;
    mini_value_t *cval = g->tail;

    if (cval == NULL && flags & MINI_FLAGS_SKIP_EMPTY_GROUPS)
        return 0;

    /* Root group doesn't have a header so it'll skip this */
    if (g->prev) {
        fprintf(f, "[%s]\n", g->id);
        wrote_something = 1;
    }

    /* Write all values of this group */
    while (cval) {
        fprintf(f, "%s=%s\n", cval->id, cval->val);
        cval = cval->prev;
        wrote_something = 1;
    }
    return wrote_something;
}

/* === API implementation === */

#if WIN32
mini_t *mini_wcreate(const wchar_t *path)
{
    mini_t *result = malloc(sizeof(mini_t));
    if (path)
        result->path = mini_utf8_from_wide_char(path);
    result->head = make_group(NULL);
    result->tail = result->head;
    return result;
}

mini_t *mini_wtry_load_ex(const wchar_t *path, int *err)
{
    mini_t *result = mini_wload_ex(path, err);

    if (!result)
        result = mini_wcreate(path);
    return result;
}

mini_t *mini_wload_ex(const wchar_t *path, int *err)
{
    mini_t *result = NULL;
    struct _stat buf;

    if (_wstat(path, &buf) != 0) {
        if (err)
            *err = MINI_FILE_NOT_FOUND;
    } else {
        FILE *fp = NULL;
        _wfopen_s(&fp, path, L"r");

        if (fp) {
            result = mini_loadf(fp);
            result->path = mini_utf8_from_wide_char(path);
            fclose(fp);
        } else if (err) {
            *err = MINI_ACCESS_DENIED;
        }
    }

    return result;
}
#endif

mini_t *mini_create(const char *path) {
    mini_t *result = malloc(sizeof(mini_t));
    if (path)
        result->path = mini_strdup(path);
    result->head = make_group(NULL);
    result->tail = result->head;
    return result;
}

mini_t *mini_try_load_ex(const char *path, int *err) {
    mini_t *result = mini_load_ex(path, err);

    if (!result)
        result = mini_create(path);
    return result;
}

mini_t *mini_load_ex(const char *path, int *err) {
    mini_t *result = NULL;
    struct stat buf;

    if (stat(path, &buf) != 0) {
        if (err)
            *err = MINI_FILE_NOT_FOUND;
    } else {
        FILE *fp = NULL;
#if WIN32
        fopen_s(&fp, path, "r");
#else
        fp = fopen(path, "r");
#endif

        if (fp) {
            result = mini_loadf(fp);
            result->path = mini_strdup(path);
            fclose(fp);
        } else if (err) {
            *err = MINI_ACCESS_DENIED;
        }
    }

    return result;
}

mini_t *mini_loadf_ex(FILE *f, int *err) {
    mini_t *result = mini_create(NULL);
    mini_group_t *current;
    char buffer[MINI_CHUNK_SIZE];

    current = result->head;

    while (fgets(buffer, sizeof(buffer), f)) {
        buffer[MINI_CHUNK_SIZE - 1] = '\0';

        if (strlen(buffer) < 1 || buffer[0] == '\n') {
            continue;
        } else if (buffer[0] == '[') {
            /* Group header */
            buffer[strlen(buffer) - 2] = '\0';                  /* Remove ']\n' */
            mini_group_t *n = get_group(result, buffer + 1, 1); /* Skip '[' */

            if (n)
                current = n;
            else if (err)
                *err |= MINI_INVALID_GROUP;
        } else {
            parse_value(current, buffer);
        }
    }

    return result;
}

int mini_save(const mini_t *mini, int flags) {
    int result = MINI_OK;

    if (!mini->path || strlen(mini->path) < 1) {
        result = MINI_INVALID_PATH;
    } else {
        FILE *fp = NULL;
#if WIN32
        _wfopen_s(&fp, mini_utf8_to_wide_char(mini->path), L"w");
#else
        fp = fopen(mini->path, "w");
#endif

        if (fp) {
            result = mini_savef(mini, fp, flags);
            fclose(fp);
        } else {
            result = MINI_ACCESS_DENIED;
        }
    }

    return result;
}

int mini_savef(const mini_t *mini, FILE *f, int flags) {
    if (!f)
        return MINI_INVALID_ARG;

    mini_group_t *grp = mini->head;

    while (grp) {
        int wrote_something = write_group(grp, f, flags);
        grp = grp->next;
        /* Unless this is the last group, add an empty line
         * to separate groups */
        if (grp && wrote_something)
            fprintf(f, "\n");
    }
    return MINI_OK;
}

void mini_free(mini_t *mini) {
    if (mini) {
        free(mini->path);
        free_group_children(mini->head);
        mini->path = NULL;
        mini->tail = NULL;
        free(mini);
    }
}

int mini_delete_value(mini_t *mini, const char *group, const char *id) {
    if (!mini || !id)
        return MINI_INVALID_ARG;
    int result = MINI_OK;
    mini_group_t *grp = NULL;
    mini_value_t *v = get_value(mini, group, id, &result, &grp);

    if (v) {
        if (v->next)
            v->next->prev = v->prev;
        if (v->prev)
            v->prev->next = v->next;
        if (v == grp->head)
            grp->head = v->next;
        if (v == grp->tail)
            grp->tail = v->prev;
        free_value(v);
    }
    return result;
}

int mini_delete_group(mini_t *mini, const char *group) {
    if (!mini)
        return MINI_INVALID_ARG;
    int result = MINI_OK;
    char *tmp = mini_strdup(group);
    mini_group_t *grp = get_group(mini, tmp, 0);
    free(tmp);

    if (grp) {
        free_group_children(grp);
        if (grp->next)
            grp->next->prev = grp->prev;
        if (grp->prev)
            grp->prev->next = grp->next;
        free_group(grp);
    } else {
        result = MINI_GROUP_NOT_FOUND;
    }
    return result;
}

int mini_value_exists(mini_t *mini, const char *group, const char *id) {
    if (!mini || !id)
        return MINI_INVALID_ARG;
    int result = MINI_OK;
    get_value(mini, group, id, &result, NULL);
    return result;
}

int mini_set_string(mini_t *mini, const char *group, const char *id, const char *val) {
    if (!mini || !id)
        return MINI_INVALID_ARG;
    int result = MINI_OK;
    mini_group_t *grp = NULL;
    mini_value_t *v = get_value(mini, group, id, &result, &grp);

    if (v) {
        free(v->val);
        v->val = mini_strdup(val);
    } else {
        if (!grp)
            grp = create_group(mini, group);
        result = add_value(grp, id, val);
    }

    return result;
}

int mini_set_int(mini_t *mini, const char *group, const char *id, long long val) {
    char buf[57];
    snprintf(buf, 56, "%lli", val);
    return mini_set_string(mini, group, id, buf);
}

int mini_set_double(mini_t *mini, const char *group, const char *id, double val) {
    char buf[64];
    snprintf(buf, 56, "%lf", val);
    return mini_set_string(mini, group, id, buf);
}

const char *mini_get_string_ex(mini_t *mini, const char *group, const char *id, const char *fallback, int *err) {
    if (!mini || !id)
        return fallback;
    const char *result = fallback;

    mini_value_t *v = get_value(mini, group, id, err, NULL);

    if (v)
        result = v->val;

    return result;
}

long long mini_get_int_ex(mini_t *mini, const char *group, const char *id, long long fallback, int *err) {
    const char *val = mini_get_string_ex(mini, group, id, NULL, err);
    if (!val)
        return fallback;
    long long res = strtoimax(val, NULL, 10);
    if ((res == INTMAX_MAX || res == INTMAX_MIN) && errno == ERANGE && err) {
        *err = MINI_CONVERSION_ERROR;
        res = fallback;
    }
    return res;
}

double mini_get_double_ex(mini_t *mini, const char *group, const char *id, double fallback, int *err) {
    const char *val = mini_get_string_ex(mini, group, id, NULL, err);
    if (!val)
        return fallback;
    double res = 0;
#if WIN32
    sscanf_s(val, "%lf", &res);
#else
    sscanf(val, "%lf", &res);
#endif

    return res;
}
