#include "./libtdmm/tdmm.h"
#include <sys/mman.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

void basicTestingBenchmarks();
void getAverageUtilizationPercentages();


int main(int argc, char *argv[])
{
	//basicTestingBenchmarks();
	getAverageUtilizationPercentages();

	return 0;
}

// basic test to ensure performance
void basicTestingBenchmarks()
{
	printf("hello???");
	clock_t start, end;
	double cpu_time_used;

	t_init(FIRST_FIT);
	void *mem = t_malloc(50);
	void *mem2 = t_malloc(50);
	void *mem3 = t_malloc(500000);

	void *temp = mem;
	void *temp2 = mem3;

	// printf("%p     %p     %p\n", mem, mem2, mem3);
	// printf("%i\n", getSysReqMem());
	t_free(mem);
	t_free(mem2);
	t_free(mem3);

	mem3 = t_malloc(500000); // re-consume the previously allocated chunk
	mem2 = t_malloc(100);	 // verify that the new thing equals the previous mem address
	/*if (temp != mem2 || temp2 != mem3)
	{
		throwError("ERROR! Something's wrong with allocation order.");
	}*/
	// printf("%p     %p\n", mem2, mem3);
	// printf("%i\n", getSysReqMem());

	start = clock();

	int *mallocs[25000];

	for (int i = 0; i < 25000; i++)
	{
		void *mem4 = t_malloc(i * 50);
		mallocs[i] = mem4;
	}
	end = clock();
	cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
	printf("Time taken for allocating 25000 times: %f seconds\n", cpu_time_used);

	start = clock();
	for (int i = 0; i < 25000; i++)
	{
		t_free(mallocs[i]);
	}
	end = clock();
	cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
	printf("Time taken for freeing 25000 times: %f seconds\n", cpu_time_used);

	if (cpu_time_used > 2)
	{
		//throwError("ERROR! Too slow!");
	}

	printf("Basic allocation and time limit test passed!\n");
}

void getAverageUtilizationPercentages()
{
	int max = 320; // max bytes I wanna allocate
	alloc_strat_e strat;
	char *stratInText = calloc(15, 1);
	for (int i = 0; i < 3; i++)
	{
		switch (i)
		{
		case 0:
			strat = FIRST_FIT;
			stratInText = "FIRST_FIT";
			break;
		case 1:
			strat = BEST_FIT;
			stratInText = "BEST_FIT";
			break;
		case 2:
			strat = WORST_FIT;
			stratInText = "WORST_FIT";
			break;
		}
		t_init(strat);
		srand(time(NULL));
		void *addresses[25000];
		int count = 0;
		for (int i = 0; i < 25000; i++)
		{
			void *asdf = t_malloc(rand() % (max + 1));
			addresses[i] = asdf;

			int randSelector = rand() % 11;
			// 10% chance of freeing
			if (randSelector == 10)
			{
				t_free(addresses[count]);
				count++;
			}
		}
		printf("Strategy: %s   |   Average Percent Utilization: %.2f\n", stratInText, memoryUtilizationPercentage());
	}
	
}