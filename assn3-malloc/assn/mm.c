/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * The way the our team approached this lab was to implement a explicit list
 * within the heap.  Malloc and free both use this free explicit free list
 * then to find a best fit.  Realloc is modified in a such that, if the 
 * block being realloced is the last block,( the next block is the eiplog),
 * it will extend the current heap size.
 * 
 * The free list is defiend as a global varible, each freed block in inserted
 * at the beginning of the free list. Each free block contains two pointers 
 * that points to the previous and next free block. Find_fit function traverses
 * the free list to find a suitable block.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"


/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Pokemon",
    /* First member's full name */
    "Chen Hao Zhang",
    /* First member's email address */
    "chenhao.zhang@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Vincent Chen",
    /* Second member's email address (leave blank if none) */
    "vincenttt.chen@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Give a free block ptr bp, compute the new and previous free block */
#define NEXT_FREEP(bp) (*(void **)(bp + WSIZE))
#define PREV_FREEP(bp) (*(void  **)bp)

static void* heap_listp = NULL;
static void* free_listp = NULL;   // A pointer for the free list blocks

static int maxBlockSize1 =0;      // The first largest free block on the list
static int maxBlockSize2 =0;      // The second largest free block on the list

/**********************************************************
 * Function Prototypes
 **********************************************************/
void insertFifoFreeBlock(void *bp);
void delete_from_free(void * bp);
int mm_check(void);
int isNextEiplog(void * bp);

/**********************************************************
 * Helper Functions
 **********************************************************/

/**********************************************************
 * Function to verify the header and footer of the 
 * block are both identical, in that the alloc as well
 * as size portions are identical
 * returns 
 **********************************************************/
int verifyBlock(void* bp)
{
    // verify the header and footers are the same
    if(GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp)))
    {
        printf("Size Incorrect\n");
        return -1;
    }
    if (GET_ALLOC(bp) != GET_ALLOC(bp))
    {
        printf("Alloc Incorrect\n");
        return -1;
    }

    return 0;
}

/**********************************************************
 * This function checks the next block to see
 * if it is the epilog block
 **********************************************************/
int isNextEiplog(void * bp)
{
    void * next = NEXT_BLKP(bp);
    if(next == NULL)
        return 0;

    if(GET_SIZE(HDRP(next))==0 && GET_ALLOC(HDRP(next)) == 0x1)
        return 1;
    else
        return 0;
}

/**********************************************************
 * This function sets up the initial block, as well as 
 * sets the heap list to memb_sbrk as well as
 * set the free list to NULL as well as 
 * sets the maxBlockSize1 and maxBlockSize2 to 0
 **********************************************************/
 int mm_init(void)
 {
    
  if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
         return -1;
     PUT(heap_listp, 0);                         // alignment padding
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
     heap_listp += DSIZE; //should case heap_listp as char*
     free_listp = NULL;
     maxBlockSize1 = 0; // set the max block size as 0
     maxBlockSize2 = 0; // set the second max block size as 0
     return 0;
 }


/**********************************************************
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing

 *  The coalesce has four conditions based on
 *  the neighbours around it
 *  basically if there is a free neighbour,
 *  we need to modify the header and footer 
 *  of the left most neightbour to coalesce the new size.
 *  But as well, since we need to remove the coalesced free
 *  block from the free list and re add the new larger block
 *  onto the free list.
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (bp == NULL)
        return NULL;

    if (prev_alloc && next_alloc) {       /* Case 1 */
    insertFifoFreeBlock(bp);              // Insert onto free list
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_from_free (NEXT_BLKP(bp));   // Remove the right block from the free list
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        insertFifoFreeBlock(bp);           // Insert onto free list
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        delete_from_free (PREV_BLKP(bp));      // Remove the left block from the free list
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        bp = PREV_BLKP(bp);

        insertFifoFreeBlock(bp);             // Insert onto free list

        return (bp);
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
        GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;

        delete_from_free (PREV_BLKP(bp));       // Remove the left block from the free list
        delete_from_free (NEXT_BLKP(bp));        // Remove the right block from the free list
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));

        bp = PREV_BLKP(bp);

        insertFifoFreeBlock(bp);                // Insert onto free list
        return (bp);
    }
}


/**********************************************************
 * Insert a block onto the free list as FIFO
 * check for previous block and next block
 * if there is a previous or next block
 * set their pointers appropiately
 **********************************************************/
void insertFifoFreeBlock(void *bp)
{
    int blockSize = GET_SIZE(HDRP(bp));

    // if the block current block sze is larger than the 
    // largest block size then reset the max block size
    if( blockSize> maxBlockSize1)
    {
        maxBlockSize1 = blockSize;
    }

    // if the block current block sze is larger than the 
    // second largest block size then reset the second max block size

    if(blockSize> maxBlockSize2 &&  blockSize< maxBlockSize1)
    {
        maxBlockSize2 = blockSize;
    }

    *((void **)bp) = free_listp;
    *((void **)((char *)bp + WSIZE)) = NULL;
    if (free_listp != NULL) {
        *((void **)((char *)free_listp + WSIZE)) = bp;
    }
    free_listp = bp; 
}

