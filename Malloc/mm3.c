/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Loser",
    /* First member's full name */
    "EI Madrigal",
    /* First member's email address */
    "renjingtao1997@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* if DEBUG is defined, enable printing on dbg_printf and contracts */
//#define DEBUG

#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#define CHECKHEAP(verbose) mm_checkheap(verbose)
#else
#define DBG_PRINTF(...)
#define CHECKHEAP(verbose)
#endif

#define MAXLISTS 16

/* Private variables */
static char *heap_listp;
void *seg_list[MAXLISTS];

/* Basic constants and macros */
#define WSIZE 4                     /* Word and header/footer size (bytes) */
#define DSIZE 8                     /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12)         /* Extend heap by this amount (bytes) */  

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & (~0x7))
#define GET_ALLOC(p) (GET(p) & (0x1))

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* compute prev ptr's pos by bp */
#define PREV_POS(bp) ((char *)(bp))
#define NEXT_POS(bp) ((char *)(bp) + WSIZE) 


/* dereference the prev and next ptr */
#define PREV(bp) (*(char **)(PREV_POS(bp)))
#define NEXT(bp) (*(char **)(NEXT_POS(bp)))

/* set the value of prev and next field */
#define SET_PTR(from, to) (*(unsigned int *)(from) = (unsigned int)(to))

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void insert(void *bp, size_t size);
static void delete(void *bp);
static void *place(void *bp, size_t asize);
void mm_checkheap(int verbose);
void checkheap(int verbose);
static void printblock(void *bp);
static void checkblock(void *bp);
static void printlist(void *bp, size_t size);
static void checklist(void *bp, size_t size);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    for (int i = 0; i < MAXLISTS; ++i) {
        seg_list[i] = NULL;
    }

    PUT(heap_listp, 0);   /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2 * WSIZE);
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    CHECKHEAP(1);
    return 0;
}

/* 
 * mm_malloc - Allocates a block from the free list
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    if (size == 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    }
    else {
        asize = 2 * DSIZE + DSIZE * ((size - 1) / DSIZE);
    }

    size_t cur_class_size = 1;
    for (int i = 0; i < MAXLISTS; ++i) {
        if (seg_list[i] != NULL && cur_class_size >= asize) {
            bp = seg_list[i];
            // find approriate block in cur list
            while (bp != NULL && GET_SIZE(HDRP(bp)) < asize) {
                bp = NEXT(bp);
            }
            if (bp != NULL) {
                bp = place(bp, asize);
                return bp;
            }
        }
        cur_class_size *= 2;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    bp = place(bp, asize);
    CHECKHEAP(1);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    /* Insert the free block into the seg list */
    insert(ptr, size);
    coalesce(ptr);
    CHECKHEAP(1);
}

void insert(void *ptr, size_t size) {
    /* find the right list */
    int class_num;
    size_t ssize = size;
    for (class_num = 0; class_num < MAXLISTS - 1; ++class_num) {
        if (ssize <= 1) {
            break;
        }
        ssize >>= 1;
    }
    void *list = seg_list[class_num];
    void *pre = NULL;
    /* find right place to insert in order to make cur list ordered */
    while (list != NULL && GET_SIZE(HDRP(list)) < size) {
        pre = list;
        list = NEXT(list);
    }

    if (list != NULL) {
        if (pre != NULL) {
            SET_PTR(PREV_POS(ptr), pre);
            SET_PTR(NEXT_POS(pre), ptr);
            SET_PTR(NEXT_POS(ptr), list);
            SET_PTR(PREV_POS(list), ptr);
        }
        /* insert at first */
        else {
            seg_list[class_num] = ptr;
            SET_PTR(PREV_POS(list), ptr);
            SET_PTR(NEXT_POS(ptr), list);
            SET_PTR(PREV_POS(ptr), NULL);
        }
    }
    else {
        /* insert at last */
        if (pre != NULL) {
            SET_PTR(PREV_POS(ptr), pre);
            SET_PTR(NEXT_POS(pre), ptr);
            SET_PTR(NEXT_POS(ptr), NULL);
        }
        /* cur list is empty */
        else {
            seg_list[class_num] = ptr;
            SET_PTR(PREV_POS(ptr), NULL);
            SET_PTR(NEXT_POS(ptr), NULL);
        }
    }
}

/*
 * mm_realloc - Implemented a stand-alone realloc
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (size <= DSIZE) {
        size = 2 * DSIZE;
    }
    else {
        size = 2 * DSIZE + DSIZE * ((size - 1) / DSIZE);
    }

    /* if the req size is less than the old */
    if (GET_SIZE(HDRP(ptr)) >= size) {
        return ptr;
    }

    void* newptr = ptr;
    size_t total = GET_SIZE(HDRP(NEXT_BLKP(ptr))) + GET_SIZE(HDRP(ptr)); 
    /* check if the next block is free, if it is, just add to the old block */
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && total >= size) {
        // delete the free block we used 
        delete(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total, 1));
        PUT(FTRP(ptr), PACK(total, 1));
    }
    /* it is the last block */
    else if (!GET_SIZE(HDRP(NEXT_BLKP(ptr))) || (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && !GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(ptr)))))) {
        if (total < size) {
            if (extend_heap(MAX(size - total, CHUNKSIZE) / WSIZE) == NULL) {
                return NULL;
            }
            total += MAX(size - total, CHUNKSIZE);
        }
        delete(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total, 1));
        PUT(FTRP(ptr), PACK(total, 1));
    }
    /* no continue free block, can only copy and paste */
    else {
        newptr = mm_malloc(size);
        if (newptr == NULL) {
            return NULL;
        }
        memcpy(newptr, ptr, GET_SIZE(HDRP(ptr)) - DSIZE);
        mm_free(ptr);
    }
    CHECKHEAP(1);
    return newptr;
}

