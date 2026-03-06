#include "tdmm.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

// this basically allows us to have chunks that are up to 4mb large, makes 1 bucket for each size range
#define numBuckets 30
// hold each pointer to each bucket.
header *buddyBuckets[numBuckets];
header *headOfFree;
header *headOfOccupied;
alloc_strat_e currentMode;

unsigned long long int currentAmountAllocated;
unsigned long long int currentBytesRequested;
int alignmentSize;
int pageSize;
int numRegions = 0;
int numBlocks = 0;
int isMixed = 0;
int currentPosInMaxSegs = 0;

regionMap mapsOfTotalRegions[1000]; //lowk arbitrary decision to only be able to map 1000 different regions... hopefully doesn't bite me in the ass...

// TODO: do some integrity checks with the 0xDEADBEEF value to actually use it.
void t_init(alloc_strat_e strat)
{
	currentBytesRequested = 0;
	alignmentSize = 4;
	pageSize = sysconf(_SC_PAGESIZE);
	currentAmountAllocated = 4096;
	headOfOccupied = NULL;
	numRegions = 0;
	numBlocks = 1;
	// set how we're gonna do this
	currentMode = strat;
	// arbitrarily deciding to allocate 4096 bytes of data
	// should be aligned
	void *mmapBase = mmap(NULL, currentAmountAllocated, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// printf("%p\n", headOfFree);
	if (mmapBase == MAP_FAILED)
	{
		throwError("ERROR! FAILED TO ALLOCATE MORE MEMORY!\n");
	}
	numRegions++;
	if (currentMode == BUDDY)
	{
		// zero initialize everything to prevent any weird stuff from going on
		memset(buddyBuckets, 0, (numBuckets * sizeof(header *)));
		currentPosInMaxSegs = 0;
		isMixed = 0;
		mapsOfTotalRegions[currentPosInMaxSegs].address = (uintptr_t)mmapBase;
		mapsOfTotalRegions[currentPosInMaxSegs].size = currentAmountAllocated;
		currentPosInMaxSegs++;
		buddyBuckets[(int)ceil(log2(currentAmountAllocated))] = mmapBase;
		// useless line of code, but too lazy to refactor
		header *headOfBucket = buddyBuckets[(int)ceil(log2(currentAmountAllocated))];
		headOfBucket->isFree = 1;
		headOfBucket->size = currentAmountAllocated;
		headOfBucket->nextBlock = NULL;
		headOfBucket->prevBlock = NULL;
		headOfBucket->protectionBlock = 0xDEADBEEF;
	}
	else
	{
		headOfFree = (header *)((char *)mmapBase + sizeof(footer));
		headOfFree->isFree = 1;
		headOfFree->size = currentAmountAllocated - sizeof(footer) - sizeof(header);
		headOfFree->prevBlock = NULL;
		headOfFree->nextBlock = NULL;
		headOfFree->protectionBlock = 0xDEADBEEF; // a fun thing that's pretty common in network stuff too -- validates that header hasn't been corrupted

		footer *footOfFree = (footer *)((char *)headOfFree + (headOfFree->size - sizeof(footer)));
		footOfFree->isFree = 1;
		footOfFree->size = headOfFree->size;
		footOfFree->protectionBlock = 0xDEADBEEF;
		if (currentMode == MIXED)
		{
			isMixed = 1;
			currentMode = WORST_FIT; // so it rotates back around to become first_fit
		}
	}
}

void *t_malloc(size_t size)
{
	currentBytesRequested += alignSize(size);
	rotateStrats();
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
	else if (currentMode == BUDDY)
	{
		return doBuddyFit(size);
	}
	return NULL;
}

void *doFirstFit(size_t size)
{
	if (headOfFree == NULL)
	{
		allocateMoreMemory(size);
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
				currentNode = allocateMoreMemory(sizePostHeader);
			}
			else
			{
				currentNode = currentNode->nextBlock;
			}
		}
	}
	currentFooter->protectionBlock = 0xDEADBEEF;
	return (void *)(currentNode + 1);
}

void t_free(void *ptr)
{
	if (currentMode == BUDDY)
	{
		doBuddyFree(ptr);
		return;
	}

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
		footOfCurrentNode->protectionBlock = 0xDEADBEEF;
	}
	else
	{
		throwError("ERROR! Cannot find address you're trying to free");
	}
	coalesceFreeSections(currentNodeStart);
}

