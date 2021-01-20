/* Blocks are aligned to double-word boundaries.  This
 * yields 8-byte aligned blocks on a 32-bit processor, and 16-byte aligned
 * blocks on a 64-bit processor.  However, 16-byte alignment is stricter
 * than necessary; the assignment only requires 8-byte alignment.  The
 * minimum block size taken is 4 words.
 * This allocator uses the size of a pointer, e.g., sizeof(void *), to
 * define the size of a word.  This allocator also uses the standard
 * type uintptr_t to define unsigned integers that are the same size
 * as a pointer, i.e., sizeof(uintptr_t) == sizeof(void *).
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
 
#include "memlib.h"
#include "mm.h"
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

team_t team = {
	"Malloc lab",
	"Seung    ",
	"20210120",
	"Huh ",
	"20210120"
	};

//* Basic constants and macros: */
#define WSIZE sizeof(void *) /* Word and header/footer size (bytes) */
#define DSIZE (2 * WSIZE)	 /* Doubleword size (bytes) */
#define CHUNKSIZE (1 << 12)	 /* Extend heap by this amount (bytes) */

/*Max value of 2 values*/
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p) (*(uintptr_t *)(p))
#define PUT(p, val) (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((void *)(bp) - WSIZE)
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLK(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLK(bp) ((void *)(bp) - GET_SIZE((void *)(bp)-DSIZE))

/* Given ptr in free list, get next and previous ptr in the list */
/* bp is address of the free block. Since minimum Block size is 16 bytes, 
   we utilize to store the address of previous block pointer and next block pointer.
*/
#define GET_NEXT_PTR(bp) (*(char **)(bp + WSIZE))
#define GET_PREV_PTR(bp) (*(char **)(bp))

/* Puts pointers in the next and previous elements of free list */
#define SET_NEXT_PTR(bp, qp) (GET_NEXT_PTR(bp) = qp)
#define SET_PREV_PTR(bp, qp) (GET_PREV_PTR(bp) = qp)

/* Global declarations */
static char *heap_listp = 0;
static char *free_list_start = 0;

/* Function prototypes for internal helper routines */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* Function prototypes for maintaining free list*/
static void insert_in_free_list(void *bp);
static void remove_from_free_list(void *bp);

/* Function prototypes for heap consistency checker routines: */
static void checkblock(void *bp);
static void checkheap(bool verbose);
static void printblock(void *bp);

/**
 * Initialize the memory manager.
 * @param - void no parameter passed in
 * @return - int 0 for success or -1 for failure
 */
int mm_init(void)
{

	/* Create the initial empty heap. */
	if ((heap_listp = mem_sbrk(8 * WSIZE)) == NULL) //왜 8 WSIZE???? 그냥 충분하게??
		return -1;

	PUT(heap_listp, 0);							   /* Alignment padding */
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));	   /* Epilogue header */
	free_list_start = heap_listp + 2 * WSIZE;
	/* Extend the empty heap with a free block of minimum possible block size */
	if (extend_heap(4) == NULL)
	{
		return -1;
	}
	return 0;
}

/* 
 * Requires:
 *   size of memory asked by the programmer.
 *
 * Effects:
 *   Allocate a block with at least "size" bytes of payload, unless "size" is
 *   zero.  Returns the address of this block if the allocation was successful
 *   and NULL otherwise.
 */
void *mm_malloc(size_t size)
{
	size_t asize;	   /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	void *bp;

	/* Ignore spurious requests. */
	if (size == 0)
		return (NULL);

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

	/* Search the free list for a fit. */
	if ((bp = find_fit(asize)) != NULL)
	{
		place(bp, asize);
		return (bp);
	}

	/* No fit found.  Get more memory and place the block. */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return (NULL);
	place(bp, asize);
	return (bp);
}

/* 
 * Requires:
 *   "bp" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Free a block.
 */
void mm_free(void *bp)
{
	size_t size;
	/* Ignore spurious requests. */
	if (bp == NULL)
		return;
	/* Free and coalesce the block. */
	size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}


void *mm_realloc(void *bp, size_t size)
{
	if ((int)size < 0)
		return NULL;
	else if ((int)size == 0)
	{
		mm_free(bp);
		return NULL;
	}
	else if (size > 0)
	{
		size_t oldsize = GET_SIZE(HDRP(bp));
		size_t newsize = size + 2 * WSIZE; // 2 words for header and footer
		/*if newsize is less than oldsize then we just return bp */
		if (newsize <= oldsize)
		{
			return bp;
		}
		/*if newsize is greater than oldsize */
		else
		{
			size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLK(bp)));
			size_t csize;
			/* next block is free and the size of the two blocks is greater than or equal the new size  */
			/* then we only need to combine both the blocks  */
			if (!next_alloc && ((csize = oldsize + GET_SIZE(HDRP(NEXT_BLK(bp))))) >= newsize)
			{
				remove_from_free_list(NEXT_BLK(bp));
				PUT(HDRP(bp), PACK(csize, 1));
				PUT(FTRP(bp), PACK(csize, 1));
				return bp;
			}
			else
			{
				void *new_ptr = mm_malloc(newsize);
				place(new_ptr, newsize);
				memcpy(new_ptr, bp, newsize);
				mm_free(bp);
				return new_ptr;
			}
		}
	}
	else
		return NULL;
}

