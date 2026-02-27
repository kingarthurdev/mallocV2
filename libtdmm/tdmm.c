#include "tdmm.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

header *headOfFree;
header *headOfOccupied;
alloc_strat_e currentMode;
unsigned long long int currentAmountAllocated;
unsigned long long int currentBytesRequested;
int alignmentSize;
int pageSize;
int numRegions = 0;

// TODO: do some integrity checks with the 0xDEADBEEF value to actually use it.
void t_init(alloc_strat_e strat)
{
	currentBytesRequested = 0;
	alignmentSize = 4; // sysconf(_SC_PAGESIZE);
	pageSize = sysconf(_SC_PAGESIZE);
	currentAmountAllocated = 32768;
	headOfOccupied = NULL;
	numRegions = 0;
	// set how we're gonna do this
	currentMode = strat;

	// arbitrarily deciding to allocate 32kib of data
	// should be aligned
	void *mmapBase = mmap(NULL, currentAmountAllocated, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// printf("%p\n", headOfFree);
	if (mmapBase == MAP_FAILED)
	{
		throwError("ERROR! FAILED TO ALLOCATE MORE MEMORY!\n");
	}
	numRegions++;
	headOfFree = (header *)((char *)mmapBase + sizeof(footer));
	headOfFree->isFree = 1;
	headOfFree->size = 32768 - sizeof(footer) - sizeof(header);
	headOfFree->prevBlock = NULL;
	headOfFree->nextBlock = NULL;
	headOfFree->protectionBlock = 0xDEADBEEF; // a fun thing that's pretty common in network stuff too -- validates that header hasn't been corrupted

	footer *footOfFree = (footer *)((char *)headOfFree + (headOfFree->size - sizeof(footer)));
	footOfFree->isFree = 1;
	footOfFree->size = headOfFree->size;
	footOfFree-> protectionBlock = 0xDEADBEEF;
}

void *t_malloc(size_t size)
{
	currentBytesRequested += alignSize(size);
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
		allocateMoreMemory(32768);
	}

	header *currentNode = headOfFree;
	footer *currentFooter;

	size_t sizePostHeader = size + sizeof(header) + sizeof(footer);
	sizePostHeader = alignSize(sizePostHeader);
	while (1)
	{
		currentFooter = (footer *)((char *)currentNode + (currentNode->size - sizeof(footer)));
		if (currentNode->size >= sizePostHeader)
		{
			// large enough for us to do stuff!
			size_t sizeOfSegmentAfterAlloc = currentNode->size - sizePostHeader;

			// if we split this, the next chunk is literally too small for us to use, so let's just give the whole block of memory to this block so we don't lose it
			if (sizeOfSegmentAfterAlloc < alignSize(sizeof(header) + sizeof(footer) + 1))
			{
				currentNode->isFree = 0;
				currentFooter->isFree = 0;
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
	currentFooter-> protectionBlock = 0xDEADBEEF;
	return (void *)(currentNode + 1);
}

void t_free(void *ptr)
{
	if (headOfOccupied == NULL)
	{
		throwError("ERROR! No allocations present, so nothing to free");
	}

	header *currentNodeStart = (header *)(ptr)-1;
	// jankify way hehe:
	if (currentNodeStart->protectionBlock == 0xDEADBEEF && currentNodeStart->isFree == 0)
	{
		currentBytesRequested -= (currentNodeStart->size - sizeof(header) - sizeof(footer));
		// we'll pretend this is good enough to validate that we found a legit pointer...
		//  we found it, now do the freeing stuff
		if (currentNodeStart == headOfOccupied)
		{
			if (currentNodeStart->nextBlock == NULL)
			{
				headOfOccupied = NULL;
			}
			else
			{
				headOfOccupied = currentNodeStart->nextBlock;
				headOfOccupied->prevBlock = NULL;
			}
			currentNodeStart->isFree = 1;
			orderNewFreeData(currentNodeStart);
		}
		else
		{
			// currentNode presumably has a previous and next block, so cut it free
			if (currentNodeStart->prevBlock != NULL)
				currentNodeStart->prevBlock->nextBlock = currentNodeStart->nextBlock;
			if (currentNodeStart->nextBlock != NULL)
				currentNodeStart->nextBlock->prevBlock = currentNodeStart->prevBlock;
			currentNodeStart->prevBlock = NULL;
			currentNodeStart->nextBlock = NULL;
			currentNodeStart->isFree = 1;
			orderNewFreeData(currentNodeStart);
		}
		footer *footOfCurrentNode = (footer *)((char *)currentNodeStart + (currentNodeStart->size - sizeof(footer)));
		footOfCurrentNode->isFree = 1;
		footOfCurrentNode -> protectionBlock = 0xDEADBEEF;
	}
	else
	{
		throwError("ERROR! Cannot find address you're trying to free");
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
	coalesceFreeSectionsV2(currentNodeStart);
}

void allocateMoreMemory(size_t amountOfMemNeeded)
{
	// we have to allocate additional mem based on the page size since mmap gives mem based on page size I think
	size_t additionalMem = ((amountOfMemNeeded + pageSize - 1) / pageSize) * pageSize;
	currentAmountAllocated += additionalMem;
	void *newBase = mmap(NULL, additionalMem, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (newBase == MAP_FAILED)
	{
		throwError("ERROR! FAILED TO ALLOCATE MORE MEMORY!\n");
	}
	numRegions++;
	header *newSegment = (header *)((char *)newBase + sizeof(footer));
	newSegment->isFree = 1;
	newSegment->protectionBlock = 0xDEADBEEF;
	newSegment->size = additionalMem - sizeof(footer) - sizeof(header);

	footer *newSegFooter = (footer *)((char *)newSegment + (newSegment->size - sizeof(footer)));
	newSegFooter->size = newSegment->size;
	newSegFooter->isFree = 1;
	newSegFooter -> protectionBlock = 0xDEADBEEF;
	orderNewFreeData(newSegment);
}

void orderNewFreeData(header *address)
{
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
	}
}

void throwError(char *message)
{
	fprintf(stderr, "%s\n ", message);
	exit(1);
}

void orderNewlyAllocatedNode(header *targetNode)
{
	if (headOfOccupied == NULL)
	{
		headOfOccupied = targetNode;
		targetNode->nextBlock = NULL;
		targetNode->prevBlock = NULL;
		return;
	}
	else
	{
		headOfOccupied->prevBlock = targetNode;
		targetNode->nextBlock = headOfOccupied;
		targetNode->prevBlock = NULL;
		headOfOccupied = targetNode;
	}
	/*
	while (1)
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
	size_t sizePostHeader = size + sizeof(header) + sizeof(footer);
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
	footer *currentFooter = (footer *)((char *)currentNode + (currentNode->size - sizeof(footer)));
currentFooter-> protectionBlock = 0xDEADBEEF;
	// if we split this, the next chunk is literally too small for us to use, so let's just give the whole block of memory to this block so we don't lose it
	if (sizeOfSegmentAfterAlloc < alignSize(sizeof(header) + sizeof(footer) + 1))
	{
		currentNode->isFree = 0;
		currentFooter->isFree = 0;
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
	size_t sizePostHeader = size + sizeof(header) + sizeof(footer);
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
	footer *currentFooter = (footer *)((char *)currentNode + (currentNode->size - sizeof(footer)));
	currentFooter -> protectionBlock = 0xDEADBEEF;
	// if we split this, the next chunk is literally too small for us to use, so let's just give the whole block of memory to this block so we don't lose it
	if (sizeOfSegmentAfterAlloc < alignSize(sizeof(header) + sizeof(footer) + 1))
	{
		currentNode->isFree = 0;
		currentFooter->isFree = 0;
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

size_t getSysReqMem()
{
	return currentAmountAllocated;
}

void coalesceFreeSectionsV2(header *currentNode)
{
	header *theoreticallyNextHeader = (header *)((char *)currentNode + currentNode->size);
	if(theoreticallyNextHeader->protectionBlock == 0xDEADBEEF){
		if(theoreticallyNextHeader->isFree == 1){
			currentNode->size += theoreticallyNextHeader->size;
			if(theoreticallyNextHeader->prevBlock != NULL)
				theoreticallyNextHeader->prevBlock->nextBlock = theoreticallyNextHeader->nextBlock;
			else
				headOfFree = theoreticallyNextHeader->nextBlock;
			if(theoreticallyNextHeader->nextBlock != NULL)
				theoreticallyNextHeader->nextBlock->prevBlock = theoreticallyNextHeader->prevBlock;
			footer *mergedFooter = (footer *)((char *)currentNode + currentNode->size - sizeof(footer));
			mergedFooter->size = currentNode->size;
			mergedFooter->isFree = 1;
			mergedFooter->protectionBlock = 0xDEADBEEF;
		}
	}

	footer *theoreticallyFooter = ((footer *)((char *)currentNode - sizeof(footer)));
	if(theoreticallyFooter->protectionBlock == 0xDEADBEEF){
		if(theoreticallyFooter->isFree == 1){
			header *headerOfPrevious = (header *)((char *)currentNode - theoreticallyFooter->size);
			headerOfPrevious->size += currentNode->size;
			if(currentNode->prevBlock != NULL)
				currentNode->prevBlock->nextBlock = currentNode->nextBlock;
			else
				headOfFree = currentNode->nextBlock;
			if(currentNode->nextBlock != NULL)
				currentNode->nextBlock->prevBlock = currentNode->prevBlock;
			footer *mergedFooter = (footer *)((char *)headerOfPrevious + headerOfPrevious->size - sizeof(footer));
			mergedFooter->size = headerOfPrevious->size;
			mergedFooter->isFree = 1;
			mergedFooter->protectionBlock = 0xDEADBEEF;
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
	footer *footOfLeftover = (footer *)((char *)leftOverFreeChunk + (leftOverFreeChunk->size - sizeof(footer)));
	footOfLeftover->isFree = 1;
	footOfLeftover->size = sizeOfSegmentAfterAlloc;
	footOfLeftover -> protectionBlock = 0xDEADBEEF;
	
	if (leftOverFreeChunk->prevBlock)
		leftOverFreeChunk->prevBlock->nextBlock = leftOverFreeChunk;
	if (leftOverFreeChunk->nextBlock)
		leftOverFreeChunk->nextBlock->prevBlock = leftOverFreeChunk;

	currentNode->size = sizePostHeader;
	currentNode->isFree = 0;

	footer *footOfCurrentNode = (footer *)((char *)currentNode + (currentNode->size - sizeof(footer)));
	footOfCurrentNode->size = sizePostHeader;
	footOfCurrentNode->isFree = 0;
	footOfCurrentNode -> protectionBlock = 0xDEADBEEF;
	orderNewlyAllocatedNode(currentNode);

	if (needUpdateFirstNode)
	{
		headOfFree = leftOverFreeChunk;
	}
}

double memoryUtilizationPercentage()
{
	printf("%llu      %llu\n", currentBytesRequested, currentAmountAllocated);
	return ((double)currentBytesRequested / (double)currentAmountAllocated) * 100.0;
}