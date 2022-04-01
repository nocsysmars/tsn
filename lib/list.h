/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2013, 2015, 2016 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIST_H
#define LIST_H 1

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "types.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Doubly linked list head or element. */
struct list_node {
    struct list_node *prev;     /* Previous list element. */
    struct list_node *next;     /* Next list element. */
};

#define LIST_INITIALIZER(LIST) { LIST, LIST }

/* "struct list_node" with pointers that will (probably) cause segfaults if
 * dereferenced and, better yet, show up clearly in a debugger.

 * MSVC2015 doesn't support designated initializers when compiling C++,
 * and doesn't support ternary operators with non-designated initializers.
 * So we use these static definitions rather than using initializer macros. */
static const struct list_node LIST_POISON =
    { (struct list_node *) (UINTPTR_MAX / 0xf * 0xc),
      (struct list_node *) (UINTPTR_MAX / 0xf * 0xc) };

static inline void list_init(struct list_node *);
static inline void list_poison(struct list_node *);

/* List insertion. */
static inline void list_insert(struct list_node *, struct list_node *);
static inline void list_splice(struct list_node *before, struct list_node *first,
                               struct list_node *last);
static inline void list_push_front(struct list_node *, struct list_node *);
static inline void list_push_back(struct list_node *, struct list_node *);
static inline void list_replace(struct list_node *, const struct list_node *);
static inline void list_moved(struct list_node *, const struct list_node *orig);
static inline void list_move(struct list_node *dst, struct list_node *src);

/* List removal. */
static inline struct list_node *list_remove(struct list_node *);
static inline struct list_node *list_pop_front(struct list_node *);
static inline struct list_node *list_pop_back(struct list_node *);

/* List elements. */
static inline struct list_node *list_front(const struct list_node *);
static inline struct list_node *list_back(const struct list_node *);

/* List properties. */
static inline size_t list_size(const struct list_node *);
static inline bool list_is_empty(const struct list_node *);
static inline bool list_is_singleton(const struct list_node *);
static inline bool list_is_short(const struct list_node *);

#define LIST_FOR_EACH(ITER, MEMBER, LIST)                               \
    for (INIT_CONTAINER(ITER, (LIST)->next, MEMBER);                    \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.next, MEMBER))
#define LIST_FOR_EACH_CONTINUE(ITER, MEMBER, LIST)                      \
    for (ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.next, MEMBER);             \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.next, MEMBER))
#define LIST_FOR_EACH_REVERSE(ITER, MEMBER, LIST)                       \
    for (INIT_CONTAINER(ITER, (LIST)->prev, MEMBER);                    \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.prev, MEMBER))
#define LIST_FOR_EACH_REVERSE_SAFE(ITER, PREV, MEMBER, LIST)        \
    for (INIT_CONTAINER(ITER, (LIST)->prev, MEMBER);                \
         (&(ITER)->MEMBER != (LIST)                                 \
          ? INIT_CONTAINER(PREV, (ITER)->MEMBER.prev, MEMBER), 1    \
          : 0);                                                     \
         (ITER) = (PREV))
#define LIST_FOR_EACH_REVERSE_CONTINUE(ITER, MEMBER, LIST)              \
    for (ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.prev, MEMBER);           \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.prev, MEMBER))
#define LIST_FOR_EACH_SAFE(ITER, NEXT, MEMBER, LIST)               \
    for (INIT_CONTAINER(ITER, (LIST)->next, MEMBER);               \
         (&(ITER)->MEMBER != (LIST)                                \
          ? INIT_CONTAINER(NEXT, (ITER)->MEMBER.next, MEMBER), 1   \
          : 0);                                                    \
         (ITER) = (NEXT))
#define LIST_FOR_EACH_POP(ITER, MEMBER, LIST)                      \
    while (!list_is_empty(LIST)                                    \
           && (INIT_CONTAINER(ITER, list_pop_front(LIST), MEMBER), 1))

/* Inline implementations. */

/* Initializes 'list' as an empty list. */
static inline void
list_init(struct list_node *list)
{
    list->next = list->prev = list;
}

/* Initializes 'list' with pointers that will (probably) cause segfaults if
 * dereferenced and, better yet, show up clearly in a debugger. */
static inline void
list_poison(struct list_node *list)
{
    *list = LIST_POISON;
}

/* Inserts 'elem' just before 'before'. */
static inline void
list_insert(struct list_node *before, struct list_node *elem)
{
    elem->prev = before->prev;
    elem->next = before;
    before->prev->next = elem;
    before->prev = elem;
}

