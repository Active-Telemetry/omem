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

omlist omlist_prepend(om_block * om, omlist l, omlistentry * e)
{
    omlistentry *list = omo2p(om, l);
    e->next = omp2o(om, list);
    if (list) {
        if (list->prev) {
            omlistentry *prev = omo2p(om, list->prev);
            prev->next = omp2o(om, e);
        }
        e->prev = list->prev;
        list->prev = omp2o(om, e);
    } else {
        e->prev = 0;
    }
    return omp2o(om, e);
}

omlist omlist_append(om_block * om, omlist l, omlistentry * e)
{
    omlistentry *list = omo2p(om, l);
    if (list) {
        omlistentry *last = list;
        while (last && last->next)
            last = omo2p(om, last->next);
        last->next = omp2o(om, e);
        e->prev = omp2o(om, last);
        return omp2o(om, list);
    } else {
        e->prev = 0;
        return omp2o(om, e);
    }
}

omlist omlist_remove(om_block * om, omlist l, omlistentry * e)
{
    omlistentry *list = omo2p(om, l);
    if (e == NULL)
        return omp2o(om, list);
    if (e->prev) {
        omlistentry *prev = omo2p(om, e->prev);
        assert(prev->next == omp2o(om, e));
        prev->next = e->next;
    }
    if (e->next) {
        omlistentry *next = omo2p(om, e->next);
        assert(next->prev == omp2o(om, e));
        next->prev = e->prev;
    }
    if (e == list)
        list = omo2p(om, list->next);
    e->next = 0;
    e->prev = 0;
    return omp2o(om, list);
}

size_t omlist_length(om_block * om, omlist l)
{
    omlistentry *list = omo2p(om, l);
    size_t length = 0;
    while (list) {
        length++;
        list = omo2p(om, list->next);
    }
    return length;
}

omlistentry *omlist_get(om_block * om, omlist l, unsigned int offset)
{
    omlistentry *list = omo2p(om, l);
    while ((offset-- > 0) && list)
        list = omo2p(om, list->next);
    return list;
}

omlist omlist_reverse(om_block * om, omlist l)
{
    omlistentry *list = omo2p(om, l);
    omlistentry *last = NULL;
    while (list) {
        last = list;
        list = omo2p(om, list->next);
        last->next = last->prev;
        last->prev = omp2o(om, list);
    }
    return omp2o(om, last);
}

omlist omlist_concat(om_block * om, omlist l1, omlist l2)
{
    omlistentry *list1 = omo2p(om, l1);
    omlistentry *list2 = omo2p(om, l2);
    omlistentry *last = list1;
    while (last && last->next)
        last = omo2p(om, last->next);
    if (last)
        last->next = omp2o(om, list2);
    else
        list1 = list2;
    list2->prev = omp2o(om, last);
    return omp2o(om, list1);
}

omlistentry *omlist_find(om_block * om, omlist l, omlist_find_fn func, void *data)
{
    assert(func);
    omlistentry *list = omo2p(om, l);
    while (list) {
        if (func(list, data))
            return list;
        list = omo2p(om, list->next);
    }
    return NULL;
}

static omlist omlist_sort_merge(om_block * om, omlist list1, omlist list2,
                                omlist_cmp_fn func)
{
    omlistentry *l1 = omo2p(om, list1);
    omlistentry *l2 = omo2p(om, list2);
    omlistentry list, *l, *lprev;
    int cmp;
    l = &list;
    lprev = NULL;
    while (l1 && l2) {
        cmp = func(l1, l2);
        if (cmp <= 0) {
            l->next = omp2o(om, l1);
            l1 = omo2p(om, l1->next);
        } else {
            l->next = omp2o(om, l2);
            l2 = omo2p(om, l2->next);
        }
        l = omo2p(om, l->next);
        l->prev = omp2o(om, lprev);
        lprev = l;
    }
    l->next = l1 ? omp2o(om, l1) : omp2o(om, l2);
    lprev = omo2p(om, l->next);
    lprev->prev = omp2o(om, l);
    return list.next;
}

omlist omlist_sort(om_block * om, omlist l, omlist_cmp_fn func)
{
    omlistentry *list = omo2p(om, l);
    omlistentry *l1, *l2;
    if (!list)
        return 0;
    if (!list->next)
        return omp2o(om, list);
    l1 = list;
    l2 = omo2p(om, list->next);
    while ((l2 = omo2p(om, l2->next)) != NULL) {
        if ((l2 = omo2p(om, l2->next)) == NULL)
            break;
        l1 = omo2p(om, l1->next);
    }
    l2 = omo2p(om, l1->next);
    l1->next = 0;
    return omlist_sort_merge(om,
                             omlist_sort(om, omp2o(om, list), func),
                             omlist_sort(om, omp2o(om, l2), func), func);
}
