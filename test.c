/**
 * @file test.c
 * Unit tests
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
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <glib.h>
#include <CUnit/Basic.h>
#include "omem.h"

#define TEST_ITERATIONS     5000
#define TEST_ITERATIONS_BIG 50000
#define TEST_SHM_FNAME      "/tmp/omem_test.shm"

static inline uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec);
}

#if 1
#define DEBUG_BLOCK(ptr)
#define DEBUG_NEXT_BLOCK(ptr)
#else
void DEBUG_BLOCK(void *ptr)
{
    g_shm_meta *bp = (g_shm_meta *) ((uint8_t *) ptr - META_SIZE);
    printf("0x%p: 0x%zu:%d 0x%zu:%d\n", bp,
           BLK_SIZE(bp), BLK_FREE(bp), BLK_SIZE(BLK_FOOT(bp)), BLK_FREE(BLK_FOOT(bp)));
}

void DEBUG_NEXT_BLOCK(void *ptr)
{
    g_shm_meta *bp = (g_shm_meta *) ((uint8_t *) ptr - META_SIZE);
    DEBUG_BLOCK(((uint8_t *) BLK_NEXT(bp) - META_SIZE));
}
#endif

#define TEST_HEAP_SIZE (8 * 1024 * 1024)
static om_block *omm;

int suite_init(void)
{
    char *cmd;

    cmd = g_strdup_printf("touch %s", TEST_SHM_FNAME);
    if (system(cmd) == 0)
        g_free(cmd);
    cmd = g_strdup_printf("ipcrm -M 0x%08x", ftok(TEST_SHM_FNAME, 'R'));
    if (system(cmd) == 0)
        g_free(cmd);
    omm = omcreate(TEST_SHM_FNAME, TEST_HEAP_SIZE);
    return 0;
}

int suite_shutdown(void)
{
    omdestroy(omm);
    if (system("rm /tmp/omem_test.shm"));
    return 0;
}

void test_attach()
{
    om_block *om = omcreate(TEST_SHM_FNAME, TEST_HEAP_SIZE);
    CU_ASSERT(om != omm);
    CU_ASSERT(omavailable(om) == TEST_HEAP_SIZE);
    omdestroy(om);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_malloc_0()
{
    CU_ASSERT(omalloc(omm, 0) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_free_null()
{
    omfree(omm, 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_malloc1()
{
    void *m;
    CU_ASSERT((m = omalloc(omm, 1)) != 0);
    // DEBUG_BLOCK(m);
    // DEBUG_NEXT_BLOCK(m);
    omfree(omm, m);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_malloc_twice()
{
    void *m1, *m2;
    CU_ASSERT((m1 = omalloc(omm, 1)) != 0);
    // DEBUG_BLOCK(m1);
    CU_ASSERT((m2 = omalloc(omm, 2)) != 0);
    // DEBUG_BLOCK(m2);
    // DEBUG_NEXT_BLOCK(m2);
    omfree(omm, m2);
    // DEBUG_BLOCK(m2);
    omfree(omm, m1);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_malloc_twice_reverse()
{
    void *m1, *m2;
    CU_ASSERT((m1 = omalloc(omm, 1)) != 0);
    CU_ASSERT((m2 = omalloc(omm, 2)) != 0);
    omfree(omm, m1);
    omfree(omm, m2);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_malloc_until_fail()
{
    GList *mlist = NULL;
    GList *iter;
    while (1) {
        size_t size = rand() % ((16 * 1024) - (2 * 2 * sizeof(size_t)));
        if (omavailable(omm) < size)
            break;
        void *m = omalloc(omm, size);
        mlist = g_list_prepend(mlist, m);
    }
    omstats(omm);
    for (iter = mlist; iter; iter = g_list_next(iter))
        omfree(omm, iter->data);
    g_list_free(mlist);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_malloc_performance()
{
    GList *allocated = NULL;
    uint64_t start;
    GList *iter;
    int i;

    start = get_time_us();
    for (i = 0; i < TEST_ITERATIONS_BIG; i++) {
        allocated = g_list_prepend(allocated, omalloc(omm, 8));
    }
    for (iter = allocated; iter; iter = iter->next) {
        omfree(omm, iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    g_list_free(allocated);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_glib_malloc_performance()
{
    GList *allocated = NULL;
    uint64_t start;
    GList *iter;
    int i;

    start = get_time_us();
    for (i = 0; i < TEST_ITERATIONS_BIG; i++) {
        allocated = g_list_prepend(allocated, g_malloc(64));
    }
    for (iter = allocated; iter; iter = iter->next) {
        g_free(iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    g_list_free(allocated);
}

void test_malloc_performance_fragmented()
{
    GList *fragments = NULL;
    GList *allocated = NULL;
    uint64_t start;
    GList *iter;
    int i;

    for (i = 0; i < (TEST_HEAP_SIZE / 50); i += 64) {
        fragments = g_list_prepend(fragments, omalloc(omm, 64));
        omfree(omm, omalloc(omm, 64));
    }
    start = get_time_us();
    for (i = 0; i < TEST_ITERATIONS_BIG; i++) {
        allocated = g_list_prepend(allocated, omalloc(omm, 64));
    }
    for (iter = allocated; iter; iter = iter->next) {
        omfree(omm, iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    g_list_free(allocated);
    for (iter = fragments; iter; iter = iter->next) {
        omfree(omm, iter->data);
    }
    g_list_free(fragments);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

typedef struct list_entry {
    omlistentry base;
    char str[0];
} list_entry;

static list_entry *list_entry_new(char *str)
{
    int size = sizeof(list_entry) + strlen(str) + 1;
    list_entry *e = omalloc(omm, size);
    memset(e, 0, size);
    strcpy(e->str, str);
    return e;
}

static void list_entry_free(list_entry * e)
{
    omfree(omm, e);
}

static bool list_entry_find(omlistentry * e, void *data)
{
    return (strcmp(((list_entry *) e)->str, (char *) data) == 0);
}

static int list_entry_cmp(omlistentry * e1, omlistentry * e2)
{
    return strcmp(((list_entry *) e1)->str, ((list_entry *) e2)->str);
}

void test_list_add_remove()
{
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
    omlist thelist = OMLIST_INIT;
    list_entry *e = list_entry_new("dummy");
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e)) != 0);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e)) == 0);
    list_entry_free(e);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_remove_not_there()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e = list_entry_new("dummy");
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e)) == 0);
    list_entry_free(e);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_prepend()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e = list_entry_new("dummy");
    list_entry *e1 = list_entry_new("dummy1");
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e)) != 0);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT(omlist_get(omm, thelist, 0) == (omlistentry *) e1);
    CU_ASSERT(omlist_get(omm, thelist, 1) == (omlistentry *) e);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e)) == 0);
    list_entry_free(e1);
    list_entry_free(e);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_append()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e = list_entry_new("dummy");
    list_entry *e1 = list_entry_new("dummy1");
    CU_ASSERT((thelist = omlist_append(omm, thelist, (omlistentry *) e)) != 0);
    CU_ASSERT((thelist = omlist_append(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT(omlist_get(omm, thelist, 0) == (omlistentry *) e);
    CU_ASSERT(omlist_get(omm, thelist, 1) == (omlistentry *) e1);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e)) == 0);
    list_entry_free(e1);
    list_entry_free(e);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_length()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e1 = list_entry_new("dummy1");
    list_entry *e2 = list_entry_new("dummy2");
    list_entry *e3 = list_entry_new("dummy3");
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT(omlist_length(omm, thelist) == 1);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e2)) != 0);
    CU_ASSERT(omlist_length(omm, thelist) == 2);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e3)) != 0);
    CU_ASSERT(omlist_length(omm, thelist) == 3);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e1);
    CU_ASSERT(omlist_length(omm, thelist) == 2);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e2);
    CU_ASSERT(omlist_length(omm, thelist) == 1);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e3);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    list_entry_free(e1);
    list_entry_free(e2);
    list_entry_free(e3);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_get()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e = list_entry_new("dummy");
    list_entry *e1 = list_entry_new("dummy1");
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e)) != 0);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT(omlist_get(omm, thelist, 0) == (omlistentry *) e1);
    CU_ASSERT(omlist_get(omm, thelist, 1) == (omlistentry *) e);
    CU_ASSERT(omlist_get(omm, thelist, 2) == NULL);
    CU_ASSERT(omlist_get(omm, thelist, -1) == NULL);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e)) == 0);
    list_entry_free(e1);
    list_entry_free(e);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_reverse()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e = list_entry_new("dummy");
    list_entry *e1 = list_entry_new("dummy1");
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e)) != 0);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT(omlist_get(omm, thelist, 0) == (omlistentry *) e1);
    CU_ASSERT(omlist_get(omm, thelist, 1) == (omlistentry *) e);
    thelist = omlist_reverse(omm, thelist);
    CU_ASSERT(omlist_get(omm, thelist, 0) == (omlistentry *) e);
    CU_ASSERT(omlist_get(omm, thelist, 1) == (omlistentry *) e1);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT((thelist = omlist_remove(omm, thelist, (omlistentry *) e)) == 0);
    list_entry_free(e1);
    list_entry_free(e);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_concat()
{
    omlist thelist = OMLIST_INIT;
    omlist theotherlist = OMLIST_INIT;
    list_entry *e = list_entry_new("dummy");
    list_entry *e1 = list_entry_new("dummy1");
    thelist = omlist_prepend(omm, thelist, (omlistentry *) e);
    theotherlist = omlist_prepend(omm, theotherlist, (omlistentry *) e1);
    CU_ASSERT((thelist = omlist_concat(omm, thelist, theotherlist)) != 0);
    CU_ASSERT(omlist_get(omm, thelist, 0) == (omlistentry *) e);
    CU_ASSERT(omlist_get(omm, thelist, 1) == (omlistentry *) e1);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e1);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e);
    list_entry_free(e1);
    list_entry_free(e);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_find()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e1 = list_entry_new("dummy1");
    list_entry *e2 = list_entry_new("dummy2");
    list_entry *e3 = list_entry_new("dummy3");
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e1)) != 0);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e2)) != 0);
    CU_ASSERT((thelist = omlist_prepend(omm, thelist, (omlistentry *) e3)) != 0);
    CU_ASSERT(omlist_find(omm, thelist, list_entry_find, "dummy") == NULL);
    CU_ASSERT(omlist_find(omm, thelist, list_entry_find, "dummy1") == (omlistentry *) e1);
    CU_ASSERT(omlist_find(omm, thelist, list_entry_find, "dummy2") == (omlistentry *) e2);
    CU_ASSERT(omlist_find(omm, thelist, list_entry_find, "dummy3") == (omlistentry *) e3);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e1);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e2);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e3);
    list_entry_free(e1);
    list_entry_free(e2);
    list_entry_free(e3);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_sort()
{
    omlist thelist = OMLIST_INIT;
    list_entry *e1 = list_entry_new("abc");
    list_entry *e2 = list_entry_new("def");
    list_entry *e3 = list_entry_new("123");
    thelist = omlist_prepend(omm, thelist, (omlistentry *) e1);
    thelist = omlist_prepend(omm, thelist, (omlistentry *) e2);
    thelist = omlist_prepend(omm, thelist, (omlistentry *) e3);
    thelist = omlist_sort(omm, thelist, list_entry_cmp);
    CU_ASSERT(omlist_get(omm, thelist, 0) == (omlistentry *) e3);
    CU_ASSERT(omlist_get(omm, thelist, 1) == (omlistentry *) e1);
    CU_ASSERT(omlist_get(omm, thelist, 2) == (omlistentry *) e2);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e1);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e2);
    thelist = omlist_remove(omm, thelist, (omlistentry *) e3);
    list_entry_free(e1);
    list_entry_free(e2);
    list_entry_free(e3);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_prepend_performance()
{
    omlist thelist = OMLIST_INIT;
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < TEST_ITERATIONS; i++) {
        list_entry *e = list_entry_new("dummy");
        entries = g_list_prepend(entries, e);
    }
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        thelist = omlist_prepend(omm, thelist, (omlistentry *) iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    CU_ASSERT(omlist_length(omm, thelist) == TEST_ITERATIONS);
    entries = g_list_reverse(entries);
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        thelist = omlist_remove(omm, thelist, (omlistentry *) iter->data);
        list_entry_free((list_entry *) iter->data);
    }
    g_list_free(entries);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_list_append_performance()
{
    omlist thelist = OMLIST_INIT;
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < TEST_ITERATIONS; i++) {
        list_entry *e = list_entry_new("dummy");
        entries = g_list_prepend(entries, e);
    }
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        thelist = omlist_append(omm, thelist, (omlistentry *) iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    CU_ASSERT(omlist_length(omm, thelist) == TEST_ITERATIONS);
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        thelist = omlist_remove(omm, thelist, (omlistentry *) iter->data);
        list_entry_free((list_entry *) iter->data);
    }
    g_list_free(entries);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_glist_append_performance()
{
    GList *thelist = NULL;
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < TEST_ITERATIONS; i++) {
        list_entry *e = list_entry_new("dummy");
        entries = g_list_prepend(entries, e);
    }
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        thelist = g_list_append(thelist, (omlistentry *) iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        thelist = g_list_remove(thelist, (omlistentry *) iter->data);
        list_entry_free((list_entry *) iter->data);
    }
    g_list_free(entries);
}

void test_list_find_performance()
{
    omlist thelist = OMLIST_INIT;
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < TEST_ITERATIONS; i++) {
        char *s = g_strdup_printf("%x", rand());
        list_entry *e = list_entry_new(s);
        free(s);
        entries = g_list_prepend(entries, e);
        thelist = omlist_append(omm, thelist, (omlistentry *) e);
    }
    entries = g_list_reverse(entries);
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        list_entry *e = (list_entry *) iter->data;
        CU_ASSERT(omlist_find(omm, thelist, list_entry_find, e->str) != NULL);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start) / TEST_ITERATIONS);
    CU_ASSERT(omlist_length(omm, thelist) == TEST_ITERATIONS);
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        thelist = omlist_remove(omm, thelist, (omlistentry *) iter->data);
        list_entry_free((list_entry *) iter->data);
    }
    g_list_free(entries);
    CU_ASSERT(omlist_length(omm, thelist) == 0);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_glist_find_performance()
{
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < TEST_ITERATIONS; i++) {
        char *s = g_strdup_printf("%x", rand());
        list_entry *e = list_entry_new(s);
        free(s);
        entries = g_list_prepend(entries, e);
    }
    entries = g_list_reverse(entries);
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        list_entry *e = (list_entry *) iter->data;
        CU_ASSERT(g_list_find_custom(entries, e, (GCompareFunc) list_entry_cmp) != NULL);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start) / TEST_ITERATIONS);
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        list_entry_free((list_entry *) iter->data);
    }
    g_list_free(entries);
}

#define TEST_HASH_TABLE_SIZE (32)

typedef struct htable_entry {
    omhtentry base;
    char str[0];
} htable_entry;

static htable_entry *htable_entry_new(char *str)
{
    int size = sizeof(htable_entry) + strlen(str) + 1;
    htable_entry *e = omalloc(omm, size);
    memset(e, 0, size);
    strcpy(e->str, str);
    return e;
}

static void htable_entry_free(htable_entry * e)
{
    omfree(omm, e);
}

static bool htable_entry_cmp(htable_entry * e, char *str)
{
    return (strcmp(e->str, str) == 0);
}

static omhtable *create_table(int buckets)
{
    omhtable *table = (omhtable *) omalloc(omm, OMHTABLE_SIZE(buckets));
    memset(table, 0, OMHTABLE_SIZE(buckets));
    table->size = buckets;
    return table;
}

static void destroy_table(omhtable * table)
{
    omfree(omm, table);
}

void test_htable_add_delete()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    omhtable_add(omm, htable, 0, (omhtentry *) e);
    omhtable_delete(omm, htable, 0, (omhtentry *) e);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_size()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e1 = htable_entry_new("dummy1");
    htable_entry *e2 = htable_entry_new("dummy2");
    htable_entry *e3 = htable_entry_new("dummy3");
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    omhtable_add(omm, htable, 0, (omhtentry *) e1);
    CU_ASSERT(omhtable_size(omm, htable) == 1);
    omhtable_add(omm, htable, 1, (omhtentry *) e2);
    CU_ASSERT(omhtable_size(omm, htable) == 2);
    omhtable_add(omm, htable, 1, (omhtentry *) e3);
    CU_ASSERT(omhtable_size(omm, htable) == 3);
    omhtable_delete(omm, htable, 0, (omhtentry *) e1);
    CU_ASSERT(omhtable_size(omm, htable) == 2);
    omhtable_delete(omm, htable, 1, (omhtentry *) e2);
    CU_ASSERT(omhtable_size(omm, htable) == 1);
    omhtable_delete(omm, htable, 1, (omhtentry *) e3);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    htable_entry_free(e1);
    htable_entry_free(e2);
    htable_entry_free(e3);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_delete_not_there()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    omhtable_delete(omm, htable, 0, (omhtentry *) e);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_add_already_there()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    omhtable_add(omm, htable, 0, (omhtentry *) e);
    omhtable_add(omm, htable, 0, (omhtentry *) e);
    omhtable_delete(omm, htable, 0, (omhtentry *) e);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_add_hash_too_large()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    omhtable_add(omm, htable, TEST_HASH_TABLE_SIZE * 2, (omhtentry *) e);
    omhtable_delete(omm, htable, TEST_HASH_TABLE_SIZE * 2, (omhtentry *) e);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_add_delete_lots()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e;
    int i;

    for (i = 0; i < 10000; i++) {
        e = htable_entry_new("dummy");
        omhtable_add(omm, htable, i, (omhtentry *) e);
    }
    omhtable_stats(omm, htable);
    for (i = 0; i < 10000; i++) {
        int offset = 0;
        CU_ASSERT((e = (htable_entry *) omhtable_get(omm, htable, i, &offset)) != NULL);
        omhtable_delete(omm, htable, i, (omhtentry *) e);
        htable_entry_free(e);
    }
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_find_first()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    htable_entry *e1 = htable_entry_new("dummy1");
    omhtable_add(omm, htable, 0, (omhtentry *) e);
    omhtable_add(omm, htable, 0, (omhtentry *) e1);
    CU_ASSERT(omhtable_find
              (omm, htable, (omhtable_cmp_fn) htable_entry_cmp, 0, "dummy") != NULL);
    omhtable_delete(omm, htable, 0, (omhtentry *) e1);
    omhtable_delete(omm, htable, 0, (omhtentry *) e);
    htable_entry_free(e1);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_find_second()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    htable_entry *e1 = htable_entry_new("dummy");
    omhtable_add(omm, htable, 0, (omhtentry *) e1);
    omhtable_add(omm, htable, 0, (omhtentry *) e);
    CU_ASSERT(omhtable_find
              (omm, htable, (omhtable_cmp_fn) htable_entry_cmp, 0, "dummy") != NULL);
    omhtable_delete(omm, htable, 0, (omhtentry *) e);
    omhtable_delete(omm, htable, 0, (omhtentry *) e1);
    htable_entry_free(e1);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_find_wrong_hash()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    omhtable_add(omm, htable, 0, (omhtentry *) e);
    CU_ASSERT(omhtable_find
              (omm, htable, (omhtable_cmp_fn) htable_entry_cmp, 1, "dummy") == NULL);
    omhtable_delete(omm, htable, 0, (omhtentry *) e);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_find_not_there()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    CU_ASSERT(omhtable_find
              (omm, htable, (omhtable_cmp_fn) htable_entry_cmp, 1, "dummy") == NULL);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_find_removed()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    htable_entry *e = htable_entry_new("dummy");
    omhtable_add(omm, htable, 0, (omhtentry *) e);
    omhtable_delete(omm, htable, 0, (omhtentry *) e);
    CU_ASSERT(omhtable_find
              (omm, htable, (omhtable_cmp_fn) htable_entry_cmp, 1, "dummy") == NULL);
    htable_entry_free(e);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_add_performance()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < TEST_ITERATIONS; i++) {
        htable_entry *e = htable_entry_new("dummy");
        entries = g_list_prepend(entries, e);
    }
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        omhtable_add(omm, htable, i, (omhtentry *) iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        omhtable_delete(omm, htable, i, (omhtentry *) iter->data);
        htable_entry_free((htable_entry *) iter->data);
    }
    g_list_free(entries);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_delete_performance()
{
    omhtable *htable = create_table(TEST_HASH_TABLE_SIZE);
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < TEST_ITERATIONS; i++) {
        htable_entry *e = htable_entry_new("dummy");
        entries = g_list_prepend(entries, e);
    }
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        omhtable_add(omm, htable, i, (omhtentry *) iter->data);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        omhtable_delete(omm, htable, i, (omhtentry *) iter->data);
        htable_entry_free((htable_entry *) iter->data);
    }
    g_list_free(entries);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

bool _htable_find_cmp_fn(omhtentry * e, void *data)
{
    return (strcmp(((htable_entry *) e)->str, (char *) data) == 0);
}

void _htable_find_performance(size_t buckets, size_t count)
{
    omhtable *htable = create_table(buckets);
    uint64_t start;
    int i;
    GList *entries = NULL;
    GList *iter;

    for (i = 0; i < count; i++) {
        char *s = g_strdup_printf("%x", rand());
        htable_entry *e = htable_entry_new(s);
        free(s);
        entries = g_list_prepend(entries, e);
        omhtable_add(omm, htable, omhtable_strhash(e->str), (omhtentry *) e);
    }
    entries = g_list_reverse(entries);
    start = get_time_us();
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        htable_entry *e = (htable_entry *) iter->data;
        CU_ASSERT(omhtable_find
                  (omm, htable, _htable_find_cmp_fn, omhtable_strhash(e->str),
                   e->str) != NULL);
    }
    printf("%" PRIu64 "us ... ", (get_time_us() - start));
    for (i = 0, iter = entries; iter; iter = iter->next, i++) {
        htable_entry *e = (htable_entry *) iter->data;
        omhtable_delete(omm, htable, omhtable_strhash(e->str), (omhtentry *) e);
        htable_entry_free(e);
    }
    g_list_free(entries);
    CU_ASSERT(omhtable_size(omm, htable) == 0);
    destroy_table(htable);
    CU_ASSERT(omavailable(omm) == TEST_HEAP_SIZE);
}

void test_htable_find_performance_32buckets()
{
    _htable_find_performance(32, TEST_ITERATIONS);
}

void test_htable_find_performance_250buckets()
{
    _htable_find_performance(250, TEST_ITERATIONS);
}

void test_htable_find_performance_1000buckets()
{
    _htable_find_performance(1000, TEST_ITERATIONS);
}

static CU_TestInfo tests_malloc[] = {
    {"attach", test_attach},
    {"malloc 0 bytes", test_malloc_0},
    {"free_null", test_free_null},
    {"malloc 1 byte", test_malloc1},
    {"malloc twice", test_malloc_twice},
    {"malloc twice reverse free", test_malloc_twice_reverse},
    {"malloc until fail", test_malloc_until_fail},
    {"malloc performance", test_malloc_performance},
    {"glib malloc performance", test_glib_malloc_performance},
    {"malloc performance fragmented", test_malloc_performance_fragmented},
    CU_TEST_INFO_NULL,
};

static CU_TestInfo tests_list[] = {
    {"add/remove", test_list_add_remove},
    {"remove not there", test_list_remove_not_there},
    {"prepend", test_list_prepend},
    {"append", test_list_append},
    {"length", test_list_length},
    {"get", test_list_get},
    {"reverse", test_list_reverse},
    {"concat", test_list_concat},
    {"find", test_list_find},
    {"sort", test_list_sort},
    {"prepend performance 5000 entries", test_list_prepend_performance},
    {"append performance 5000 entries", test_list_append_performance},
    {"glist append performance 5000 entries", test_glist_append_performance},
    {"find performance 5000 entries", test_list_find_performance},
    {"glist find performance 5000 entries", test_glist_find_performance},
    CU_TEST_INFO_NULL,
};

static CU_TestInfo tests_htable[] = {
    {"add/delete", test_htable_add_delete},
    {"size", test_htable_size},
    {"delete not there", test_htable_delete_not_there},
    // { "add already there", test_htable_add_already_there },
    {"hash too large", test_htable_add_hash_too_large},
    {"add/delete lots", test_htable_add_delete_lots},
    {"find first", test_htable_find_first},
    {"find second", test_htable_find_second},
    {"find wrong hash", test_htable_find_wrong_hash},
    {"find not there", test_htable_find_not_there},
    {"find removed", test_htable_find_removed},
    {"add performance 5000 entries 32 buckets", test_htable_add_performance},
    {"delete performance 5000 entries 32 buckets", test_htable_delete_performance},
    {"find performance 5000 entries 32 buckets", test_htable_find_performance_32buckets},
    {"find performance 5000 entries 250 buckets", test_htable_find_performance_250buckets},
    {"find performance 5000 entries 1000 buckets",
     test_htable_find_performance_1000buckets},
    CU_TEST_INFO_NULL,
};

static CU_SuiteInfo suites[] = {
    {"Malloc tests", suite_init, suite_shutdown, 0, 0, tests_malloc},
    {"List tests", suite_init, suite_shutdown, 0, 0, tests_list},
    {"Hash Table tests", suite_init, suite_shutdown, 0, 0, tests_htable},
    CU_SUITE_INFO_NULL,
};

int main(int argc, char **argv)
{
    const char *filter = argv[1];
    CU_SuiteInfo *suite;

    /* Initialize the CUnit test registry */
    assert(CU_initialize_registry() == CUE_SUCCESS);
    assert(CU_get_registry() != NULL);
    assert(!CU_is_test_running());

    /* Make some random numbers */
    srand(time(NULL));

    /* Add tests */
    suite = &suites[0];
    while (suite && suite->pName) {
        bool all = true;
        CU_pSuite pSuite;
        CU_TestInfo *test;

        /* Default to running all tests of a suite */
        if (filter && strstr(suite->pName, filter) != NULL)
            all = true;
        else if (filter)
            all = false;

        /* Add suite */
        pSuite = CU_add_suite(suite->pName, suite->pInitFunc, suite->pCleanupFunc);
        if (pSuite == NULL) {
            fprintf(stderr, "suite add failed - %s\n", CU_get_error_msg());
            exit(EXIT_FAILURE);
        }

        /* Add test */
        test = &suite->pTests[0];
        while (test && test->pName) {
            if (all || (filter && strstr(test->pName, filter) != NULL)) {
                if (CU_add_test(pSuite, test->pName, test->pTestFunc)
                    == NULL) {
                    fprintf(stderr, "test add failed - %s\n", CU_get_error_msg());
                    exit(EXIT_FAILURE);
                }
            }
            test++;
        }
        suite++;
    }

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_set_error_action(CUEA_IGNORE);
    CU_basic_run_tests();
    CU_cleanup_registry();

    return CU_get_error();
}