/* Removes elements 'first' though 'last' (exclusive) from their current list,
   then inserts them just before 'before'. */
static inline void
list_splice(struct list_node *before, struct list_node *first, struct list_node *last)
{
    if (first == last) {
        return;
    }
    last = last->prev;

    /* Cleanly remove 'first'...'last' from its current list. */
    first->prev->next = last->next;
    last->next->prev = first->prev;

    /* Splice 'first'...'last' into new list. */
    first->prev = before->prev;
    last->next = before;
    before->prev->next = first;
    before->prev = last;
}

/* Inserts 'elem' at the beginning of 'list', so that it becomes the front in
   'list'. */
static inline void
list_push_front(struct list_node *list, struct list_node *elem)
{
    list_insert(list->next, elem);
}

/* Inserts 'elem' at the end of 'list', so that it becomes the back in
 * 'list'. */
static inline void
list_push_back(struct list_node *list, struct list_node *elem)
{
    list_insert(list, elem);
}

/* Puts 'elem' in the position currently occupied by 'position'.
 * Afterward, 'position' is not part of a list. */
static inline void
list_replace(struct list_node *element, const struct list_node *position)
{
    element->next = position->next;
    element->next->prev = element;
    element->prev = position->prev;
    element->prev->next = element;
}

/* Adjusts pointers around 'list' to compensate for 'list' having been moved
 * around in memory (e.g. as a consequence of realloc()), with original
 * location 'orig'.
 *
 * ('orig' likely points to freed memory, but this function does not
 * dereference 'orig', it only compares it to 'list'.  In a very pedantic
 * language lawyer sense, this still yields undefined behavior, but it works
 * with actual compilers.) */
static inline void
list_moved(struct list_node *list, const struct list_node *orig)
{
    if (list->next == orig) {
        list_init(list);
    } else {
        list->prev->next = list->next->prev = list;
    }
}

/* Initializes 'dst' with the contents of 'src', compensating for moving it
 * around in memory.  The effect is that, if 'src' was the head of a list, now
 * 'dst' is the head of a list containing the same elements. */
static inline void
list_move(struct list_node *dst, struct list_node *src)
{
    *dst = *src;
    list_moved(dst, src);
}

/* Removes 'elem' from its list and returns the element that followed it.
   Undefined behavior if 'elem' is not in a list. */
static inline struct list_node *
list_remove(struct list_node *elem)
{
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    return elem->next;
}

/* Removes the front element from 'list' and returns it.  Undefined behavior if
   'list' is empty before removal. */
static inline struct list_node *
list_pop_front(struct list_node *list)
{
    struct list_node *front = list->next;

    list_remove(front);
    return front;
}

/* Removes the back element from 'list' and returns it.
   Undefined behavior if 'list' is empty before removal. */
static inline struct list_node *
list_pop_back(struct list_node *list)
{
    struct list_node *back = list->prev;

    list_remove(back);
    return back;
}

/* Returns the front element in 'list_'.
   Undefined behavior if 'list_' is empty. */
static inline struct list_node *
list_front(const struct list_node *list_)
{
    struct list_node *list = CONST_CAST(struct list_node *, list_);

    assert(!list_is_empty(list));

    return list->next;
}

/* Returns the back element in 'list_'.
   Undefined behavior if 'list_' is empty. */
static inline struct list_node *
list_back(const struct list_node *list_)
{
    struct list_node *list = CONST_CAST(struct list_node *, list_);

    assert(!list_is_empty(list));

    return list->prev;
}

/* Returns the number of elements in 'list'.
   Runs in O(n) in the number of elements. */
static inline size_t
list_size(const struct list_node *list)
{
    const struct list_node *e;
    size_t cnt = 0;

    for (e = list->next; e != list; e = e->next) {
        cnt++;
    }
    return cnt;
}

/* Returns true if 'list' is empty, false otherwise. */
static inline bool
list_is_empty(const struct list_node *list)
{
    return list->next == list;
}

/* Returns true if 'list' has exactly 1 element, false otherwise. */
static inline bool
list_is_singleton(const struct list_node *list)
{
    return list_is_short(list) && !list_is_empty(list);
}

/* Returns true if 'list' has 0 or 1 elements, false otherwise. */
static inline bool
list_is_short(const struct list_node *list)
{
    return list->next == list->prev;
}

/* Transplant a list into another, and resets the origin list */
static inline void
list_push_back_all(struct list_node *dst, struct list_node *src)
{
    list_splice(dst, src->next, src);
}

#ifdef __cplusplus
}
#endif

#endif /* list.h */
