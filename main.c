#include "./libtdmm/tdmm.h"
#include <sys/mman.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	t_init(BEST_FIT);
	void *mem = t_malloc(50);
	void *mem2 = t_malloc(50);
	void *mem3 = t_malloc(500000);

	printf("%p     %p     %p\n", mem, mem2, mem3);
	printf("%i\n", getSysReqMem());
	t_free(mem);
	t_free(mem2);
	t_free(mem3);

	mem3 = t_malloc(500000);//re-consume the previously allocated chunk
	mem2 = t_malloc(100); //verify that the new thing equals the previous mem address
	printf("%p     %p\n", mem2, mem3);
	printf("%i\n", getSysReqMem());

	return 0;
}
