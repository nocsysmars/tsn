#ifndef SSET_H
#define SSET_H

#include "hmap.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sset_node {
    struct hmap_node hmap_node;
    char name[1];
};

/* A set of strings. */
struct sset {
    struct hmap map;
};

#define SSET_INITIALIZER(SSET) { HMAP_INITIALIZER(&(SSET)->map) }

/* Basics. */
void sset_init(struct sset *);
void sset_destroy(struct sset *);
void sset_clone(struct sset *, const struct sset *);
void sset_swap(struct sset *, struct sset *);
void sset_moved(struct sset *);

/* String parsing and formatting. */
void sset_from_delimited_string(struct sset *, const char *s,
                                const char *delimiters);
char *sset_join(const struct sset *,
                const char *delimiter, const char *terminator);

/* Count. */
bool sset_is_empty(const struct sset *);
size_t sset_count(const struct sset *);

/* Insertion. */
struct sset_node *sset_add(struct sset *, const char *);
struct sset_node *sset_add_and_free(struct sset *, char *);
void sset_add_assert(struct sset *, const char *);
void sset_add_array(struct sset *, char **, size_t n);

/* Deletion. */
void sset_clear(struct sset *);
void sset_delete(struct sset *, struct sset_node *);
bool sset_find_and_delete(struct sset *, const char *);
void sset_find_and_delete_assert(struct sset *, const char *);
char *sset_pop(struct sset *);

/* Search. */
struct sset_node *sset_find(const struct sset *, const char *);
bool sset_contains(const struct sset *, const char *);
bool sset_equals(const struct sset *, const struct sset *);

struct sset_position {
    struct hmap_position pos;
};

struct sset_node *sset_at_position(const struct sset *,
                                   struct sset_position *);

/* Set operations. */
void sset_intersect(struct sset *, const struct sset *);

/* Iteration macros. */
#define SSET_FOR_EACH(NAME, SSET)               \
    for ((NAME) = SSET_FIRST(SSET);             \
         NAME != NULL;                          \
         (NAME) = SSET_NEXT(SSET, NAME))

#define SSET_FOR_EACH_SAFE(NAME, NEXT, SSET)        \
    for ((NAME) = SSET_FIRST(SSET);                 \
         (NAME != NULL                              \
          ? (NEXT) = SSET_NEXT(SSET, NAME), true    \
          : false);                                 \
         (NAME) = (NEXT))

const char **sset_array(const struct sset *);
const char **sset_sort(const struct sset *);

/* Implementation helper macros. */

#define SSET_NODE_FROM_HMAP_NODE(HMAP_NODE) \
    CONTAINER_OF(HMAP_NODE, struct sset_node, hmap_node)
#define SSET_NAME_FROM_HMAP_NODE(HMAP_NODE) \
    HMAP_NODE == NULL                       \
    ? NULL                                  \
    : (CONST_CAST(const char *, (SSET_NODE_FROM_HMAP_NODE(HMAP_NODE)->name)))
#define SSET_NODE_FROM_NAME(NAME) CONTAINER_OF(NAME, struct sset_node, name)
#define SSET_FIRST(SSET)                                    \
    (BUILD_ASSERT_TYPE(SSET, struct sset *),                \
     SSET_NAME_FROM_HMAP_NODE(hmap_first(&(SSET)->map)))
#define SSET_NEXT(SSET, NAME)                                           \
    (BUILD_ASSERT_TYPE(SSET, struct sset *),                            \
     SSET_NAME_FROM_HMAP_NODE(                                          \
         hmap_next(&(SSET)->map, &SSET_NODE_FROM_NAME(NAME)->hmap_node)))

#ifdef __cplusplus
}
#endif

#endif /* sset.h */