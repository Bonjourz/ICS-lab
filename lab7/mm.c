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

/* For this lab, I use the strategy of segregated fit and explicit
 * free lists. I only do some modify based on the code from the textbook,
 * and for the function of realloc, I do some optimize according to the
 * trace file. For the number of free lists and the size of each list,
 * after test, I find the set in my program can get the best performance.
 * The structure of free block is sa following:
 * ---------------------------------------------------------------------
 *            the size of block              |               a/f
 * ---------------------------------------------------------------------
 *				The address of predecessor block in the free list
 * ---------------------------------------------------------------------
 *				The addrss of successor block in the free list
 * ---------------------------------------------------------------------
 *
 *
 *
 *								padding(optional)
 *
 *
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "515030910298",
    /* First member's full name */
    "Zhu Bojun",
    /* First member's email address */
    "869502588@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define MAX_HEAP (1 << 12)

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define MAX(x, y) ((x) > (y)? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)	((char *)(bp) - WSIZE)
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given free block ptr bp, compute address of predecessor and successor
 * free block in the free list */
#define PRED_BLKP(bp)	((void *)GET(bp))
#define SUCC_BLKP(bp)	((void *)GET((char *)(bp) + WSIZE))

/* Set the predecessor and successor free block in the free list of the 
 * free block */
#define SET_PRED(bp, val)		(*(unsigned int *)(bp) = (val))	
#define SET_SUCC(bp, val)		(*(unsigned int *)((char *)(bp) + WSIZE) = (val))

/* Used in the mm_check(), mark the block visited and get the 
 * visit information of one block */
#define MARK_VISITED(p)			(PUT((p), GET(p) & (1 << 1)))
#define VISITED(p)				(GET(p) & (1 << 1))

#define MIN_BLK_SIZE		16		/* The minimum size of a free block */

/* Given the index and get the address of the header of the corresponding free block */
#define GET_HEADER(bp, index)	((char *)(bp) + (index) * DSIZE)

#define LIST_NUM		6			/* The number of free list */
#define CHUNKSIZE		48			/* Extend heap by this amount (bytes) */ 

/* Declaration of funtion */
static size_t mm_check(void);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void delete_block(void *bp);
static void insert_block(void *bp, size_t size);
static void realloc_coalesce(void *bp, size_t alloc_size);
static size_t get_index(size_t size);
void *mm_realloc(void *ptr, size_t size);
int mm_init(void);
void mm_free(void *bp);
void *mm_malloc(size_t size);

static void *heap_listp = NULL;

/* The maximum size(bytes) of corresponding free list */
static int list_size[LIST_NUM] = {64, 128, 256, 512, 1024};

/* Given the size of new free block, return the index of free 
 * list it should be 
*/
static size_t get_index(size_t size){
	int i = 0;
	for (i; i < LIST_NUM; i++) {
		if (size <= list_size[i])
			return i;
	}

	return (LIST_NUM - 1);
}

/* Check the heap consistency, if there is something wrong 
 * with the heap, print out the information and return 1,
 * else return 0
*/
static size_t mm_check(void){
	/* init */
	void *heap_tail = (void *)((char *)mem_heap_hi() + 1);	/* The tail byte of
															   the heap plus 1(bytes) */
	size_t index = 0;
	void *header = NULL;
	void *bp = NULL;
	
	/* Check if every block in the free list
	 * is marked as free */
	for (index; index < LIST_NUM; index++) {
		header = GET_HEADER(heap_listp, index);
		for (bp = SUCC_BLKP(header); bp != NULL; bp = SUCC_BLKP(header)) {
			if (VISITED(HDRP(bp))) {
				printf("The block whose address from %p to %p is in two more list!\n",
						HDRP(bp), FTRP(bp));
				return 1;
			}

			else if (GET_ALLOC(HDRP(bp))) {
				printf("The block whose address from %p to %pin the free list is not free!\n",
						HDRP(bp), FTRP(bp));
				return 1;
			}
			
			MARK_VISITED(HDRP(bp));
		}
	}

	/* Check if every free block is actually in free list */
	for (bp = heap_listp; bp = NEXT_BLKP(bp); bp <= heap_tail) {
		if (VISITED(HDRP(bp)) == 0 && GET_ALLOC(HDRP(bp)) == 0){
			printf("The free block whose address from %p to %pis not in the free list!\n",
					HDRP(bp), FTRP(bp));
			return 1;
		}
		MARK_VISITED(HDRP(bp));
	}

	/* Check if there are any contiguous free blocks that somehow
	 * escaped coalescing */
	for (bp = heap_listp; bp = NEXT_BLKP(bp); NEXT_BLKP(bp) < heap_tail) {
		if (GET_ALLOC(HDRP(bp)) == 0)
			if (GET_ALLOC(FTRP(PREV_BLKP(bp))) == 0 || 
					GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 0){
				printf("Ouch! The free block whose address from %p to %p somehow escaped coalescing!\n",
						HDRP(bp), FTRP(bp));
				return 1;
			}
	}

	/* Check consistency of the heap */
	for (bp = heap_listp; bp = NEXT_BLKP(bp); NEXT_BLKP(bp) < heap_tail);
	if (NEXT_BLKP(bp) != heap_tail) {
		printf("The heap is not consistent!\n");
		return 1;
	}
	return 0;



}

