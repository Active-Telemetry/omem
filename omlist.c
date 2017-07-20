/**
 * @file omlist.c
 * Offset based List implementation
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

omlist omlist_prepend(size_t base, omlist l, omlistentry * e)
{
    omlistentry *list = omo2p(base, l);
    e->next = omp2o(base, list);
    if (list) {
        if (list->prev) {
            omlistentry *prev = omo2p(base, list->prev);
            prev->next = omp2o(base, e);
        }
        e->prev = list->prev;
        list->prev = omp2o(base, e);
    } else {
        e->prev = 0;
    }
    return omp2o(base, e);
}

omlist omlist_append(size_t base, omlist l, omlistentry * e)
{
    omlistentry *list = omo2p(base, l);
    if (list) {
        omlistentry *last = list;
        while (last && last->next)
            last = omo2p(base, last->next);
        last->next = omp2o(base, e);
        e->prev = omp2o(base, last);
        return omp2o(base, list);
    } else {
        e->prev = 0;
        return omp2o(base, e);
    }
}

omlist omlist_remove(size_t base, omlist l, omlistentry * e)
{
    omlistentry *list = omo2p(base, l);
    if (e == NULL)
        return omp2o(base, list);
    if (e->prev) {
        omlistentry *prev = omo2p(base, e->prev);
        assert(prev->next == omp2o(base, e));
        prev->next = e->next;
    }
    if (e->next) {
        omlistentry *next = omo2p(base, e->next);
        assert(next->prev == omp2o(base, e));
        next->prev = e->prev;
    }
    if (e == list)
        list = omo2p(base, list->next);
    e->next = 0;
    e->prev = 0;
    return omp2o(base, list);
}

size_t omlist_length(size_t base, omlist l)
{
    omlistentry *list = omo2p(base, l);
    size_t length = 0;
    while (list) {
        length++;
        list = omo2p(base, list->next);
    }
    return length;
}

omlistentry *omlist_get(size_t base, omlist l, unsigned int offset)
{
    omlistentry *list = omo2p(base, l);
    while ((offset-- > 0) && list)
        list = omo2p(base, list->next);
    return list;
}

omlist omlist_reverse(size_t base, omlist l)
{
    omlistentry *list = omo2p(base, l);
    omlistentry *last = NULL;
    while (list) {
        last = list;
        list = omo2p(base, list->next);
        last->next = last->prev;
        last->prev = omp2o(base, list);
    }
    return omp2o(base, last);
}

omlist omlist_concat(size_t base, omlist l1, omlist l2)
{
    omlistentry *list1 = omo2p(base, l1);
    omlistentry *list2 = omo2p(base, l2);
    omlistentry *last = list1;
    while (last && last->next)
        last = omo2p(base, last->next);
    if (last)
        last->next = omp2o(base, list2);
    else
        list1 = list2;
    list2->prev = omp2o(base, last);
    return omp2o(base, list1);
}

omlistentry *omlist_find(size_t base, omlist l, omlist_find_fn func, void *data)
{
    assert(func);
    omlistentry *list = omo2p(base, l);
    while (list) {
        if (func(list, data))
            return list;
        list = omo2p(base, list->next);
    }
    return NULL;
}

static omlist omlist_sort_merge(size_t base, omlist list1, omlist list2, omlist_cmp_fn func)
{
    omlistentry *l1 = omo2p(base, list1);
    omlistentry *l2 = omo2p(base, list2);
    omlistentry list, *l, *lprev;
    int cmp;
    l = &list;
    lprev = NULL;
    while (l1 && l2) {
        cmp = func(l1, l2);
        if (cmp <= 0) {
            l->next = omp2o(base, l1);
            l1 = omo2p(base, l1->next);
        } else {
            l->next = omp2o(base, l2);
            l2 = omo2p(base, l2->next);
        }
        l = omo2p(base, l->next);
        l->prev = omp2o(base, lprev);
        lprev = l;
    }
    l->next = l1 ? omp2o(base, l1) : omp2o(base, l2);
    lprev = omo2p(base, l->next);
    lprev->prev = omp2o(base, l);
    return list.next;
}

omlist omlist_sort(size_t base, omlist l, omlist_cmp_fn func)
{
    omlistentry *list = omo2p(base, l);
    omlistentry *l1, *l2;
    if (!list)
        return 0;
    if (!list->next)
        return omp2o(base, list);
    l1 = list;
    l2 = omo2p(base, list->next);
    while ((l2 = omo2p(base, l2->next)) != NULL) {
        if ((l2 = omo2p(base, l2->next)) == NULL)
            break;
        l1 = omo2p(base, l1->next);
    }
    l2 = omo2p(base, l1->next);
    l1->next = 0;
    return omlist_sort_merge(base,
                             omlist_sort(base, omp2o(base, list), func),
                             omlist_sort(base, omp2o(base, l2), func), func);
}
