#ifndef TDMM_H
#define TDMM_H

#include <stddef.h>

typedef struct header
{
	size_t size;
	int isFree; // 0 for no, 1 for yes
	struct header *nextBlock;
	struct header *prevBlock;
	int protectionBlock;
} header;

typedef struct footer
{
	size_t size;
	int isFree;
	int protectionBlock;
} footer;

void throwError(char *message);
void *doFirstFit(size_t size);
void *doBestFit(size_t size);
void *doWorstFit(size_t size);
header *allocateMoreMemory(size_t amountOfMemNeeded);
size_t alignSize(size_t sizeOfDataPlusHeader);
void orderNewlyAllocatedNode(header *targetNode);
void orderNewFreeData(header *address);
size_t getSysReqMem();
void coalesceFreeSections();
void splitCurrentBlock(header *currentNode, size_t sizePostHeader, size_t sizeOfSegmentAfterAlloc);
void coalesceFreeSectionsV2(header *currentNode);
double memoryUtilizationPercentage();
size_t getOverheadBytes();

typedef enum
{
	FIRST_FIT,
	BEST_FIT,
	WORST_FIT,
} alloc_strat_e;

/**
 * Initializes the memory allocator with the given strategy.
 *
 * @param strat The strategy to use for memory allocation.
 */
void t_init(alloc_strat_e strat);

/**
 * Allocates a block of memory of the given size.
 *
 * @param size The size of the memory block to allocate.
 * @return A pointer to the allocated memory block fails.
 */
void *t_malloc(size_t size);

/**
 * Frees the given memory block.
 *
 * @param ptr The pointer to the memory block to free. This must be a pointer returned by t_malloc.
 */
void t_free(void *ptr);

#endif // TDMM_H
