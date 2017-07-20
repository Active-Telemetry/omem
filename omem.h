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
/*********************************
 * Offset based memory allocator
 *********************************/
/**
 * A memory segment provided to the allocator
 */
typedef struct om_block {
    size_t base;
    size_t size;
    size_t next;
} om_block;

/**
 * Offset to Pointer conversion
 */
#define omo2p(mb,offset) (offset ? (void *)((mb) + offset) : NULL)
#define omp2o(mb,pointer) (pointer ? ((size_t)pointer - (mb)) : 0)

/**
 * Routines for memory allocation
 */
void ominit(om_block * om, size_t base, size_t size);
void *omalloc(om_block * om, size_t size);
void omfree(om_block * om, void *m);
size_t omavailable(om_block * om);
void omstats(om_block * om);

/*********************************
 * Offset based list
 *********************************/
/**
 * List entry
 */
typedef struct omlistentry {
    size_t next;
    size_t prev;
} omlistentry;

#define OMLIST_INIT (0)
typedef size_t omlist;

omlist omlist_prepend(size_t base, omlist l, omlistentry * e);
omlist omlist_append(size_t base, omlist l, omlistentry * e);
omlist omlist_remove(size_t base, omlist l, omlistentry * data);
size_t omlist_length(size_t base, omlist l);
omlistentry *omlist_get(size_t base, omlist l, unsigned int offset);
omlist omlist_reverse(size_t base, omlist l);
omlist omlist_concat(size_t base, omlist l1, omlist l2);
typedef bool(*omlist_find_fn) (omlistentry * e, void *data);
omlistentry *omlist_find(size_t base, omlist l, omlist_find_fn func, void *data);
typedef int (*omlist_cmp_fn) (omlistentry * e1, omlistentry * e2);
omlist omlist_sort(size_t base, omlist l, omlist_cmp_fn func);

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

void omhtable_add(size_t base, omhtable * ht, size_t hash, omhtentry * e);
void omhtable_delete(size_t base, omhtable * ht, size_t hash, omhtentry * e);
size_t omhtable_size(size_t base, omhtable * ht);
omhtentry *omhtable_get(size_t base, omhtable * ht, size_t hash, int *offset);
typedef bool(*omhtable_cmp_fn) (omhtentry * e, void *data);
omhtentry *omhtable_find(size_t base, omhtable * ht, omhtable_cmp_fn cmp, size_t hash,
                         void *data);
void omhtable_stats(size_t base, omhtable * ht);
size_t omhtable_strhash(const char *s);
