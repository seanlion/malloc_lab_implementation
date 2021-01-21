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
#define WSIZE 4 /* Word and header/footer size (bytes) */
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
#define GET_NEXT_PTR(bp) (*(char **)(bp + WSIZE))
#define GET_PREV_PTR(bp) (*(char **)(bp))

/* Puts pointers in the next and previous elements of free list */
#define SET_NEXT_PTR(bp, qp) (GET_NEXT_PTR(bp) = qp)
#define SET_PREV_PTR(bp, qp) (GET_PREV_PTR(bp) = qp)

/* 글로벌 변수 */
static char *heap_listp = 0;
static char *free_list_start = 0;

/* Function prototypes */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* Function prototypes for maintaining free list*/
static void insert_in_free_list(void *bp);
static void remove_from_free_list(void *bp);

/* heap consistency checker routines: 이건 직접 로직을 구현하진 않고 참고 코드 그대로. */
static void checkblock(void *bp);
static void checkheap(bool verbose);
static void printblock(void *bp);

int mm_init(void)
{
	/* Create the initial empty heap. */
	if ((heap_listp = mem_sbrk(8 * WSIZE)) == NULL) // 8 * WSIZE에 큰 이유는 없음.
		return -1;

	PUT(heap_listp, 0);							   /* padding 만듬 */
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

static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	// minimum block을 16사이즈로 정함
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
	}
	insert_in_free_list(bp);
	return bp;
}

void *mm_malloc(size_t size)
{
	size_t asize;	   /* 조정 block size */
	size_t extendsize; /* fit 안 됐으면 늘려야 할 사이즈 */
	void *bp;

	if (size == 0)
		return (NULL);

	/* Adjust block size 하는거 */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

	// explicit은 first fit으로
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
 * [김용욱]: 진짜 생각지도 못한 방법으로 최적화를 이뤄내셨군요.. 와.. 대단하십니다.. 
 * 함수가 초기화 되면 변수가 다시 0으로 돌아간다고 생각했는데, 정적 변수를 사용하셔서
 * 변수가 초기화 되지 않는군요! 정적 변수를 어떻게 사용하는지 배웠습니다! 감사합니다!
 */
static void *find_fit(size_t asize)
{
	void *bp;
	static int last_malloced_size = 0;
	static int repeat_counter = 0;
	// 계속 똑같은 사이즈만 요청했을 때 요청 횟수가 60회가 넘어가면 아예 힙사이즈 확늘린다.(성능을 높이기 위한 방법) 
	if (last_malloced_size == (int)asize)
	{
		if (repeat_counter > 60)
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
	// free block list에서 first fit으로 찾기
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

/* 
 * [김용욱]: 중복되는 코드를 허락하지 않는 모습이 너무 좋습니다! 크으 
 * SET_NEXT_PTR, SET_PREV_PTR 매크로 정의 해두신게 
 * GET_NEXT_PTR, GET_PREV_PTR만 사용해서 적는 것보다 가독성이 훨씬 올라가네요!
 * 좋은 부분을 배운 것 같습니다!
 */

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

void mm_free(void *bp)
{
	size_t size;
	if (bp == NULL)
		return;
	/* Free and coalesce the block. */
	size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}

/* 
 * [김용욱]: realloc 함수를 직접 구현하셨네요! 대단하십니다!
 * 다만 if문이 깊게 들어가서 가독성 측면에서 조금 아쉬운 것 같습니다. 
 * 300번째 줄부터는 if return, if return, return 구조로 조금 더 가독성을 높히실 수 있을 것 같습니다!
 */

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
		size_t newsize = size + (2 * WSIZE); // 2 words for header and footer
		/*if newsize가 oldsize보다 작거나 같으면 그냥 그대로 써도 됨. just return bp */
		if (newsize <= oldsize)
		{
			return bp;
		}
		//oldsize 보다 new size가 크면 바꿔야 함.*/
		else
		{
			size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLK(bp)));
			size_t csize;
			/* next block is free and the size of the two blocks is greater than or equal the new size  */
			/* next block이 가용상태이고 old, next block의 사이즈 합이 new size보다 크면 그냥 그거 바로 합쳐서 쓰기  */
			if (!next_alloc && ((csize = oldsize + GET_SIZE(HDRP(NEXT_BLK(bp))))) >= newsize)
			{
				remove_from_free_list(NEXT_BLK(bp));
				PUT(HDRP(bp), PACK(csize, 1));
				PUT(FTRP(bp), PACK(csize, 1));
				return bp;
			}
			// 아니면 새로 block 만들어서 거기로 옮기기
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



// heap consistency check하기
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