/**********************************************************
 * Delete a free block from the free list
 * check if this block is being pointed at from a previous or next
 * block, if it is, pointer the previous pointer foward
 * and point the next previous pointer previous.
 **********************************************************/
void delete_from_free(void * bp)
{
    int blockSize = GET_SIZE(HDRP(bp));
    if(blockSize == maxBlockSize1)
    {
        maxBlockSize1 = maxBlockSize2;
    }
    void *next_free_block, *pre_free_block;
    
    next_free_block = *((void **)bp); 
    pre_free_block = *((void **)((char *)bp + WSIZE));
 
    // case 1: this block is a middle block
    if (next_free_block != NULL && pre_free_block != NULL) {
        *((void **)pre_free_block) = next_free_block;
        *((void **)((char *)next_free_block + WSIZE)) = pre_free_block;
    }
    // case 2: this block is the first block
    if (next_free_block == NULL && pre_free_block != NULL){
        *((void **)pre_free_block) = next_free_block;
    }

    // case 3: this block is the last block
    if (next_free_block != NULL && pre_free_block == NULL) {
        
        *((void **)((char *)next_free_block + WSIZE)) = NULL;
        free_listp = next_free_block;
    }
    // case 4: this block is the only block left of the free list
    if ((next_free_block == NULL) && (pre_free_block == NULL)) {
        free_listp = NULL;
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * split_block
 * Split the block into two smaller pieces and returns the 
 *   lower piece
 * the pointers stored in the upper piece are untouched
 **********************************************************/

void *split_block (void *bp, size_t asize) {
    if (bp == NULL) {
        return NULL;
    }
    void *end_of_block, *new_bp;
    size_t old_size = GET_SIZE(HDRP(bp));
    end_of_block = (char *)bp + old_size - WSIZE;
    new_bp = (char *)end_of_block - asize + WSIZE;
    PUT(HDRP(new_bp), PACK(asize, 0));
    PUT(FTRP(new_bp), PACK(asize, 0));

    PUT(HDRP(bp), PACK(old_size-asize, 0));
    PUT(FTRP(bp), PACK(old_size-asize, 0));

    return new_bp;
}



/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 * Uses fit bit option
 * If we find a block that is close to the request size (4*sizethreshold)
 *   we will use that block
 * If we can't find a block close to request size then we return the closest
 *    size and split the block before using it
 **********************************************************/
void * find_fit(size_t asize, int *list_broken)
{
    if(asize>maxBlockSize1)
        return NULL;
    size_t bestfit = mem_heapsize(); 
    size_t curfit;

    void *bestfitptr = NULL;
    void *free_bp;
    int sizethreshold = 2*DSIZE;
    for (free_bp = free_listp; free_bp != NULL; free_bp = *((void **)free_bp))
    {
        if (asize <= GET_SIZE(HDRP(free_bp)))
        {
            curfit = GET_SIZE(HDRP(free_bp)) - asize;
            if (curfit < 4*sizethreshold) {
                bestfitptr = free_bp;
                break;
            }
            if (curfit < bestfit) {
                bestfit = curfit;
                bestfitptr = free_bp;
            }
        }
    }
    if ((bestfitptr != NULL) && (GET_SIZE(HDRP(bestfitptr)) - asize >= sizethreshold)) {
        bestfitptr = split_block (bestfitptr, asize);
        *list_broken = 1;
    }
    return bestfitptr;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize, int call_delete)
{
  /* Get the current block size */
    size_t bsize = GET_SIZE(HDRP(bp));
  if (call_delete == 1) {
     delete_from_free(bp);
                             // since we are returning
                             // free block to be used
                             // remove the block from the free
                             // list
  }      
    PUT(HDRP(bp), PACK(bsize, 1));
    PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
     if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;
    int list_broken = 0;
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize, &list_broken)) != NULL) {
        place(bp, asize, !list_broken);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize, 1);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 * The modified realloc works in this regard
 * if the new size of the block is smaller than the current block
 * size,
 *********************************************************/