static void *coalesce(void *bp) 
{

	//if previous block is allocated or its size is zero then PREV_ALLOC will be set.
	size_t NEXT_ALLOC = GET_ALLOC(HDRP(NEXT_BLK(bp)));
	size_t PREV_ALLOC = GET_ALLOC(FTRP(PREV_BLK(bp))) || PREV_BLK(bp) == bp;  
	//PREV_BLK(bp) == bp: epilogue block을 만났을 떄. Extend했을 때 epilogue를 만나는 유일한 경우
	size_t size = GET_SIZE(HDRP(bp));

	/* Next block is only free */
	if (PREV_ALLOC && !NEXT_ALLOC)
	{
		size += GET_SIZE(HDRP(NEXT_BLK(bp)));
		remove_from_free_list(NEXT_BLK(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	/* Prev block is only free */
	else if (!PREV_ALLOC && NEXT_ALLOC)
	{
		size += GET_SIZE(HDRP(PREV_BLK(bp)));
		bp = PREV_BLK(bp);
		remove_from_free_list(bp);
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	/* Both blocks are free */
	else if (!PREV_ALLOC && !NEXT_ALLOC)
	{
		size += GET_SIZE(HDRP(PREV_BLK(bp))) + GET_SIZE(HDRP(NEXT_BLK(bp)));
		remove_from_free_list(PREV_BLK(bp));
		remove_from_free_list(NEXT_BLK(bp));
		bp = PREV_BLK(bp);
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	} /* lastly insert bp into free list and return bp */
	insert_in_free_list(bp);
	return bp;
}

static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	//Since minimum block size given to us is 4 words (ie 16 bytes)
	if (size < 16)
	{
		size = 16;
	}
	/* call for more memory space */
	if ((int)(bp = mem_sbrk(size)) == -1)
	{
		return NULL;
	}
	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));		 /* free block header */
	PUT(FTRP(bp), PACK(size, 0));		 /* free block footer */
	PUT(HDRP(NEXT_BLK(bp)), PACK(0, 1)); /* new epilogue header */
	/* coalesce bp with next and previous blocks */
	return coalesce(bp);
}


static void *find_fit(size_t asize)
{
	void *bp;
	static int last_malloced_size = 0;
	static int repeat_counter = 0;
	if (last_malloced_size == (int)asize)
	{
		if (repeat_counter > 60) // 성능을 위해
		{
			int extendsize = MAX(asize, 4 * WSIZE);
			bp = extend_heap(extendsize / 4);
			return bp;
		}
		else
			repeat_counter++;
	}
	else
		repeat_counter = 0;

	for (bp = free_list_start; GET_ALLOC(HDRP(bp)) == 0; bp = GET_NEXT_PTR(bp))
	{
		if (asize <= (size_t)GET_SIZE(HDRP(bp)))
		{
			last_malloced_size = asize;
			return bp;
		}
	}
	return NULL;
}

static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));

	if ((csize - asize) >= 4 * WSIZE)
	{
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		remove_from_free_list(bp);
		bp = NEXT_BLK(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
		coalesce(bp);
	}
	else
	{
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		remove_from_free_list(bp);
	}
}

static void insert_in_free_list(void *bp)
{
	SET_NEXT_PTR(bp, free_list_start);
	SET_PREV_PTR(free_list_start, bp);
	SET_PREV_PTR(bp, NULL);
	free_list_start = bp;
}
/*Removes the free block pointer int the free_list*/

static void remove_from_free_list(void *bp)
{

	//내 앞에 누구 있으면
	if (GET_PREV_PTR(bp))
		SET_NEXT_PTR(GET_PREV_PTR(bp), GET_NEXT_PTR(bp)); //내 앞 노드의 주소에다가, 내 뒤 노드의 주소를 넣어준다.
	else												  // 내 앞에 아무도 없으면 == 내가 젤 앞 노드이면
		free_list_start = GET_NEXT_PTR(bp);				  //나를 없애면서, 내 뒷 노드에다가 가장 앞자리의 왕관을 물려주고 간다!
	SET_PREV_PTR(GET_NEXT_PTR(bp), GET_PREV_PTR(bp));
}

/* 
 * The remaining routines are heap consistency checker routines. 
 */

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Perform a minimal check on the block "bp".
 */
static void checkblock(void *bp)
{

	if ((uintptr_t)bp % DSIZE)
		printf("Error: %p is not doubleword aligned\n", bp);
	if (GET(HDRP(bp)) != GET(FTRP(bp)))
		printf("Error: header does not match footer\n");
}

void checkheap(bool verbose)
{
	void *bp;

	if (verbose)
		printf("Heap (%p):\n", heap_listp);

	if (GET_SIZE(HDRP(heap_listp)) != DSIZE ||
		!GET_ALLOC(HDRP(heap_listp)))
		printf("Bad prologue header\n");
	checkblock(heap_listp);

	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = (void *)NEXT_BLKP(bp))
	{
		if (verbose)
			printblock(bp);
		checkblock(bp);
	}

	if (verbose)
		printblock(bp);
	if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
		printf("Bad epilogue header\n");
}


static void printblock(void *bp)
{
	bool halloc, falloc;
	size_t hsize, fsize;

	checkheap(false);
	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));

	if (hsize == 0)
	{
		printf("%p: end of heap\n", bp);
		return;
	}

	printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp,
		   hsize, (halloc ? 'a' : 'f'),
		   fsize, (falloc ? 'a' : 'f'));
}


