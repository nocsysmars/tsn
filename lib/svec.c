#include "svec.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dynamic-string.h"
#include "util.h"

void
svec_init(struct svec *svec)
{
    svec->names = NULL;
    svec->n = 0;
    svec->allocated = 0;
}

void
svec_clone(struct svec *svec, const struct svec *other)
{
    svec_init(svec);
    svec_append(svec, other);
}

void
svec_destroy(struct svec *svec)
{
    svec_clear(svec);
    free(svec->names);
}

void
svec_clear(struct svec *svec)
{
    size_t i;

    for (i = 0; i < svec->n; i++) {
        free(svec->names[i]);
    }
    svec->n = 0;
}

bool
svec_is_empty(const struct svec *svec)
{
    return svec->n == 0;
}

void
svec_add(struct svec *svec, const char *name)
{
    svec_add_nocopy(svec, strdup(name));
}

void
svec_del(struct svec *svec, const char *name)
{
    size_t offset;

    offset = svec_find(svec, name);
    if (offset != SIZE_MAX) {
        free(svec->names[offset]);
        memmove(&svec->names[offset], &svec->names[offset + 1],
                sizeof *svec->names * (svec->n - offset - 1));
        svec->n--;
    }
}

static void
svec_expand(struct svec *svec)
{
    if (svec->n >= svec->allocated) {
        svec->names = x2nrealloc(svec->names, &svec->allocated,
                                 sizeof *svec->names);
    }
}

void
svec_add_nocopy(struct svec *svec, char *name)
{
    svec_expand(svec);
    svec->names[svec->n++] = name;
}

void
svec_append(struct svec *svec, const struct svec *other)
{
    size_t i;
    for (i = 0; i < other->n; i++) {
        svec_add(svec, other->names[i]);
    }
}

void
svec_terminate(struct svec *svec)
{
    svec_expand(svec);
    svec->names[svec->n] = NULL;
}

static int
compare_strings(const void *a_, const void *b_)
{
    char *const *a = a_;
    char *const *b = b_;
    return strcmp(*a, *b);
}

void
svec_sort(struct svec *svec)
{
    if (svec->n) {
        qsort(svec->names, svec->n, sizeof *svec->names, compare_strings);
    }
}

void
svec_sort_unique(struct svec *svec)
{
    svec_sort(svec);
    svec_unique(svec);
}

void
svec_unique(struct svec *svec)
{
    assert(svec_is_sorted(svec));
    if (svec->n > 1) {
        /* This algorithm is lazy and sub-optimal, but it's "obviously correct"
         * and asymptotically optimal . */
        struct svec tmp;
        size_t i;

        svec_init(&tmp);
        svec_add(&tmp, svec->names[0]);
        for (i = 1; i < svec->n; i++) {
            if (strcmp(svec->names[i - 1], svec->names[i])) {
                svec_add(&tmp, svec->names[i]);
            }
        }
        svec_swap(&tmp, svec);
        svec_destroy(&tmp);
    }
}

void
svec_compact(struct svec *svec)
{
    size_t i, j;

    for (i = j = 0; i < svec->n; i++) {
        if (svec->names[i] != NULL) {
            svec->names[j++] = svec->names[i];
        }
    }
    svec->n = j;
}

static void
swap_strings(char **a, char **b)
{
    char *tmp = *a;
    *a = *b;
    *b = tmp;
}

void
svec_diff(const struct svec *a, const struct svec *b,
          struct svec *a_only, struct svec *both, struct svec *b_only)
{
    size_t i, j;

    assert(svec_is_sorted(a));
    assert(svec_is_sorted(b));
    if (a_only) {
        svec_init(a_only);
    }
    if (both) {
        svec_init(both);
    }
    if (b_only) {
        svec_init(b_only);
    }
    for (i = j = 0; i < a->n && j < b->n; ) {
        int cmp = strcmp(a->names[i], b->names[j]);
        if (cmp < 0) {
            if (a_only) {
                svec_add(a_only, a->names[i]);
            }
            i++;
        } else if (cmp > 0) {
            if (b_only) {
                svec_add(b_only, b->names[j]);
            }
            j++;
        } else {
            if (both) {
                svec_add(both, a->names[i]);
            }
            i++;
            j++;
        }
    }
    if (a_only) {
        for (; i < a->n; i++) {
            svec_add(a_only, a->names[i]);
        }
    }
    if (b_only) {
        for (; j < b->n; j++) {
            svec_add(b_only, b->names[j]);
        }
    }
}

bool
svec_contains(const struct svec *svec, const char *name)
{
    return svec_find(svec, name) != SIZE_MAX;
}

size_t
svec_find(const struct svec *svec, const char *name)
{
    char **p;

    assert(svec_is_sorted(svec));
    p = bsearch(&name, svec->names, svec->n, sizeof *svec->names,
                compare_strings);
    return p ? p - svec->names : SIZE_MAX;
}

bool
svec_is_sorted(const struct svec *svec)
{
    size_t i;

    for (i = 1; i < svec->n; i++) {
        if (strcmp(svec->names[i - 1], svec->names[i]) > 0) {
            return false;
        }
    }
    return true;
}

bool
svec_is_unique(const struct svec *svec)
{
    return svec_get_duplicate(svec) == NULL;
}

const char *
svec_get_duplicate(const struct svec *svec)
{
    assert(svec_is_sorted(svec));
    if (svec->n > 1) {
        size_t i;
        for (i = 1; i < svec->n; i++) {
            if (!strcmp(svec->names[i - 1], svec->names[i])) {
                return svec->names[i];
            }
        }
    }
    return NULL;
}

void
svec_swap(struct svec *a, struct svec *b)
{
    struct svec tmp = *a;
    *a = *b;
    *b = tmp;
}

void
svec_print(const struct svec *svec, const char *title)
{
    size_t i;

    printf("%s:\n", title);
    for (i = 0; i < svec->n; i++) {
        printf("\"%s\"\n", svec->names[i]);
    }
}

bool
svec_equal(const struct svec *a, const struct svec *b)
{
    size_t i;

    if (a->n != b->n) {
        return false;
    }
    for (i = 0; i < a->n; i++) {
        if (strcmp(a->names[i], b->names[i])) {
            return false;
        }
    }
    return true;
}

char *
svec_join(const struct svec *svec,
          const char *delimiter, const char *terminator)
{
    struct ds ds;
    size_t i;

    ds_init(&ds);
    for (i = 0; i < svec->n; i++) {
        if (i) {
            ds_put_cstr(&ds, delimiter);
        }
        ds_put_cstr(&ds, svec->names[i]);
    }
    ds_put_cstr(&ds, terminator);
    return ds_cstr(&ds);
}

const char *
svec_back(const struct svec *svec)
{
    assert(svec->n);
    return svec->names[svec->n - 1];
}

void
svec_pop_back(struct svec *svec)
{
    assert(svec->n);
    free(svec->names[--svec->n]);
}
