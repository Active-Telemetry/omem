/**
 * @file omhtree.c
 * Offset based Hash Tree implementation
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

static bool _htable_find_cmp_fn(om_block * om, omlistentry * e, void *data)
{
    char *key = omo2p(om, ((omhtree *) e)->key);
    return (key && strcmp(key, (char *) data) == 0);
}

static bool omhtree_empty(om_block * om, omhtree * tree)
{
    return tree->children == 0 || (omhtable_size(om, omo2p(om, tree->children)) == 0);
}

omhtree *omhtree_get(om_block * om, omhtree * root, const char *path)
{
    char *ptr = NULL;
    char *key = NULL;
    omhtree *next = root;
    omhtree *ret = root;

    char *p = g_strdup(path);
    key = strtok_r(p, "/", &ptr);
    while (key) {
        ret = next;
        if (next->children) {
            omhtable *table = (omhtable *) omo2p(om, next->children);
            next = (omhtree *) omhtable_find(om, table, _htable_find_cmp_fn,
                                             omhtable_strhash(key), key);
            ret = next;
        } else {
            next = NULL;
        }
        if (next == NULL) {
            ret = NULL;
            break;
        }
        key = strtok_r(NULL, "/", &ptr);
    }
    g_free(p);
    return ret;
}

omhtree *omhtree_add(om_block * om, omhtree * root, const char *path, size_t size)
{
    char *p = g_strdup(path);
    char *ptr = NULL;
    char *key;
    omhtree *node = NULL;
    omhtree *parent = root;

    if (size < sizeof(omhtree))
        return NULL;

    key = strtok_r(p, "/", &ptr);
    while (key) {
        omhtable *children = parent->children ? omo2p(om, parent->children) : NULL;
        if (children &&
            (node = (omhtree *) omhtable_find(om, children, _htable_find_cmp_fn,
                                              omhtable_strhash(key), key)) != NULL) {
            parent = node;
        } else {
            node = omalloc(om, size);
            memset(node, 0, size);
            node->parent = omp2o(om, parent);
            char *nkey = omalloc(om, strlen(key) + 1);
            node->key = omp2o(om, nkey);
            memcpy(nkey, key, strlen(key) + 1);
            if (parent->children == 0) {
                children = omalloc(om, OMHTABLE_SIZE(32));
                memset(children, 0, OMHTABLE_SIZE(32));
                children->size = 32;
                parent->children = omp2o(om, children);
            }
            omhtable_add(om, children, omhtable_strhash(key), (omlistentry *) node);
            parent = node;
        }
        key = strtok_r(NULL, "/", &ptr);
    }
    g_free(p);
    return parent;
}

void omhtree_delete(om_block * om, omhtree * root, omhtree * node)
{
    if (!node || !node->key)
        return;

    omhtree *parent = (omhtree *) omo2p(om, node->parent);
    if (parent && parent->children) {
        omhtable *table = (omhtable *) omo2p(om, parent->children);
        char *key = (char *) omo2p(om, node->key);
        omhtable_delete(om, table, omhtable_strhash(key), (omlistentry *) node);
        if (omhtable_size(om, table) == 0) {
            omfree(om, table);
            parent->children = 0;
        }
    }
    node->parent = 0;

    if (node->children) {
        omhtable *table = (omhtable *) omo2p(om, parent->children);
        int i;

        for (i = 0; i < table->size; i++) {
            omhtree *child;
            int offset = 0;

            while ((child = (omhtree *) omhtable_get(om, table, i, &offset)) != NULL) {
                omhtree_delete(om, node, child);
            }
        }
        omfree(om, table);
    }

    omfree(om, omo2p(om, node->key));
    omfree(om, node);

    if (parent) {
        /* This is now a hanging node, remove it */
        if (omhtree_empty(om, parent) && parent != root) {
            omhtree_delete(om, root, parent);
        }
    }
}

omhtree *omhtree_child(om_block * om, omhtree * node, omhtree * prev)
{
    omhtable *table;
    omhtree *child = NULL;
    int i;

    if (!node || !node->key || !node->children)
        return NULL;

    if (prev && prev->base.next) {
        return (omhtree *) omo2p(om, prev->base.next);
    }

    table = (omhtable *) omo2p(om, node->children);
    for (i = 0; i < table->size; i++) {
        int offset = 0;
        while ((child = (omhtree *) omhtable_get(om, table, i, &offset)) != NULL) {
            if (prev == NULL)
                return child;
            if (prev == child)
                prev = NULL;
        }
    }

    return NULL;
}
