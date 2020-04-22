/*
 * next fit
 * 
 * 42 + 40
 */
#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "afkbrb",
    /* First member's full name */
    "afkbrb",
    /* First member's email address */
    "2w6f8c@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define DEBUG 0

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)

char *heap_listp;

#define OFFSET(bp) ((char *)(bp)-heap_listp)

size_t prev_size = 0;
char *last_allocated_bp = NULL;

static void *merge(void *bp) {
    if (DEBUG) printf("merge @%d\n", OFFSET(bp));
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        if (last_allocated_bp == bp) {
            last_allocated_bp = PREV_BLKP(bp);
        }
        return bp;
    } else if (prev_alloc && !next_alloc) {
        if (last_allocated_bp == bp || last_allocated_bp == NEXT_BLKP(bp)) {
            last_allocated_bp = PREV_BLKP(bp);
        }
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        if (last_allocated_bp == PREV_BLKP(bp) || last_allocated_bp == bp) {
            last_allocated_bp = PREV_BLKP(PREV_BLKP(bp));
        }
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        if (last_allocated_bp == PREV_BLKP(bp) || last_allocated_bp == bp ||
            last_allocated_bp == NEXT_BLKP(bp)) {
            last_allocated_bp = PREV_BLKP(PREV_BLKP(bp));
        }
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

static void *extend_heap(size_t words) {
    if (DEBUG) printf("expand heap\n");
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // end

    return merge(bp);
}

static void *find_fit(size_t asize) {
    // last_allocated_bp is allocated, so we don't need to check it
    // char *p = NEXT_BLKP(last_allocated_bp);
    char *p = last_allocated_bp;
    size_t size = GET_SIZE(HDRP(p));
    while (size) {  // search until reach end block
        if (!GET_ALLOC(HDRP(p)) && size >= asize + 2 * WSIZE) {
            last_allocated_bp = p;
            prev_size = asize;
            return p;
        }
        p = NEXT_BLKP(p);
        size = GET_SIZE(HDRP(p));
    }

    p = heap_listp;
    while (p != last_allocated_bp) {
        size = GET_SIZE(HDRP(p));
        if (!GET_ALLOC(HDRP(p)) && size >= asize + 2 * WSIZE) {
            last_allocated_bp = p;
            prev_size = asize;
            return p;
        }
        p = NEXT_BLKP(p);
    }

    return NULL;
}

static void place(void *bp, size_t asize) {
    if (DEBUG) printf("place (@%d, %d)\n", OFFSET(bp), asize);
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + 1 * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += 2 * WSIZE;
    last_allocated_bp = heap_listp;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    if (DEBUG) printf("malloc %d\n", size);

    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) return NULL;

    if (size <= DSIZE) {
        asize = 2 * DSIZE;  // header + footer + size
    } else {
        asize = ALIGN(size + DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL;
    place(bp, asize);

    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    if (DEBUG) printf("free (@%d, %d)\n", OFFSET(bp), size);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    merge(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    if (DEBUG) printf("realloc (@%d, %d)\n", OFFSET(ptr), size);
    void *newptr;
    size_t csize;

    if ((newptr = mm_malloc(size)) == NULL) return NULL;
    csize = GET_SIZE(HDRP(ptr));
    if (size < csize) csize = size;
    memcpy(newptr, ptr, csize);
    mm_free(ptr);
    return newptr;
}