void delete(void *ptr) {
    int size = GET_SIZE(HDRP(ptr));
    int classnum = 0;
    for (classnum = 0; classnum < MAXLISTS - 1; ++classnum) {
        if (size <= 1) {
            break;
        }
        size >>= 1;
    }

    if (PREV(ptr) != NULL) {
        if (NEXT(ptr) != NULL) {
            SET_PTR(NEXT_POS(PREV(ptr)), NEXT(ptr));
            SET_PTR(PREV_POS(NEXT(ptr)), PREV(ptr));
        }
        /*delete last one*/
        else {
            SET_PTR(NEXT_POS(PREV(ptr)), NULL);
        }
    }
    /* first one*/
    else {
        if (NEXT(ptr) != NULL) {
            seg_list[classnum] = NEXT(ptr);
            SET_PTR(PREV_POS(NEXT(ptr)), NULL);
        }
        /*nothing to do*/
        else {
            seg_list[classnum] = NULL;
        }
    }
}

int mm_check(void) {
    return 0;
}

/*
 * mm_check - Check the heap for correctness
 */
void mm_checkheap(int verbose) {
   checkheap(verbose);
}

static void printblock(void *bp) {
    size_t hsize = GET_SIZE(HDRP(bp));
    size_t halloc = GET_ALLOC(HDRP(bp));
    size_t fsize = GET_SIZE(FTRP(bp));
    size_t falloc = GET_ALLOC(FTRP(bp));
    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }
    printf("%p: header: [%u:%c] footer: [%u:%c]\n", bp, 
                    hsize, (halloc ? 'a' : 'f'),
                    fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *bp) {
    if ((size_t)bp % 8) {
        printf("Error: %p is not doubleword aligned\n", bp);
    }
    if (GET(HDRP(bp)) != GET(FTRP(bp))) {
        printf("Error: header does not match footer\n");
    }
}

static void printlist(void *head, size_t size) {
    for (; head != NULL; head = NEXT(head)) {
        size_t hsize = GET_SIZE(HDRP(head));
        size_t halloc = GET_ALLOC(HDRP(head));
        printf("[Node %u] %p: header: [%u:%c] prev: [%p] next: [%p]\n", size, 
                    head, hsize, (halloc ? 'a' : 'f'),
                    PREV(head), NEXT(head));
    }
}

static void checklist(void *head, size_t size) {
    void *pre = NULL;
    for (; head != NULL; head = NEXT(head)) {
        if (PREV(head) != pre) {
            printf("Error: prev ptr error\n");
        }
        if (pre != NULL && NEXT(pre) != head) {
            printf("Error: next ptr error\n");
        }
        size_t hsize = GET_SIZE(HDRP(head));
        size_t halloc = GET_ALLOC(HDRP(head));
        if (halloc) {
            printf("Erro: free node should be free\n");
        }
        if (pre && GET_SIZE(HDRP(pre)) > hsize) {
            printf("Error: cur list size order is wrong\n");
        }
        if (hsize < size || (size != (1 << 15) && (hsize > (size << 1) - 1))) {
            printf("Error: cur block size error\n");
        }
        pre = head;
    }
}

// minimal check of the heap for consistency
void checkheap(int verbose) {
    char *bp = heap_listp;
    if (verbose) {
        printf("Heap (%p):\n", heap_listp);
    }

    if (GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp))) {
        printf("Bad prologue header\n");
    }
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose) {
            printblock(bp);
        }
        checkblock(bp);
    }

    // list level
    for (int i = 0, size = 1; i < MAXLISTS; ++i, size *= 2) {
        if (verbose) {
            printlist(seg_list[i], size);
        }
        checklist(seg_list[i], size);
    }

    if (verbose) {
        printblock(bp);
    }

    if (GET_SIZE(HDRP(bp)) || !(GET_ALLOC(HDRP(bp)))) {
        printf("%p %d\n", bp, GET_ALLOC(HDRP(bp)));
        printf("Bad epilogue header\n");
    }
}

static void *extend_heap(size_t words) {
   /* Allocate an even number of words to maintain alignment */
   size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
   char *bp;
   if ((bp = mem_sbrk(size)) == (void *)-1) {
       return NULL;
   }

   /* Initialize free block header/footer and the epilogue header */
   PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
   PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
   PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

   insert(bp, size);

   /* Coalesce if the previous block was free */
   return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1 */
    if (prev_alloc && next_alloc) {
        return bp;
    }

    /* Case 2 */
    else if (prev_alloc && !next_alloc) {
        delete(bp);
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* Case 3 */
    else if (!prev_alloc && next_alloc) {
        delete(bp);
        delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* Case 4 */
    else {
        delete(bp);
        delete(NEXT_BLKP(bp));
        delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert(bp, size);
    return bp;
}

/* Place the req block at the beginning of the free block, 
 * splitting only if the size of the remainder would equal or exceed the min block size */
static void* place(void *bp, size_t asize) {
    size_t size = GET_SIZE(HDRP(bp));

    delete(bp);
    if (size - asize < 2 * DSIZE) {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }

    else if (asize >= 96) {
        PUT(HDRP(bp), PACK(size - asize, 0));
        PUT(FTRP(bp), PACK(size - asize, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        insert(bp, size - asize);
        return NEXT_BLKP(bp);
    }

    /* Need split */
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
        insert(NEXT_BLKP(bp), size - asize);
    }
    return bp;
}

