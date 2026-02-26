#include "tdmm.h"
#include <sys/mman.h>

typedef struct header {
	int size;
	int isFree; //0 for no, 1 for yes
	struct header *nextBlock;
	struct header *prevBlock;
} header;

header *headOfFree;
header *headOfOccupied;

void t_init(alloc_strat_e strat) {
	//arbitrarily deciding to allocate 32kib of data 
	//should be aligned
	headOfFree = mmap(NULL, 32768 + sizeof(header), PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
	headOfFree->isFree = 1;
	headOfFree->size = 32768;// we conceal the true size of the mem region since the header is gonna take 24bytes of it 
	headOfFree->prevBlock = NULL;
	headOfFree->nextBlock=NULL;
	

	// TODO: Implement this
}

void *t_malloc(size_t size) {
	// TODO: Implement this
	return NULL;
}

void t_free(void *ptr) {
  	// TODO: Implement this
}
