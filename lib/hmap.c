#include "hmap.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "util.h"

/* Initializes 'hmap' as an empty hash table. */
void
hmap_init(struct hmap *hmap)
{
    hmap->buckets = &hmap->one;
    hmap->one = NULL;
    hmap->mask = 0;
    hmap->n = 0;
}

/* Frees memory reserved by 'hmap'.  It is the client's responsibility to free
 * the nodes themselves, if necessary. */
void
hmap_destroy(struct hmap *hmap)
{
    if (hmap && hmap->buckets != &hmap->one) {
        free(hmap->buckets);
    }
}

/* Removes all node from 'hmap', leaving it ready to accept more nodes.  Does
 * not free memory allocated for 'hmap'.
 *
 * This function is appropriate when 'hmap' will soon have about as many
 * elements as it did before.  If 'hmap' will likely have fewer elements than
 * before, use hmap_destroy() followed by hmap_init() to save memory and
 * iteration time. */
void
hmap_clear(struct hmap *hmap)
{
    if (hmap->n > 0) {
        hmap->n = 0;
        memset(hmap->buckets, 0, (hmap->mask + 1) * sizeof *hmap->buckets);
    }
}

/* Exchanges hash maps 'a' and 'b'. */
void
hmap_swap(struct hmap *a, struct hmap *b)
{
    struct hmap tmp = *a;
    *a = *b;
    *b = tmp;
    hmap_moved(a);
    hmap_moved(b);
}

/* Adjusts 'hmap' to compensate for having moved position in memory (e.g. due
 * to realloc()). */
void
hmap_moved(struct hmap *hmap)
{
    if (!hmap->mask) {
        hmap->buckets = &hmap->one;
    }
}

static void
resize(struct hmap *hmap, size_t new_mask)
{
    struct hmap tmp;
    size_t i;

    assert(is_pow2(new_mask + 1));

    hmap_init(&tmp);
    if (new_mask) {
        tmp.buckets = xmalloc(sizeof *tmp.buckets * (new_mask + 1));
        tmp.mask = new_mask;
        for (i = 0; i <= tmp.mask; i++) {
            tmp.buckets[i] = NULL;
        }
    }
    int n_big_buckets = 0;
    int biggest_count = 0;
    int n_biggest_buckets = 0;
    for (i = 0; i <= hmap->mask; i++) {
        struct hmap_node *node, *next;
        int count = 0;
        for (node = hmap->buckets[i]; node; node = next) {
            next = node->next;
            hmap_insert_fast(&tmp, node, node->hash);
            count++;
        }
        if (count > 5) {
            n_big_buckets++;
            if (count > biggest_count) {
                biggest_count = count;
                n_biggest_buckets = 1;
            } else if (count == biggest_count) {
                n_biggest_buckets++;
            }
        }
    }
    hmap_swap(hmap, &tmp);
    hmap_destroy(&tmp);

    if (n_big_buckets) {
        ;
        /*
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(10, 10);
        VLOG_DBG_RL(&rl, "%s: %d bucket%s with 6+ nodes, "
                         "including %d bucket%s with %d nodes "
                         "(%"PRIuSIZE" nodes total across %"PRIuSIZE" buckets)",
                where,
                n_big_buckets, n_big_buckets > 1 ? "s" : "",
                n_biggest_buckets, n_biggest_buckets > 1 ? "s" : "",
                biggest_count,
                hmap->n, hmap->mask + 1);
        */
    }
}

static size_t
calc_mask(size_t capacity)
{
    size_t mask = capacity / 2;
    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;
    mask |= mask >> 16;
#if SIZE_MAX > UINT32_MAX
    mask |= mask >> 32;
#endif

    /* If we need to dynamically allocate buckets we might as well allocate at
     * least 4 of them. */
    mask |= (mask & 1) << 1;

    return mask;
}

/*
 * Expands 'hmap', if necessary, to optimize the performance of searches.
 */
void
hmap_expand(struct hmap *hmap)
{
    size_t new_mask = calc_mask(hmap->n);
    if (new_mask > hmap->mask) {
        resize(hmap, new_mask);
    }
}

/*
 * Shrinks 'hmap', if necessary, to optimize the performance of iteration.
 */
void
hmap_shrink(struct hmap *hmap)
{
    size_t new_mask = calc_mask(hmap->n);
    if (new_mask < hmap->mask) {
        resize(hmap, new_mask);
    }
}

/*
 * Expands 'hmap', if necessary, to optimize the performance of searches when
 * it has up to 'n' elements.  (But iteration will be slow in a hash map whose
 * allocated capacity is much higher than its current number of nodes.)
 *
 */
void
hmap_reserve(struct hmap *hmap, size_t n)
{
    size_t new_mask = calc_mask(n);
    if (new_mask > hmap->mask) {
        resize(hmap, new_mask);
    }
}

/* Adjusts 'hmap' to compensate for 'old_node' having moved position in memory
 * to 'node' (e.g. due to realloc()). */
void
hmap_node_moved(struct hmap *hmap,
                struct hmap_node *old_node, struct hmap_node *node)
{
    struct hmap_node **bucket = &hmap->buckets[node->hash & hmap->mask];
    while (*bucket != old_node) {
        bucket = &(*bucket)->next;
    }
    *bucket = node;
}

/* Returns the next node in 'hmap' in hash order, or NULL if no nodes remain in
 * 'hmap'.  Uses '*pos' to determine where to begin iteration, and updates
 * '*pos' to pass on the next iteration into them before returning.
 *
 * It's better to use plain HMAP_FOR_EACH and related functions, since they are
 * faster and better at dealing with hmaps that change during iteration.
 *
 * Before beginning iteration, set '*pos' to all zeros. */
struct hmap_node *
hmap_at_position(const struct hmap *hmap,
                 struct hmap_position *pos)
{
    size_t offset;
    size_t b_idx;

    offset = pos->offset;
    for (b_idx = pos->bucket; b_idx <= hmap->mask; b_idx++) {
        struct hmap_node *node;
        size_t n_idx;

        for (n_idx = 0, node = hmap->buckets[b_idx]; node != NULL;
             n_idx++, node = node->next) {
            if (n_idx == offset) {
                if (node->next) {
                    pos->bucket = node->hash & hmap->mask;
                    pos->offset = offset + 1;
                } else {
                    pos->bucket = (node->hash & hmap->mask) + 1;
                    pos->offset = 0;
                }
                return node;
            }
        }
        offset = 0;
    }

    pos->bucket = 0;
    pos->offset = 0;
    return NULL;
}

/* Returns true if 'node' is in 'hmap', false otherwise. */
bool
hmap_contains(const struct hmap *hmap, const struct hmap_node *node)
{
    struct hmap_node *p;

    for (p = hmap_first_in_bucket(hmap, node->hash); p; p = p->next) {
        if (p == node) {
            return true;
        }
    }

    return false;
}
