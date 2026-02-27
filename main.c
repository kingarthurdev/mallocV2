#include "./libtdmm/tdmm.h"
#include <sys/mman.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	t_init(FIRST_FIT);
	void *mem = t_malloc(50);
	void *mem2 = t_malloc(50);
	void *mem3 = t_malloc(500000);

	printf("%p     %p     %p\n", mem, mem2, mem3);
	printf("%i\n", getSysReqMem());
	t_free(mem);
	t_free(mem2);
	t_free(mem3);

	mem3 = t_malloc(500000); // re-consume the previously allocated chunk
	mem2 = t_malloc(100);	 // verify that the new thing equals the previous mem address
	printf("%p     %p\n", mem2, mem3);
	printf("%i\n", getSysReqMem());

	for (int i = 0; i < 500000; i++)
	{
		void *mem4 = t_malloc(i);
		void *mem5 = t_malloc(i);
		t_free(mem5);
		t_free(mem4);
	}
	return 0;
}
