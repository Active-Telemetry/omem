/**
 * @file omem.h
 * Offset based memory allocation
 *
 * Copyright 2017, ECLB Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>
 */

/* Make it clear what parameters are offsets
 */
typedef size_t offset_t;

/**
 * Offset to Pointer conversion
 */
#define omo2p(mb,offset) (offset ? (void *)(((size_t)mb) + offset) : NULL)
#define omp2o(mb,pointer) (pointer ? ((size_t)pointer - ((size_t)mb)) : 0)

/*********************************
 * Offset based memory allocator
 *********************************/
/**
 * A memory segment provided to the allocator
 */
typedef struct om_block {
    int shmid;
    size_t size;
    offset_t next;
    size_t headroom;
} om_block;

/**
 * Routines for memory allocation
 */
om_block *omcreate(const char *fname, size_t size, size_t headroom);
void *omalloc(om_block * om, size_t size);
void omfree(om_block * om, void *m);
size_t omavailable(om_block * om);
void omstats(om_block * om);
void omdestroy(om_block * om);

/*********************************
 * Offset based list
 *********************************/
/**
 * List entry
 */
typedef struct omlistentry {
    offset_t next;
    offset_t prev;
} omlistentry;

#define OMLIST_INIT (0)
typedef offset_t omlist;

omlist omlist_prepend(om_block * om, omlist l, omlistentry * e);
omlist omlist_append(om_block * om, omlist l, omlistentry * e);
omlist omlist_remove(om_block * om, omlist l, omlistentry * data);
size_t omlist_length(om_block * om, omlist l);
omlistentry *omlist_get(om_block * om, omlist l, unsigned int offset);
omlist omlist_reverse(om_block * om, omlist l);
omlist omlist_concat(om_block * om, omlist l1, omlist l2);
typedef bool(*omlist_find_fn) (om_block * om, omlistentry * e, void *data);
omlistentry *omlist_find(om_block * om, omlist l, omlist_find_fn func, void *data);
typedef int (*omlist_cmp_fn) (om_block * om, omlistentry * e1, omlistentry * e2);
omlist omlist_sort(om_block * om, omlist l, omlist_cmp_fn func);

/*********************************
 * Offset based hash table
 *********************************/
/**
 * Hash table bucket entry
 */
typedef struct omlistentry omhtentry;

/**
 * Hash Table base structure
 */
typedef struct omhtable {
    int size;
    omlist table[0];
} omhtable;

#define OMHTABLE_SIZE(buckets) (sizeof(omhtable) + buckets * sizeof(omhtentry *))

void omhtable_add(om_block * om, omhtable * ht, size_t hash, omhtentry * e);
void omhtable_delete(om_block * om, omhtable * ht, size_t hash, omhtentry * e);
size_t omhtable_size(om_block * om, omhtable * ht);
omhtentry *omhtable_get(om_block * om, omhtable * ht, size_t hash, int *offset);
typedef bool(*omhtable_cmp_fn) (om_block * om, omhtentry * e, void *data);
omhtentry *omhtable_find(om_block * om, omhtable * ht, omhtable_cmp_fn cmp, size_t hash,
                         void *data);
void omhtable_stats(om_block * om, omhtable * ht);
size_t omhtable_strhash(const char *s);

/*********************************
 * Offset based hash tree
 *********************************/
/**
 * Hash tree node
 */
typedef struct omhtree {
    omhtentry base;
    offset_t parent;
    offset_t key;
    offset_t children;
} omhtree;

omhtree *omhtree_add(om_block * om, omhtree * root, const char *path, size_t size);
void omhtree_delete(om_block * om, omhtree * root, omhtree * node);
omhtree *omhtree_get(om_block * om, omhtree * root, const char *path);
#define omhtree_parent(om, node) ((omhtree *) omo2p(om, ((omhtree *) node)->parent))
#define omhtree_key(om, node) ((const char *) omo2p(om, ((omhtree *) node)->key))
omhtree *omhtree_child(om_block * om, omhtree * node, omhtree * prev);
void omhtree_stats(om_block * om, omhtree * tree);
