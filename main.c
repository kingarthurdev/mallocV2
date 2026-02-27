#include "./libtdmm/tdmm.h"
#include <sys/mman.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[])
{

	clock_t start, end;
	double cpu_time_used;

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

	start = clock();

	int *mallocs[25000];

	for (int i = 0; i < 25000; i++)
	{
		void *mem4 = t_malloc(i * 50);
		mallocs[i] = mem4;
	}
	end = clock();
	cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
	printf("Time taken for allocation: %f seconds\n", cpu_time_used);


	start = clock();
	for (int i = 0; i < 25000; i++)
	{
		t_free(mallocs[i]);
	}
	end = clock();
	cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
	printf("Time taken for free: %f seconds\n", cpu_time_used);

	return 0;
}
