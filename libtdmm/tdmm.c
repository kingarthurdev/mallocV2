#include "tdmm.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

header *headOfFree;
header *headOfOccupied;
alloc_strat_e currentMode;
int currentAmountAllocated;
int alignmentSize;

// TODO: do some integrity checks with the 0xDEADBEEF value to actually use it.
void t_init(alloc_strat_e strat)
{
	alignmentSize = sysconf(_SC_PAGESIZE); // because mmap already has its own internal alignment -- so if we don't do this, we're just wasting memory
	currentAmountAllocated = 32768 + sizeof(header);
	// set how we're gonna do this
	currentMode = strat;

	// arbitrarily deciding to allocate 32kib of data
	// should be aligned
	headOfFree = (header *)mmap(NULL, currentAmountAllocated, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	printf("%p\n", headOfFree);
	if (headOfFree == MAP_FAILED)
	{
		throwError("ERROR! FAILED TO ALLOCATE MORE MEMORY!\n");
	}
	headOfFree->isFree = 1;
	headOfFree->size = 32768 + sizeof(header);
	headOfFree->prevBlock = NULL;
	headOfFree->nextBlock = NULL;
	headOfFree->protectionBlock = 0xDEADBEEF; // a fun thing that's pretty common in network stuff too -- validates that header hasn't been corrupted
}

void *t_malloc(size_t size)
{
	// We need to implement 3 different methods -- best fit, worst fit, and first fit
	if (currentMode == FIRST_FIT)
	{
		return doFirstFit(size);
	}
	else if (currentMode == BEST_FIT)
	{
		return doBestFit(size);
	}
	else if (currentMode == WORST_FIT)
	{
		return doWorstFit(size);
	}
	return NULL;
}

void *doFirstFit(size_t size)
{
	if (headOfFree == NULL)
	{
		allocateMoreMemory(32768 + sizeof(header));
	}
	header *currentNode = headOfFree;
	size_t sizePostHeader = size + sizeof(header);
	sizePostHeader = alignSize(sizePostHeader);
	while (1)
	{
		if (currentNode->size >= sizePostHeader)
		{
			// large enough for us to do stuff!
			size_t sizeOfSegmentAfterAlloc = currentNode->size - sizePostHeader;

			// if we split this, the next chunk is literally too small for us to use, so let's just give the whole block of memory to this block so we don't lose it
			if (sizeOfSegmentAfterAlloc < alignSize(sizeof(header) + 1))
			{
				currentNode->isFree = 0;
				// cut out from the free list
				if (currentNode == headOfFree)
				{
					// TODO: figure out what happens if we're out of stuff -- prob need to do update to the get more memory function
					headOfFree = currentNode->nextBlock; // TODO: what happens if this is null?
				}
				else
				{
					currentNode->prevBlock->nextBlock = currentNode->nextBlock;
				}
				if (currentNode->nextBlock != NULL)
				{
					currentNode->nextBlock->prevBlock = currentNode->prevBlock;
				}
				orderNewlyAllocatedNode(currentNode);
				break;
			}
			else
			{
				// standard case -- split up the current block
				splitCurrentBlock(currentNode, sizePostHeader, sizeOfSegmentAfterAlloc);
				break;
			}
		}
		else
		{
			if (currentNode->nextBlock == NULL)
			{
				// did not find a suitable block, need to allocate more memory
				allocateMoreMemory(sizePostHeader);
				currentNode = headOfFree;
			}
			else
			{
				currentNode = currentNode->nextBlock;
			}
		}
	}
	return (void *)(currentNode + 1);
}

void t_free(void *ptr)
{
	header *currentNode = headOfOccupied;
	if (currentNode == NULL)
	{
		throwError("ERROR! No allocations present, so nothing to free");
	}
	/*
	while (1)
	{
		if ((currentNode + 1) == ptr)
		{
			// we found it, now do the freeing stuff
			if (currentNode == headOfOccupied)
			{
				if (currentNode->nextBlock == NULL)
				{
					headOfOccupied = NULL;
				}
				else
				{
					headOfOccupied = currentNode->nextBlock;
					headOfOccupied->prevBlock = NULL;
				}
				currentNode->isFree = 1;
				orderNewFreeData(currentNode);
			}
			else
			{
				// currentNode presumably has a previous and next block, so cut it free
				if (currentNode->prevBlock != NULL)
					currentNode->prevBlock->nextBlock = currentNode->nextBlock;
				if (currentNode->nextBlock != NULL)
					currentNode->nextBlock->prevBlock = currentNode->prevBlock;
				currentNode->prevBlock = NULL;
				currentNode->nextBlock = NULL;
				currentNode->isFree = 1;
				orderNewFreeData(currentNode);
			}
			break;
		}
		else if (currentNode->nextBlock == NULL)
		{
			throwError("ERROR! Cannot find address you're trying to free");
		}
		else
		{
			currentNode = currentNode->nextBlock;
		}
	}*/

	// jankify way hehe:
	if (((header *)(ptr)-1)->protectionBlock == 0xDEADBEEF)
	{
		// we'll pretend this is good enough to validate that we found a legit pointer...
		//  we found it, now do the freeing stuff
		if (currentNode == headOfOccupied)
		{
			if (currentNode->nextBlock == NULL)
			{
				headOfOccupied = NULL;
			}
			else
			{
				headOfOccupied = currentNode->nextBlock;
				headOfOccupied->prevBlock = NULL;
			}
			currentNode->isFree = 1;
			orderNewFreeData(currentNode);
		}
		else
		{
			// currentNode presumably has a previous and next block, so cut it free
			if (currentNode->prevBlock != NULL)
				currentNode->prevBlock->nextBlock = currentNode->nextBlock;
			if (currentNode->nextBlock != NULL)
				currentNode->nextBlock->prevBlock = currentNode->prevBlock;
			currentNode->prevBlock = NULL;
			currentNode->nextBlock = NULL;
			currentNode->isFree = 1;
			orderNewFreeData(currentNode);
		}
	}
	else
	{
		throwError("ERROR! Cannot find address you're trying to free");
	}

	coalesceFreeSectionsV2(currentNode);
}

void allocateMoreMemory(size_t amountOfMemNeeded)
{
	size_t additionalMem = alignSize(amountOfMemNeeded);
	currentAmountAllocated += additionalMem;
	header *newSegment = (header *)mmap(NULL, additionalMem, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (newSegment == MAP_FAILED)
	{
		throwError("ERROR! FAILED TO ALLOCATE MORE MEMORY!\n");
	}
	newSegment->isFree = 1;
	newSegment->protectionBlock = 0xDEADBEEF;
	newSegment->size = additionalMem;
	orderNewFreeData(newSegment);
}

void orderNewFreeData(header *address)
{
	// temp just to try something:
	if (headOfFree == NULL)
	{
		headOfFree = address;
		headOfFree->prevBlock = NULL;
		headOfFree->nextBlock = NULL;
		return;
	}
	else
	{
		// put address before the headOfFree in the linked list
		address->nextBlock = headOfFree;
		address->prevBlock = NULL;
		headOfFree->prevBlock = address;
		headOfFree = address;
	}

	/*
	if (headOfFree == NULL)
	{
		headOfFree = address;
		headOfFree->prevBlock = NULL;
		headOfFree->nextBlock = NULL;
		return;
	}
	else if (address < headOfFree)
	{
		// put address before the headOfFree in the linked list
		address->nextBlock = headOfFree;
		address->prevBlock = NULL;
		headOfFree->prevBlock = address;
		headOfFree = address;
	}
	else
	{
		header *currentNode = headOfFree;
		while (1)
		{
			if (currentNode->nextBlock == NULL)
			{
				currentNode->nextBlock = address;
				address->prevBlock = currentNode;
				address->nextBlock = NULL;
				break;
			}
			else if (address > currentNode && address < currentNode->nextBlock)
			{
				address->prevBlock = currentNode;
				address->nextBlock = currentNode->nextBlock;
				currentNode->nextBlock->prevBlock = address;
				currentNode->nextBlock = address;
				break;
			}
			else
			{
				currentNode = currentNode->nextBlock;
			}
		}
	}*/
}

void throwError(char *message)
{
	fprintf(stderr, "%s\n ", message);
	exit(1);
}

void orderNewlyAllocatedNode(header *targetNode)
{
	header *currentNode = headOfOccupied;
	if (currentNode == NULL)
	{
		headOfOccupied = targetNode;
		targetNode->nextBlock = NULL;
		targetNode->prevBlock = NULL;
		return;
	}
	else
	{
		targetNode->nextBlock = headOfOccupied;
		targetNode->prevBlock = NULL;
		headOfOccupied->prevBlock = targetNode;
		headOfOccupied = targetNode;
	}

	// what if I just don't...
	/*while (1)
	{
		// Case: we've reached the end of the allocated list
		if (currentNode->nextBlock == NULL)
		{
			currentNode->nextBlock = targetNode;
			targetNode->prevBlock = currentNode;
			targetNode->nextBlock = NULL;
			break;
		}
		else if (targetNode > currentNode && targetNode < currentNode->nextBlock)
		{
			// Case: we found a nice place to slot the new node into
			targetNode->nextBlock = currentNode->nextBlock;
			currentNode->nextBlock->prevBlock = targetNode;
			currentNode->nextBlock = targetNode;
			targetNode->prevBlock = currentNode;
			break;
		}
		else
		{
			// Case: we're not at the right place and there's still places to go
			currentNode = currentNode->nextBlock;
		}
	}*/
}

size_t alignSize(size_t sizeOfDataPlusHeader)
{
	return ((sizeOfDataPlusHeader + alignmentSize - 1) / alignmentSize) * alignmentSize;
}
void *doBestFit(size_t size)
{
	if (headOfFree == NULL)
	{
		allocateMoreMemory(32768 + sizeof(header));
	}
	header *currentNode = headOfFree;
	size_t sizePostHeader = size + sizeof(header);
	sizePostHeader = alignSize(sizePostHeader);
	header *smallestSectionSoFar = NULL;

	while (1)
	{
		if (currentNode->size >= sizePostHeader)
		{
			if (smallestSectionSoFar == NULL || currentNode->size < smallestSectionSoFar->size)
				smallestSectionSoFar = currentNode;
		}
		if (currentNode->nextBlock == NULL)
		{
			if (smallestSectionSoFar == NULL)
			{
				allocateMoreMemory(sizePostHeader);
				currentNode = headOfFree;
			}
			else
			{
				break;
			}
		}
		else
		{
			currentNode = currentNode->nextBlock;
		}
	}
	currentNode = smallestSectionSoFar;
	size_t sizeOfSegmentAfterAlloc = currentNode->size - sizePostHeader;

	// if we split this, the next chunk is literally too small for us to use, so let's just give the whole block of memory to this block so we don't lose it
	if (sizeOfSegmentAfterAlloc < alignSize(sizeof(header) + 1))
	{
		currentNode->isFree = 0;
		// cut out from the free list
		if (currentNode == headOfFree)
		{
			// TODO: figure out what happens if we're out of stuff -- prob need to do update to the get more memory function
			headOfFree = currentNode->nextBlock; // TODO: what happens if this is null?
		}
		else
		{
			currentNode->prevBlock->nextBlock = currentNode->nextBlock;
		}
		if (currentNode->nextBlock != NULL)
		{
			currentNode->nextBlock->prevBlock = currentNode->prevBlock;
		}
		orderNewlyAllocatedNode(currentNode);
	}
	else
	{
		// standard case -- split up the current block
		splitCurrentBlock(currentNode, sizePostHeader, sizeOfSegmentAfterAlloc);
	}
	return (void *)(currentNode + 1);
}
void *doWorstFit(size_t size)
{
	if (headOfFree == NULL)
	{
		allocateMoreMemory(32768 + sizeof(header));
	}
	header *currentNode = headOfFree;
	size_t sizePostHeader = size + sizeof(header);
	sizePostHeader = alignSize(sizePostHeader);
	header *largestSectionSoFar = NULL;

	while (1)
	{
		if (currentNode->size >= sizePostHeader)
		{
			if (largestSectionSoFar == NULL || currentNode->size > largestSectionSoFar->size)
				largestSectionSoFar = currentNode;
		}
		if (currentNode->nextBlock == NULL)
		{
			if (largestSectionSoFar == NULL)
			{
				allocateMoreMemory(sizePostHeader);
				currentNode = headOfFree;
			}
			else
			{
				break;
			}
		}
		else
		{
			currentNode = currentNode->nextBlock;
		}
	}
	currentNode = largestSectionSoFar;
	size_t sizeOfSegmentAfterAlloc = currentNode->size - sizePostHeader;

	// if we split this, the next chunk is literally too small for us to use, so let's just give the whole block of memory to this block so we don't lose it
	if (sizeOfSegmentAfterAlloc < alignSize(sizeof(header) + 1))
	{
		currentNode->isFree = 0;
		// cut out from the free list
		if (currentNode == headOfFree)
		{
			// TODO: figure out what happens if we're out of stuff -- prob need to do update to the get more memory function
			headOfFree = currentNode->nextBlock; // TODO: what happens if this is null?
		}
		else
		{
			currentNode->prevBlock->nextBlock = currentNode->nextBlock;
		}
		if (currentNode->nextBlock != NULL)
		{
			currentNode->nextBlock->prevBlock = currentNode->prevBlock;
		}
		orderNewlyAllocatedNode(currentNode);
	}
	else
	{
		// standard case -- split up the current block
		splitCurrentBlock(currentNode, sizePostHeader, sizeOfSegmentAfterAlloc);
	}
	return (void *)(currentNode + 1);
}

int getSysReqMem()
{
	return currentAmountAllocated;
}

void coalesceFreeSections()
{
	header *currentNode = headOfFree;
	if (currentNode == NULL)
	{
		return;
	}
	while (currentNode != NULL && currentNode->nextBlock != NULL)
	{
		if ((char *)currentNode + currentNode->size == currentNode->nextBlock)
		{
			currentNode->size = currentNode->size + currentNode->nextBlock->size;
			currentNode->nextBlock = currentNode->nextBlock->nextBlock;
			if (currentNode->nextBlock != NULL)
				currentNode->nextBlock->prevBlock = currentNode;
		}
		else
		{
			currentNode = currentNode->nextBlock;
		}
	}
}

void coalesceFreeSectionsV2(header *currentNode)
{
	if (currentNode->nextBlock != NULL)
	{
		if ((char *)(currentNode) + currentNode->size == currentNode->nextBlock)
		{
			currentNode->size = currentNode->size + currentNode->nextBlock->size;
			currentNode->nextBlock = currentNode->nextBlock->nextBlock;
			if (currentNode->nextBlock != NULL)
				currentNode->nextBlock->prevBlock = currentNode;
		}
	}
	if (currentNode->prevBlock != NULL)
	{
		if ((char *)(currentNode)-currentNode->prevBlock->size == currentNode->prevBlock)
		{
			currentNode = currentNode->prevBlock; // go back one so I can be lazy and just recycle the same logic as above
			currentNode->size = currentNode->size + currentNode->nextBlock->size;
			currentNode->nextBlock = currentNode->nextBlock->nextBlock;
			if (currentNode->nextBlock != NULL)
				currentNode->nextBlock->prevBlock = currentNode;
		}
	}
}

void splitCurrentBlock(header *currentNode, size_t sizePostHeader, size_t sizeOfSegmentAfterAlloc)
{
	int needUpdateFirstNode = 0;

	if (currentNode == headOfFree)
	{
		needUpdateFirstNode = 1;
	}
	// initialize the new next chunk first
	header *leftOverFreeChunk = (header *)((char *)currentNode + sizePostHeader);
	leftOverFreeChunk->isFree = 1;
	leftOverFreeChunk->nextBlock = currentNode->nextBlock;
	leftOverFreeChunk->size = sizeOfSegmentAfterAlloc;
	leftOverFreeChunk->prevBlock = currentNode->prevBlock;
	leftOverFreeChunk->protectionBlock = 0xDEADBEEF;
	if (leftOverFreeChunk->prevBlock)
		leftOverFreeChunk->prevBlock->nextBlock = leftOverFreeChunk;
	if (leftOverFreeChunk->nextBlock)
		leftOverFreeChunk->nextBlock->prevBlock = leftOverFreeChunk;

	currentNode->size = sizePostHeader;
	currentNode->isFree = 0;
	orderNewlyAllocatedNode(currentNode);

	if (needUpdateFirstNode)
	{
		headOfFree = leftOverFreeChunk;
	}
}