/**
 * @file omem.c
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <glib.h>
#include "omem.h"

/* Meta stored at start and end of allocated blocks */
typedef struct om_meta {
    size_t mark;
} om_meta;

/* Macro's for manipulating block meta-data */
#define META_SIZE               sizeof(om_meta)
#define ALIGNMENT               8       /* Must be a power of 2 */
#define BLK_BASE(om)            ((size_t)om + sizeof(om_block) + om->headroom)
#define BLK_MIN_SIZE            ((2 * META_SIZE) + 8)
#define BLK_ALIGN(size)         (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define BLK_USED(m)             ((m)->mark & 1)
#define BLK_FREE(m)             (!BLK_USED((m)))
#define BLK_SIZE(m)             ((m)->mark & ~1L)
#define BLK_HEAD(m)             (m)
#define BLK_FOOT(m)             ((om_meta *)((uint8_t *)(m) + BLK_SIZE((m)) - META_SIZE))
#define BLK_SET(m,size,used)    {(m)->mark = ((size)|used); BLK_FOOT((m))->mark = (m)->mark;}
#define BLK_NEXT(m)             ((om_meta *)((uint8_t *)(m) + BLK_SIZE((m))))
#define BLK_PREV(m)             ((om_meta *)((uint8_t *)(m) - (BLK_SIZE(((om_meta *)((uint8_t *)(m) - META_SIZE))))))

/* Print a pretty histogram of the block sizes */
#define HISTOGRAM_NUM_BUCKETS 28
#define HISTOGRAM_BUCKET_SIZE 8
void omstats(om_block * om)
{
    size_t histogram[HISTOGRAM_NUM_BUCKETS] = { 0 };
    size_t full = 0;
    size_t empty = 0;
    size_t used = 0;
    size_t free = 0;
    size_t max = 0;
    size_t scale = 0;
    size_t max_bucket = 0;
    size_t min_bucket = HISTOGRAM_NUM_BUCKETS;
    size_t i, j;

    om_meta *bp = (om_meta *) BLK_BASE(om);
    while ((size_t) bp < (BLK_BASE(om) + om->size)) {
        size_t size = BLK_SIZE(bp);
        if (BLK_FREE(bp)) {
            free += size;
            empty++;
            bp = BLK_NEXT(bp);
            continue;
        }
        int bucket = 0;
        int power = 1;
        while (power < size) {
            power *= 2;
            bucket++;
        }
        histogram[bucket] += 1;
        used += size;
        full++;
        max_bucket = (bucket > max_bucket) ? bucket : max_bucket;
        min_bucket = (bucket < min_bucket) ? bucket : min_bucket;
        bp = BLK_NEXT(bp);
    }

    for (i = 0; i < HISTOGRAM_NUM_BUCKETS; i++) {
        max = (histogram[i] > max) ? histogram[i] : max;
    }
    scale = (max > 50) ? max / 50 : 1;

    printf("\nHeap size: %zu bytes\n", om->size);
    printf("Used: %zu blocks (%zu bytes)\n", full, used);
    printf("Free: %zu blocks (%zu bytes)\n", empty, free);
    for (i = min_bucket; i <= max_bucket; i++) {
        printf("%10zu ", 1UL << i);
        for (j = 0; j < histogram[i] / scale; j++) {
            putchar('x');
        }
        if (histogram[i])
            printf(" (%zu)", histogram[i]);
        printf("\n");
    }
}

/* Return how much memory is still available */
size_t omavailable(om_block * om)
{
    size_t free = 0;
    if (om) {
        om_meta *bp = (om_meta *) BLK_BASE(om);
        while ((size_t) bp < (BLK_BASE(om) + om->size)) {
            if (BLK_FREE(bp))
                free += BLK_SIZE(bp);
            bp = BLK_NEXT(bp);
        }
    }
    return free;
}

/* Given pointer to free block header, coalesce with adjacent blocks and
 * return pointer to coalesced block */