header *allocateMoreMemory(size_t amountOfMemNeeded)
{
	// we have to allocate additional mem based on the page size since mmap gives mem based on page size I think
	size_t additionalMem = ((amountOfMemNeeded + sizeof(header) + sizeof(footer) + pageSize - 1) / pageSize) * pageSize;
	currentAmountAllocated += additionalMem;
	void *newBase = mmap(NULL, additionalMem, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (newBase == MAP_FAILED)
	{
		throwError("ERROR! FAILED TO ALLOCATE MORE MEMORY!\n");
	}
	numRegions++;
	numBlocks++;
	header *newSegment = (header *)((char *)newBase + sizeof(footer));
	newSegment->isFree = 1;
	newSegment->protectionBlock = 0xDEADBEEF;
	newSegment->size = additionalMem - sizeof(footer) - sizeof(header);

	footer *newSegFooter = (footer *)((char *)newSegment + (newSegment->size - sizeof(footer)));
	newSegFooter->size = newSegment->size;
	newSegFooter->isFree = 1;
	newSegFooter->protectionBlock = 0xDEADBEEF;
	orderNewFreeData(newSegment);
	return newSegment;
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
	else // if (address < headOfFree)
	{
		// put address before the headOfFree in the linked list
		address->nextBlock = headOfFree;
		address->prevBlock = NULL;
		headOfFree->prevBlock = address;
		headOfFree = address;
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
				smallestSectionSoFar = allocateMoreMemory(sizePostHeader);
				break;
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
	currentFooter->protectionBlock = 0xDEADBEEF;
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
				largestSectionSoFar = allocateMoreMemory(sizePostHeader);
				break;
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
	currentFooter->protectionBlock = 0xDEADBEEF;
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

void coalesceFreeSections(header *currentNode)
{
	header *theoreticallyNextHeader = (header *)((char *)currentNode + currentNode->size);
	if (theoreticallyNextHeader->protectionBlock == 0xDEADBEEF)
	{
		if (theoreticallyNextHeader->isFree == 1)
		{
			numBlocks--;
			currentNode->size += theoreticallyNextHeader->size;
			if (theoreticallyNextHeader->prevBlock != NULL)
				theoreticallyNextHeader->prevBlock->nextBlock = theoreticallyNextHeader->nextBlock;
			else
				headOfFree = theoreticallyNextHeader->nextBlock;
			if (theoreticallyNextHeader->nextBlock != NULL)
				theoreticallyNextHeader->nextBlock->prevBlock = theoreticallyNextHeader->prevBlock;
			footer *mergedFooter = (footer *)((char *)currentNode + currentNode->size - sizeof(footer));
			mergedFooter->size = currentNode->size;
			mergedFooter->isFree = 1;
			mergedFooter->protectionBlock = 0xDEADBEEF;
		}
	}

	footer *theoreticallyFooter = ((footer *)((char *)currentNode - sizeof(footer)));
	if (theoreticallyFooter->protectionBlock == 0xDEADBEEF)
	{
		if (theoreticallyFooter->isFree == 1)
		{
			numBlocks--;
			header *headerOfPrevious = (header *)((char *)currentNode - theoreticallyFooter->size);
			headerOfPrevious->size += currentNode->size;
			if (currentNode->prevBlock != NULL)
				currentNode->prevBlock->nextBlock = currentNode->nextBlock;
			else
				headOfFree = currentNode->nextBlock;
			if (currentNode->nextBlock != NULL)
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
	footOfLeftover->protectionBlock = 0xDEADBEEF;

	if (leftOverFreeChunk->prevBlock)
		leftOverFreeChunk->prevBlock->nextBlock = leftOverFreeChunk;
	if (leftOverFreeChunk->nextBlock)
		leftOverFreeChunk->nextBlock->prevBlock = leftOverFreeChunk;

	numBlocks++;
	currentNode->size = sizePostHeader;
	currentNode->isFree = 0;

	footer *footOfCurrentNode = (footer *)((char *)currentNode + (currentNode->size - sizeof(footer)));
	footOfCurrentNode->size = sizePostHeader;
	footOfCurrentNode->isFree = 0;
	footOfCurrentNode->protectionBlock = 0xDEADBEEF;
	orderNewlyAllocatedNode(currentNode);

	if (needUpdateFirstNode)
	{
		headOfFree = leftOverFreeChunk;
	}
}

double memoryUtilizationPercentage()
{
	return ((double)currentBytesRequested / (double)currentAmountAllocated) * 100.0;
}

size_t getOverheadBytes()
{
	return numBlocks * (sizeof(header) + sizeof(footer));
}

void rotateStrats()
{
	if (isMixed)
	{
		if (currentMode == FIRST_FIT)
		{
			currentMode = BEST_FIT;
		}
		else if (currentMode == BEST_FIT)
		{
			currentMode = WORST_FIT;
		}
		else if (currentMode == WORST_FIT)
		{
			currentMode = FIRST_FIT;
		}
	}
}
header *buddyAllocateMoreMemory(size_t amountOfMemNeeded)
{
	// amount needed + header overhead aligned to the nearest poweer of 2
	size_t alignedBaseMem = pow(2, (int)ceil(log2(amountOfMemNeeded + sizeof(header))));

	// now aligned to page size to prevent waste
	alignedBaseMem = ((alignedBaseMem + pageSize - 1) / pageSize) * pageSize;
	currentAmountAllocated += alignedBaseMem;
	header *newBase = mmap(NULL, alignedBaseMem, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (newBase == MAP_FAILED)
	{
		throwError("ERROR! FAILED TO ALLOCATE MORE MEMORY!\n");
	}
	mapsOfTotalRegions[currentPosInMaxSegs].address = (uintptr_t)newBase;
	mapsOfTotalRegions[currentPosInMaxSegs].size = alignedBaseMem;
	currentPosInMaxSegs++;
	newBase->size = alignedBaseMem;
	newBase->nextBlock = NULL;
	newBase->prevBlock = NULL;
	splitBlocks(newBase, amountOfMemNeeded);
	newBase->protectionBlock = 0xDEADBEEF;
	return newBase;
}

void splitBlocks(header *currentBlock, int alignedSize)
{
	if (((currentBlock->size) / 2) >= alignedSize)
	{
		currentBlock->size = (currentBlock->size) / 2;
		int bucket = (int)log2(currentBlock->size);
		header *newSplitChunk = (header *)((char *)currentBlock + currentBlock->size);
		newSplitChunk->isFree = 1;
		newSplitChunk->size = currentBlock->size;
		newSplitChunk->nextBlock = buddyBuckets[bucket];
		newSplitChunk->prevBlock = NULL;
		newSplitChunk->protectionBlock = 0xDEADBEEF;
		if (buddyBuckets[bucket] != NULL)
		{
			buddyBuckets[bucket]->prevBlock = newSplitChunk;
		}
		buddyBuckets[bucket] = newSplitChunk;
		splitBlocks(currentBlock, alignedSize);
	}
	else
	{
		int bucket = (int)log2(currentBlock->size);
		currentBlock->isFree = 1;
		currentBlock->nextBlock = buddyBuckets[bucket];
		currentBlock->prevBlock = NULL;
		if (buddyBuckets[bucket] != NULL)
		{
			buddyBuckets[bucket]->prevBlock = currentBlock;
		}
		buddyBuckets[bucket] = currentBlock;
	}
}

void *doBuddyFit(size_t size)
{
	int bucket = (int)ceil(log2(size + sizeof(header)));
	int alignedSize = pow(2, bucket);

	// if the size we need is empty
	if (buddyBuckets[bucket] == NULL)
	{
		int foundBigEnough = 0;
		for (int i = bucket + 1; i < numBuckets; i++)
		{
			header *currentBlock = buddyBuckets[i];
			// we found one big enough
			if (currentBlock != NULL)
			{
				foundBigEnough = 1;
				// remove from its bucket before splitting
				buddyBuckets[i] = currentBlock->nextBlock;
				if (currentBlock->nextBlock != NULL)
					currentBlock->nextBlock->prevBlock = NULL;
				currentBlock->nextBlock = NULL;
				currentBlock->prevBlock = NULL;
				splitBlocks(currentBlock, alignedSize);
				break;
			}
		}
		if (!foundBigEnough)
		{
			buddyAllocateMoreMemory(alignedSize);
		}
	}
	header *newlyAssigned = buddyBuckets[bucket];
	if (newlyAssigned != NULL) //umm it shouldn't be null, but...
	{
		newlyAssigned->isFree = 0;
		newlyAssigned->protectionBlock = 0xDEADBEEF;
		buddyBuckets[bucket] = newlyAssigned->nextBlock;
		if (newlyAssigned->nextBlock != NULL)
			newlyAssigned->nextBlock->prevBlock = NULL;
		newlyAssigned->nextBlock = NULL;
		newlyAssigned->prevBlock = NULL;
	}else{
		throwError("ERMMMMM");
	}
	return (void *)(newlyAssigned + 1);
}

void doBuddyFree(void *ptr)
{
	header *currentNodeStart = ((header *)ptr) - 1;

	// we'll pretend this is good enough to validate that we found a legit pointer...
	// jankify way hehe:
	if (currentNodeStart->protectionBlock == 0xDEADBEEF && currentNodeStart->isFree == 0)
	{
		currentBytesRequested -= (currentNodeStart->size - sizeof(header));
		currentNodeStart->isFree = 1;
		if (currentNodeStart->nextBlock != NULL)
		{
			currentNodeStart->nextBlock->prevBlock = currentNodeStart->prevBlock;
		}
		if (currentNodeStart->prevBlock != NULL)
		{
			currentNodeStart->prevBlock->nextBlock = currentNodeStart->nextBlock;
		}
		currentNodeStart->nextBlock = NULL;
		currentNodeStart->prevBlock = NULL;
		mergeHanging(currentNodeStart);
	}
	else
	{
		throwError("ERROR! Cannot find address you're trying to free");
	}
}

void mergeHanging(header *hangingBlock)
{
	uintptr_t potentialNewAddr =((uintptr_t)hangingBlock ^ hangingBlock->size);
	header *buddyBlock = (header *)potentialNewAddr;
	header *endBlock;
	int shouldGo = 0;

	for (int i = 0; i < currentPosInMaxSegs; i++) {
		if ((uintptr_t)hangingBlock >= mapsOfTotalRegions[i].address &&
		    (uintptr_t)hangingBlock < mapsOfTotalRegions[i].address + mapsOfTotalRegions[i].size)
		{
			if (potentialNewAddr >= mapsOfTotalRegions[i].address &&
			    potentialNewAddr < mapsOfTotalRegions[i].address + mapsOfTotalRegions[i].size)
				shouldGo = 1;
			break;
		}
	}

	if (shouldGo && (buddyBlock -> protectionBlock) == 0xDEADBEEF)
	{
		if ((buddyBlock->isFree) && (buddyBlock->size == hangingBlock->size))
		{
			int buddyBucket = (int)log2(buddyBlock->size);

			if (buddyBlock->nextBlock != NULL)
			{
				buddyBlock->nextBlock->prevBlock = buddyBlock->prevBlock;
			}

			if (buddyBlock->prevBlock == NULL)
			{
				// if (buddyBuckets[buddyBucket] == buddyBlock)
				buddyBuckets[buddyBucket] = buddyBlock->nextBlock;
			}
			else
			{
				buddyBlock->prevBlock->nextBlock = buddyBlock->nextBlock;
			}

			// Now buddy block is also detached (hanging)
			buddyBlock->nextBlock = NULL;
			buddyBlock->prevBlock = NULL;

			// case: buddy block is first in memory
			if ((uintptr_t)hangingBlock > (uintptr_t)buddyBlock)
			{
				buddyBlock->size = (buddyBlock->size) * 2;
				endBlock = buddyBlock;
			}
			else
			{
				// case: hanging block is first
				hangingBlock->size = (hangingBlock->size) * 2;
				endBlock = hangingBlock;
			}
			mergeHanging(endBlock);
		}
		else
		{
			int bucket = (int)ceil(log2(hangingBlock->size));
			if (buddyBuckets[bucket] == NULL)
			{
				buddyBuckets[bucket] = hangingBlock;
			}
			else
			{
				hangingBlock->nextBlock = buddyBuckets[bucket];
				buddyBuckets[bucket]->prevBlock = hangingBlock;
				buddyBuckets[bucket] = hangingBlock;
			}
		}
	}
}