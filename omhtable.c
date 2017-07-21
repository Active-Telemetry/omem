/**
 * @file omhtable.c
 * Offset based Hash Table implementation
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include "omem.h"

/* Print a pretty histogram of the bucket sizes */
void omhtable_stats(om_block * om, omhtable * ht)
{
    size_t *histogram = calloc(1, ht->size * sizeof(size_t));
    omhtentry *e;
    size_t max = 0;
    size_t scale = 1;
    size_t max_bucket = 0;
    size_t min_bucket = ht->size;
    size_t i, j;

    for (i = 0; i < ht->size; i++) {
        int offset = 0;
        while ((e = omhtable_get(om, ht, i, &offset)) != NULL) {
            histogram[i] += 1;
            max_bucket = (i > max_bucket) ? i : max_bucket;
            min_bucket = (i < min_bucket) ? i : min_bucket;
        }
    }

    for (i = 0; i < ht->size; i++) {
        max = (histogram[i] > max) ? histogram[i] : max;
    }
    scale = (max > 50) ? max / 50 : 1;

    printf("\n");
    for (i = min_bucket; i <= max_bucket; i++) {
        printf("%10zu ", i);
        for (j = 0; j < histogram[i] / scale; j++) {
            putchar('x');
        }
        if (histogram[i])
            printf(" (%zu)", histogram[i]);
        printf("\n");
    }
}

void omhtable_add(om_block * om, omhtable * ht, size_t hash, omhtentry * e)
{
    assert(ht && ht->size && e && !e->next);
    hash = hash % ht->size;
    ht->table[hash] = omlist_prepend(om, ht->table[hash], (omlistentry *) e);
    return;
}

void omhtable_delete(om_block * om, omhtable * ht, size_t hash, omhtentry * e)
{
    assert(ht && ht->size && e);
    hash = hash % ht->size;
    ht->table[hash] = omlist_remove(om, ht->table[hash], (omlistentry *) e);
    return;
}

size_t omhtable_size(om_block * om, omhtable * ht)
{
    assert(ht && ht->size);
    size_t length = 0;
    int i;
    for (i = 0; i < ht->size; i++)
        length += omlist_length(om, ht->table[i]);
    return length;
}

omhtentry *omhtable_get(om_block * om, omhtable * ht, size_t hash, int *offset)
{
    hash = hash % ht->size;
    return (omhtentry *) omlist_get(om, ht->table[hash], (*offset)++);
}

omhtentry *omhtable_find(om_block * om, omhtable * ht, omhtable_cmp_fn cmp, size_t hash,
                         void *data)
{
    hash = hash % ht->size;
    return (omhtentry *) omlist_find(om, ht->table[hash], (omlist_find_fn) cmp, data);
}

size_t omhtable_strhash(const char *s)
{
    size_t hash = 5381;
    int c;
    while ((c = *s)) {
        hash = ((hash << 5) + hash) + c;
        s++;
    }
    return hash;
}
