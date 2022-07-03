/*
 * mm.c
 *
 * Name: Group member :How Yeh Wan; Li Peng
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read malloclab.pdf carefully and in its entirety before beginning.
 * 
 * 
 * This homework use some reference from text book that was list in the commecnt section 
 * Heap checker -->to do list not finished yet
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

#define WORDSIZE 8             /* Word and header/footer s (bytes) */
#define DOUBLESIZE 16            /* Double word s (bytes) */
#define CHUNKSIZE (1<<12)   /* Extend heap by this amount (bytes) */
static void *heap_listp;    /* Extend heap by this amount (bytes) */


/* ---------------------------- helper functions from textbook ---------------------------- */
/*-------CMPSC 473 - Operating Systems Design and Construction / Urgaonkar, Bhuvan / summer 2021----*/
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t s);
static void place(void *bp,size_t asize);

/* Get a larger value */
static size_t MAX(size_t x, size_t y) {
    
    if(x > y)
        return x;
    else
        return y;
    
}

/* Pack a s and allocated bit into a word */
static size_t PACK(size_t s, size_t alloc) {
    return (s | alloc);
}

/* Read and write a word at address p */
static size_t GET(char *p) {
    return (*(size_t *)(p));
}
static void PUT(char *p, size_t value) {
    (*(size_t *)(p)) = value;
}

/* Read the s and allocated fields from address p */
static size_t GET_SIZE(char *p) {
    return (size_t)GET(p) & ~0X7;
}
static size_t GET_ALLOC(char *p) {
    return (size_t)GET(p) & 0x1;
}

/* Given block ptr bp, compute address of its header and footer */
static char *HDRP(char *bp) {
    return ((char *)(bp) - WORDSIZE);
}
static char *FTRP(char *bp) {
    return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DOUBLESIZE);
}

/* Given block ptr bp, compute address of next and previous blocks */
static char *NEXT_BLKP(char * bp) {
    return ((char *)(bp) + GET_SIZE((char *)(bp) - WORDSIZE));
}
static char *PREV_BLKP(char * bp) {
    return ((char *)(bp) - GET_SIZE((char *)(bp) - DOUBLESIZE));
}

/* Extend heap of "words" s */
static void *extend_heap(size_t words) {

    char *bp;
    size_t s;

    // Allocate an even number of words to maintain alignment
    s = (words % 2) ? (words + 1) * WORDSIZE : words * WORDSIZE;
    if((long)(bp = mem_sbrk(s)) == -1) {
        return NULL;
    }

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(s, 0));
    PUT(FTRP(bp), PACK(s, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // Coalesce if the previous block was free
    return coalesce(bp);
}

/* Coalesce free blocks */
static void *coalesce(void *bp) {

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t s = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) {              // Case 1: if both prev and next blocks are allocated
        return bp;
    }
    else if(prev_alloc && !next_alloc) {        // Case 2: if next block is free
        s += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        // update headter and footer
        PUT(HDRP(bp), PACK(s, 0));
        PUT(FTRP(bp), PACK(s, 0));
    }
    else if(!prev_alloc && next_alloc) {        // Case 3: if prev block is free
        s += GET_SIZE(HDRP(PREV_BLKP(bp)));

        // update headter and footer
        PUT(FTRP(bp), PACK(s, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(s,0));

        // points to prev
        bp = PREV_BLKP(bp);
    }
    else {                                      // Case 4: if both of them are free
        s += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));

        // update headter and footer
        PUT(HDRP(PREV_BLKP(bp)), PACK(s, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(s, 0));

        // points to prev
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* first-fit*/
static void *find_fit(size_t asize) {

    void *bp;
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

/* Place block into free block */
static void place(void *bp,size_t asize) {

    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2 * DOUBLESIZE)) {    // split when remainning part >= block s
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }else {                                 // otherwise just place the block
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* ---------------------------- helper functions from textbook ---------------------------- */
/*-------CMPSC 473 - Operating Systems Design and Construction / Urgaonkar, Bhuvan / summer 2021----*/


/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{

    // Reference from textbook
/*-------CMPSC 473 - Operating Systems Design and Construction / Urgaonkar, Bhuvan / summer 2021----*/
    // Create the initial empty heap
    if((heap_listp = mem_sbrk(4 * WORDSIZE)) == (void *) - 1) {
        return false;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WORDSIZE), PACK(DOUBLESIZE, 1));
    PUT(heap_listp + (2 * WORDSIZE), PACK(DOUBLESIZE, 1));
    PUT(heap_listp + (3 * WORDSIZE), PACK(0, 1));
    heap_listp += (2 * WORDSIZE);

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if(extend_heap(CHUNKSIZE / WORDSIZE) == NULL) {
        return false;
    }
    return true;

}

/*
 * malloc
 */
void* malloc(size_t s)
{

    // Reference from textbook
/*-------CMPSC 473 - Operating Systems Design and Construction / Urgaonkar, Bhuvan / summer 2021----*/
    size_t asize;
    size_t extendsize;
    char *bp;

    if(s == 0) {
        return NULL;
    }

    // Adjust block s to include overhead and alignment reqs
    if(s <= DOUBLESIZE) {
        asize = 2 * DOUBLESIZE;
    }
    else {
        asize = DOUBLESIZE * ((s + (DOUBLESIZE) + (DOUBLESIZE - 1)) / DOUBLESIZE);
    }

    // Search the free list for a fit
    if((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // No fit found. Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize / WORDSIZE)) == NULL){
        return NULL;
    }
    place(bp,asize);
    return bp;
}

/*
 * free
 */
void free(void* ptr)
{

    // Reference from textbook
/*-------CMPSC 473 - Operating Systems Design and Construction / Urgaonkar, Bhuvan / summer 2021----*/
    size_t s = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(s, 0));
    PUT(FTRP(ptr), PACK(s, 0));
    coalesce(ptr);
}

/*
 * realloc
 */

/*
 if ptr is NULL, the call is equivalent to malloc(size);
 if size is equal to zero, the call is equivalent to free(ptr);
 if ptr is not NULL, it must have been returned by an earlier call to malloc, calloc, or realloc.
 The call to realloc changes the size of the memory block pointed to by ptr (the old block) to size
*/
void* realloc(void* preptr, size_t s)
{
    s = align(s);
    size_t blksize = GET_SIZE(HDRP(preptr));
    void *postptr = malloc(s);

    if(s == 0) {                 // If s is equal to zero                                
        free(preptr);            //the call is equivalent to free(ptr)
        return NULL;
    }
    if(preptr == NULL) {           // If ptr is NULL,
        return malloc(s);          //  the call is equivalent to malloc(s)
    }

    // copy data of preptr to postptr
    if(s < blksize) {
        blksize = s;
    }
    memcpy(postptr, preptr, blksize);

    // free old ptr and return new ptr
    free(preptr);
    return postptr;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 to do list
 */
bool mm_checkheap(int lineno) //not finish yet
{
    #ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    #endif /* DEBUG */
    return true;
}