int mm_init(void){
	/* The total size of prologue header */
	size_t init_blk_size = DSIZE + DSIZE * LIST_NUM; 

	/* Create the initial empty heap */
	if ((heap_listp = mem_sbrk(init_blk_size + DSIZE)) == (void *)-1)
		return -1;

	PUT(heap_listp, 0); /* Alignment padding */
	PUT(heap_listp + WSIZE, PACK(init_blk_size, 1)); /* Prologue header */
	PUT(heap_listp + init_blk_size, PACK(init_blk_size, 1)); /* Prologue footer */
	PUT(heap_listp + WSIZE + init_blk_size, PACK(0, 1)); /* Epilogue header */
	heap_listp += (2*WSIZE);

	/* Set the header of every free list size */
	int i = 0;
	void *header = NULL;
	for (i; i < LIST_NUM; i++){
		header = GET_HEADER(heap_listp, i);

		/* Set the left and right free block(NULL, NULL) of each header */
		SET_PRED(header, NULL);
		SET_SUCC(header, NULL);
	}

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	void *ptr = NULL;
	if ((ptr = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
		return -1;
	
	/* insert free block to the corresponding list */
	insert_block(ptr, GET_SIZE(HDRP(ptr))); 
	return 0;
}

static void *extend_heap(size_t words){
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
	PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

	/* Not the coalesce the free block in this function,
	 * but it will be coalesced in other function */
	return bp;
}

void mm_free(void *bp){
	size_t size = GET_SIZE(HDRP(bp));

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}

static void *coalesce(void *bp){
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
	
	if (prev_alloc && !next_alloc) {			/* Case 1 */
		delete_block(NEXT_BLKP(bp));

    	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    	PUT(HDRP(bp), PACK(size, 0));
    	PUT(FTRP(bp), PACK(size,0));
	}
	else if (!prev_alloc && next_alloc) {		/* Case 2 */
		delete_block(PREV_BLKP(bp));

    	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    	PUT(FTRP(bp), PACK(size, 0));
    	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    	bp = PREV_BLKP(bp);
	}
	else if (!prev_alloc && !next_alloc){		/* Case 3 */
		delete_block(PREV_BLKP(bp));
		delete_block(NEXT_BLKP(bp));

    	size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
			GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	/* Case 4, do nothing */

	insert_block(bp, size);
	return bp;
}


void *mm_malloc(size_t size){
	size_t asize;		/* Adjusted block size */
	size_t extendsize;	/* Amount to extend heap if no fit */
	char *bp;

	/* Ignore spurious requests */
	if (size == 0)
    	return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
    	asize = 2*DSIZE;
	else
    	asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

	/* Search the free list for a fit */
	
	bp = find_fit(asize);

	if (bp != NULL) {
		delete_block(bp);
    	place(bp, asize);
    	return bp;
	}

	/* No fit found. Get more memory and place the block */

	extendsize = MAX(asize,CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize){
	/* First fit search */
	void *bp;
	
	/* Get the corresponding header of free list according to the size */
	void *header = NULL;
	size_t index = get_index(asize);
	header = GET_HEADER(heap_listp, index);
	bp = SUCC_BLKP(header);
	
	for(bp; bp != NULL; bp = SUCC_BLKP(bp)) {
		if (asize <= GET_SIZE(HDRP(bp))) 
			return bp;
	}
	
	return NULL; /* No fit */
}

static void place(void *bp, size_t asize){
	size_t csize = GET_SIZE(HDRP(bp));
	
	if ((csize - asize) >= MIN_BLK_SIZE) {
    	PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
        void *ptr = NEXT_BLKP(bp);
        PUT(HDRP(ptr), PACK(csize-asize, 0));
		PUT(FTRP(ptr), PACK(csize-asize, 0));

		coalesce(ptr);			/* Coalesce the free block */
	}
	else {
       	PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* Delete one block from the free list */
static void delete_block(void *bp){
	void *pred = PRED_BLKP(bp);
	void *succ = SUCC_BLKP(bp);

	SET_SUCC(pred, succ);

	/* If the block is the end of the free list, don't set the NULL */
	if (succ != NULL)
		SET_PRED(succ, pred);
}

/* Insert one block to the corresponding list according to its size */
static void insert_block(void *bp, size_t size){
	void *header = GET_HEADER(heap_listp, get_index(size));
	void *succ = SUCC_BLKP(header);

	SET_PRED(bp, header);
	SET_SUCC(bp, succ);
	SET_SUCC(header, bp);

	/* If there is no free block in the list, don't set the NULL */
	if (succ != NULL)
		SET_PRED(succ, bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
   void *oldptr = ptr;

   if(size == 0){
	   mm_free(oldptr);
	   return ptr;
   }

   else if (oldptr == NULL){
	   ptr = mm_malloc(size);
	   return ptr;
   }

   else{
	   size_t bsize = GET_SIZE(HDRP(ptr));
	   size_t asize = ALIGN(size) + DSIZE;

	   if (bsize >= asize){	/* If the size of block is bigger than it asked for */
		   if ((bsize - asize) >= MIN_BLK_SIZE){ /* If the block can be slice and coalesced */
			   PUT(HDRP(ptr), PACK(asize, 1));
			   PUT(FTRP(ptr), PACK(asize, 1));
			   void *next_blk = NEXT_BLKP(ptr);
			   PUT(HDRP(next_blk), PACK(bsize-asize, 0));
			   PUT(FTRP(next_blk), PACK(bsize-asize, 0));
			   coalesce(next_blk);
			}
		   return ptr;
		}
	   else{ /* If the size of block is smaller than it asked for */

		   /* If the block is the last block of the heap, just extend it
			* (useful in the last tracefile) */
		   if (GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0){ 
			   mem_sbrk(asize-bsize);
			   PUT(HDRP(ptr), PACK(asize, 1));
			   PUT(FTRP(ptr), PACK(asize, 1));
			   PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));
			   return ptr;
		   }

		   /* If the previous block and next block is free and the size of all satisfy the need */
		   else if (GET_ALLOC(HDRP(PREV_BLKP(ptr))) == 0 && 
				   GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0 && 
				   GET_SIZE(HDRP(PREV_BLKP(ptr))) + 
				   GET_SIZE(HDRP(NEXT_BLKP(ptr))) + bsize >= asize) {
			   size_t blk_size = GET_SIZE(HDRP(PREV_BLKP(ptr))) + 
				   GET_SIZE(HDRP(NEXT_BLKP(ptr))) + bsize;

			   void *new_ptr = PREV_BLKP(ptr);
			   delete_block(NEXT_BLKP(ptr));
			   delete_block(new_ptr);
			   
			   PUT(HDRP(new_ptr), PACK(blk_size, 1));
			   PUT(FTRP(new_ptr), PACK(blk_size, 1));
			   memcpy(new_ptr, ptr, bsize-DSIZE);

			   /* If it can be sliced and be freed */
			   if (blk_size - asize >= MIN_BLK_SIZE) 
					realloc_coalesce(new_ptr, asize);

			   return new_ptr;
			}

		   /* If the next block is free and the size of both satisfy the need */
			else if (GET_ALLOC(HDRP(PREV_BLKP(ptr))) == 0 
				   && GET_SIZE(HDRP(PREV_BLKP(ptr))) + bsize >= asize) {
			   size_t blk_size = GET_SIZE(HDRP(PREV_BLKP(ptr))) + bsize;
			   void *new_ptr = PREV_BLKP(ptr);
			   delete_block(new_ptr);
			   
				PUT(HDRP(new_ptr), PACK(blk_size, 1));
				PUT(FTRP(new_ptr), PACK(blk_size, 1));
				memcpy(new_ptr, ptr, bsize-DSIZE);

				/* If it can be sliced and be freed */
				if (blk_size - asize >= MIN_BLK_SIZE) 
					realloc_coalesce(new_ptr, asize);
			   
			   return new_ptr;
			}

			/* If the previous block is free and the size of both satisfy the need */
			else if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0 
				   && GET_SIZE(HDRP(NEXT_BLKP(ptr))) + bsize >= asize) {
			   size_t blk_size = GET_SIZE(HDRP(NEXT_BLKP(ptr))) + bsize;
			   void *next_blk = NEXT_BLKP(ptr);
			   delete_block(next_blk);
			   
			   PUT(FTRP(next_blk), PACK(blk_size, 1));
			   PUT(HDRP(ptr), PACK(blk_size, 1));
			   
			   /* If it can be sliced and be freed */
			   if (blk_size - asize >= MIN_BLK_SIZE) 
					realloc_coalesce(ptr, asize);

				return ptr;
			}
		   
			/* Just malloc new block and free the primitive one */
			else {
				void *new_ptr = mm_malloc(asize);
				memcpy(new_ptr, ptr, bsize-DSIZE);
				mm_free(ptr);
				return new_ptr;
			}
		
		}
	}
}

/* Coalesce for realloc */
static void realloc_coalesce(void *bp, size_t alloc_size){
	size_t blk_size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(alloc_size, 1));
	PUT(FTRP(bp), PACK(alloc_size, 1));
	void *next_blk = NEXT_BLKP(bp);
	PUT(HDRP(next_blk), PACK(blk_size - alloc_size, 0));
	PUT(HDRP(next_blk), PACK(blk_size - alloc_size, 0));
	coalesce(next_blk);
}