static void *coalesce(om_block * om, om_meta * bp)
{
    /* Check if there is a previous block */
    if (BLK_BASE(om) < (size_t) bp) {
        om_meta *prev = BLK_PREV(bp);

        /* Check if the previous block is free */
        if (BLK_FREE(prev)) {
            BLK_SET(prev, BLK_SIZE(prev) + BLK_SIZE(bp), 0);
            if (om->next == ((size_t) bp - BLK_BASE(om)))
                om->next = (size_t) prev - BLK_BASE(om);
            bp = prev;
        }
    }

    /* Check if there is a next block that is free */
    om_meta *next = BLK_NEXT(bp);
    if ((size_t) next < (BLK_BASE(om) + om->size) && BLK_FREE(next)) {
        if (om->next == ((size_t) next - BLK_BASE(om)))
            om->next = (size_t) bp - BLK_BASE(om);
        BLK_SET(bp, BLK_SIZE(bp) + BLK_SIZE(next), false);
    }
    return bp;
}

/* Search the heap for a free block ≤ required size */
static void *find_fit(om_block * om, size_t size)
{
    size_t checked = 0;
    om_meta *bp;

    bp = (om_meta *) (BLK_BASE(om) + om->next);
    while (1) {
        if (checked >= om->size)
            break;
        if ((size_t) bp >= (BLK_BASE(om) + om->size))
            bp = (om_meta *) BLK_BASE(om);
        if (BLK_FREE(bp) && BLK_SIZE(bp) >= size)
            return bp;
        checked += BLK_SIZE(bp);
        bp = BLK_NEXT(bp);
    }
    return NULL;
}

void *omalloc(om_block * om, size_t size)
{
    size_t blk_size;
    om_meta *bp;

    if (!size)
        return 0;
    blk_size = BLK_ALIGN(size + (2 * META_SIZE));
    blk_size = blk_size > BLK_MIN_SIZE ? blk_size : BLK_MIN_SIZE;

    bp = find_fit(om, blk_size);
    if (!bp) {
        assert(bp && "om_block exhausted");
        return 0;
    }
    om->next = (size_t) bp - BLK_BASE(om);

    if (blk_size < BLK_SIZE(bp) && (BLK_SIZE(bp) - blk_size) > BLK_MIN_SIZE) {
        om_meta *next = (om_meta *) ((uint8_t *) bp + blk_size);
        BLK_SET(next, BLK_SIZE(bp) - blk_size, false);
    }
    BLK_SET(bp, blk_size, true);

    return (void *) ((uint8_t *) bp + META_SIZE);
}

void omfree(om_block * om, void *m)
{
    if (m) {
        om_meta *bp;

        bp = (om_meta *) ((uint8_t *) m - META_SIZE);
        BLK_SET(bp, BLK_SIZE(bp), false);
        coalesce(om, bp);
    }
}

om_block *omcreate(const char *fname, size_t rsize, size_t headroom)
{
    om_block *om = NULL;
    size_t pgsz = sysconf(_SC_PAGE_SIZE);
    bool already_init = false;
    key_t key;
    int shmid = 0;
    om_meta *bp;

    size_t size = sizeof(om_block) + headroom + rsize;
    size = (((size) + (pgsz) - 1) & ~((pgsz) - 1));

    if (fname != NULL) {
        key = ftok(fname, 'R');
        if (key < 0) {
            perror("ftok");
            return NULL;
        }

        shmid = shmget(key, size, 0644 | IPC_CREAT | IPC_EXCL);
        if (shmid < 0) {
            /* Another process is initializing this memory */
            shmid = shmget(key, size, 0644);
            already_init = 1;
        }

        om = (om_block *) shmat(shmid, (void *) 0, 0);
        if (om == (om_block *) (-1)) {
            perror("shmat");
            return NULL;
        }

        if (already_init) {
            /* Wait for the other process to finish if required */
            while (shmid != om->shmid)
                usleep(10);
            if (om->size != rsize) {
                /* Incompatible shared memory segments! */
                shmdt(om);
                return NULL;
            }
            return om;
        }
    } else {
        om = (om_block *) malloc(size);
    }

    om->shmid = 0;
    om->size = rsize;
    om->headroom = headroom;
    memset((void *) BLK_BASE(om), 0, rsize);
    bp = (om_meta *) BLK_BASE(om);
    BLK_SET(bp, rsize, false);
    om->next = 0;
    om->shmid = shmid;
    return om;
}

void omdestroy(om_block * om)
{
    if (!om)
        return;
    if (om->shmid) {
        shmdt(om);
    } else {
        free(om);
    }
    return;
}