void *mm_realloc(void *ptr, size_t size)
{
if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
    {
        return (mm_malloc(size));
    }
      
    // get the proper alligned size
    size_t asize,currentSize;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);


    currentSize = GET_SIZE(HDRP(ptr));


    if (asize < currentSize)
    {
        size_t splitBlockSize = currentSize-asize;
        if(splitBlockSize>= DSIZE )
        {
            // If after the realloc, the two blocks
            // are both bigger than the minium size for 
            // a memory block, split the blocks, 
            // set the header and footer for the realloc block
            // and the header and footer for the smaller split block
            // this persrves the heap consistancy.
            // Finally free the smaller block so that it can be used.
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));

            PUT(HDRP(NEXT_BLKP(ptr)), PACK(currentSize-asize, 1));
            PUT(FTRP(NEXT_BLKP(ptr)), PACK(currentSize-asize, 1));

            mm_free(NEXT_BLKP(ptr));
            return ptr;
        }
        else
        {
           // or else do not touch the size of the malloc block and 
            // just return the origonal size.
            return ptr;
        }
       
    }
    else if(asize >currentSize)
    {

        if(isNextEiplog(ptr) == 1)
        {

            // If the current block is the last block on the heap
            // just increase the memory size of mem_sbrk and append 
            // the current block.  This avoids a copy and memcpy
            // as it is un-necessary.

            size_t payLoadSize = (asize- currentSize) + WSIZE;

            // extend the heap
            mem_sbrk(payLoadSize);
            size_t newSize = currentSize + payLoadSize;
            PUT(HDRP(ptr), PACK(newSize-WSIZE, 1));       // Reset the new header size
            PUT(FTRP(ptr), PACK(newSize-WSIZE, 1));       // Reset the new footer size
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));        // new epilogue header
            //return the origonal pointer
            return ptr;
        }
        else
        {
            void* newptr = mm_malloc(size);
            memcpy(newptr, ptr, currentSize);
            mm_free(ptr);
            return newptr;
        }
    }
    else
    {
        return ptr;
    }
      
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 * Checks:
 * -If the alloc bit and size are same in both header and footer
 * -If the total free and allocated blocks add up to the heap size
 * -If adjacent free blocks might have skipped coalescing
 * -If all the free blocks are in the free list
 * -If a free block is marked correctly
 * -Check if block is valid heap address
 *********************************************************/
int mm_check(void){
    if (heap_listp == NULL) { //heap has not been initialized
        return 1;
    }
    void *traverse = NEXT_BLKP(heap_listp); //traverse heap
    void *traverse_free; //traverse free list
    int heap_size_counter, heap_size;
    heap_size_counter = 2*WSIZE;

    if (GET_SIZE(HDRP(traverse)) == 0) { //no memory has been allocated so far
        return 1;
    }
    
    while (GET_SIZE(HDRP(traverse)) != 0) {
        if (GET_SIZE(HDRP(traverse)) != GET_SIZE(FTRP(traverse))) {
            //check if the size indicated by header is the same as footer
            printf ("Error: Size indicated by header does not equal to size indicated by footer\n");
            printf("%p\n", traverse);
            return 0;
        }
        if (GET_ALLOC(HDRP(traverse)) != GET_ALLOC(FTRP(traverse))) {
            //check if the alloc bit indicated by header is the same as footer
            printf ("Error: Alloc bit indicated by the header does not equal to alloc bit indicated by footer\n");
            printf("%p\n", traverse);
            return 0;
        }

        if (GET_ALLOC(HDRP(traverse)) == 0 && GET_ALLOC(HDRP(NEXT_BLKP(traverse))) == 0) {
            // check if there exists two adjacent free blocks not coalesced
            printf("Error: two adjacent free blocks not coalesced\n");
            printf("%p\n", traverse);
            return 0;
        }

        if (( ((void *)NEXT_BLKP(traverse)-WSIZE) > mem_heap_hi()) || ((traverse + WSIZE) < mem_heap_lo())) {
            printf("Error: block is not valid heap memory\n");
            printf("%p\n", traverse);
            return 0;
        }

        if (GET_ALLOC(HDRP(traverse)) == 0) {
            // check if this free block is in the free list
            int found_free = 0;
            traverse_free = free_listp;
            while (traverse_free != NULL) {
                if (traverse_free == traverse) {
                    found_free = 1;
                    break;
                }
                traverse_free = NEXT_FREEP(traverse_free);
            } 
            if (found_free == 0) {
                printf("Error: free block not in free list\n");
                return 0;
            }          
        }


        heap_size_counter += GET_SIZE(HDRP(traverse));
        traverse = NEXT_BLKP(traverse);
    }
    heap_size = mem_sbrk(0) - heap_listp;
    if (heap_size_counter != heap_size) {
        printf ("Error: heap size counter does not match total heap size\n");
        return 0;
    }


    //Check if every free block is marked as free
    traverse = free_listp;
    while (traverse != NULL){
        if (GET_ALLOC(HDRP(traverse)) != 0 || GET_ALLOC(FTRP(traverse)) != 0) {
            printf ("Error: free blocks not marked as free\n");
            printf("%p\n", traverse);
            return 0;
        }
        traverse = NEXT_FREEP(traverse);
    }

    return 1;
}
