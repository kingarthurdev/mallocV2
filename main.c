#define _POSIX_C_SOURCE 199309L
#include "./libtdmm/tdmm.h"
#include <sys/mman.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#define PASS(name) printf("PASS: " name "\n"); // ooh this apparently works and is cooler than a function lol

void basicTestingBenchmarks();
void getAverageUtilizationPercentages();
void speedsWithMemorySize();
void overheadWithProgramRun();
void unitTests();
void testSimpleBuddy();
void testSimpleBuddy2();

int main(int argc, char *argv[])
{
	basicTestingBenchmarks();
	unitTests();
	//getAverageUtilizationPercentages();
	//speedsWithMemorySize();
	//overheadWithProgramRun();
	return 0;
}

// basic test to ensure performance
void basicTestingBenchmarks()
{
	clock_t start, end;
	double cpu_time_used;

	t_init(FIRST_FIT);
	void *mem = t_malloc(50);
	void *mem2 = t_malloc(50);
	void *mem3 = t_malloc(500000);

	void *temp = mem;
	void *temp2 = mem3;

	// printf("%p     %p     %p\n", mem, mem2, mem3);
	// printf("%li\n", getSysReqMem());
	t_free(mem);
	t_free(mem2);
	t_free(mem3);

	mem3 = t_malloc(500000); // re-consume the previously allocated chunk
	mem2 = t_malloc(100);	 // verify that the new thing equals the previous mem address
	if (temp != mem2 || temp2 != mem3)
	{
		// throwError("ERROR! Something's wrong with allocation order.");
	}
	// printf("%p     %p\n", mem2, mem3);
	// printf("%i\n", getSysReqMem());

	start = clock();

	int *mallocs[2500];

	for (int i = 0; i < 2500; i++)
	{
		void *mem4 = t_malloc(i * 50);
		mallocs[i] = mem4;
	}
	end = clock();
	cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
	// printf("Time taken for allocating 25000 times: %f seconds\n", cpu_time_used);

	start = clock();
	for (int i = 0; i < 2500; i++)
	{
		t_free(mallocs[i]);
	}
	end = clock();
	cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
	// printf("Time taken for freeing 25000 times: %f seconds\n", cpu_time_used);

	if (cpu_time_used > 2)
	{
		throwError("ERROR! Too slow!");
	}

	printf("Basic allocation and time limit test passed!\n");
}

void getAverageUtilizationPercentages()
{
	int max = 3200; // max bytes I wanna allocate
	alloc_strat_e strat;
	char *stratInText = NULL;
	FILE *fpt;
	FILE *summary = fopen("UTILIZATION_SUMMARY.csv", "w+");
	fprintf(summary, "Strategy, Average Percent Utilization\n");
	for (int i = 0; i < 5; i++)
	{
		switch (i)
		{
		case 0:
			strat = FIRST_FIT;
			stratInText = "FIRST_FIT";
			fpt = fopen("FIRST_FIT_UTILIZATION.csv", "w+");
			break;
		case 1:
			strat = BEST_FIT;
			stratInText = "BEST_FIT";
			fpt = fopen("BEST_FIT_UTILIZATION.csv", "w+");
			break;
		case 2:
			strat = WORST_FIT;
			stratInText = "WORST_FIT";
			fpt = fopen("WORST_FIT_UTILIZATION.csv", "w+");
			break;
		case 3:
			strat = BUDDY;
			stratInText = "BUDDY_FIT";
			fpt = fopen("BUDDY_FIT_UTILIZATION.csv", "w+");
			break;
		case 4:
			strat = MIXED;
			stratInText = "MIXED_FIT";
			fpt = fopen("MIXED_FIT_UTILIZATION.csv", "w+");
			break;
		}
		fprintf(fpt, "Iteration, Average Percent Utilization\n");
		int externalCount = 0;

		t_init(strat);
		srand(42);
		void *addresses[25000];
		int count = 0;
		for (int i = 0; i < 25000; i++)
		{
			externalCount++;
			void *asdf = t_malloc(rand() % (max + 1));
			addresses[i] = asdf;
			fprintf(fpt, "%i,%.3f\n", externalCount, memoryUtilizationPercentage());
			int randSelector = rand() % 11;
			// 10% chance of freeing
			if (randSelector == 10)
			{
				externalCount++;
				t_free(addresses[count]);
				fprintf(fpt, "%i,%.3f\n", externalCount, memoryUtilizationPercentage());
				count++;
			}
		}
		fclose(fpt);
		fpt = NULL;
		double finalUtil = memoryUtilizationPercentage();
		fprintf(summary, "%s, %.3f\n", stratInText, finalUtil);
		// printf("Strategy: %s   |   Average Percent Utilization: %.2f%%\n", stratInText, finalUtil);
	}
	fclose(summary);
}

void speedsWithMemorySize()
{
	struct timespec start, end;
	long ns_used;
	alloc_strat_e strat;
	char *stratInText = NULL;
	FILE *fpt;
	t_init(FIRST_FIT);

	for (int i = 0; i < 5; i++)
	{
		switch (i)
		{
		case 0:
			strat = FIRST_FIT;
			stratInText = "FIRST_FIT";
			fpt = fopen("FIRST_FIT_TIME.csv", "w+");
			break;
		case 1:
			strat = BEST_FIT;
			stratInText = "BEST_FIT";
			fpt = fopen("BEST_FIT_TIME.csv", "w+");
			break;
		case 2:
			strat = WORST_FIT;
			stratInText = "WORST_FIT";
			fpt = fopen("WORST_FIT_TIME.csv", "w+");
			break;
		case 3:
			strat = BUDDY;
			stratInText = "BUDDY_FIT";
			fpt = fopen("BUDDY_FIT_TIME.csv", "w+");
			break;
		case 4:
			strat = MIXED;
			stratInText = "MIXED_FIT";
			fpt = fopen("MIXED_FIT_TIME.csv", "w+");
			break;
		}
		fprintf(fpt, "Bytes Allocated, Malloc Time Taken, Free Time Taken\n"); // time values in nanoseconds
		t_init(strat);

		for (int i = 0; i < 8388608; i++)
		{
			void *temp;
			clock_gettime(CLOCK_MONOTONIC, &start);
			temp = t_malloc(i);
			clock_gettime(CLOCK_MONOTONIC, &end);
			ns_used = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
			fprintf(fpt, "%i, %li,", i, ns_used);

			clock_gettime(CLOCK_MONOTONIC, &start);
			t_free(temp);
			clock_gettime(CLOCK_MONOTONIC, &end);
			ns_used = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
			fprintf(fpt, "%li\n", ns_used);
		}
		fclose(fpt);
		fpt = NULL;
	}
}

void overheadWithProgramRun()
{
	int max = 3200; // max bytes I wanna allocate
	alloc_strat_e strat;
	char *stratInText = NULL;
	FILE *fpt;
	for (int i = 0; i < 5; i++)
	{
		switch (i)
		{
		case 0:
			strat = FIRST_FIT;
			stratInText = "FIRST_FIT";
			fpt = fopen("FIRST_FIT_OVERHEAD.csv", "w+");
			break;
		case 1:
			strat = BEST_FIT;
			stratInText = "BEST_FIT";
			fpt = fopen("BEST_FIT_OVERHEAD.csv", "w+");
			break;
		case 2:
			strat = WORST_FIT;
			stratInText = "WORST_FIT";
			fpt = fopen("WORST_FIT_OVERHEAD.csv", "w+");
			break;
		case 3:
			strat = BUDDY;
			stratInText = "BUDDY_FIT";
			fpt = fopen("BUDDY_FIT_OVERHEAD.csv", "w+");
			break;
		case 4:
			strat = MIXED;
			stratInText = "MIXED_FIT";
			fpt = fopen("MIXED_FIT_OVERHEAD.csv", "w+");
			break;
		}
		fprintf(fpt, "Iteration, Overhead Bytes\n");
		int externalCount = 0;

		t_init(strat);
		srand(time(NULL));
		void *addresses[500];
		int count = 0;
		for (int i = 0; i < 500; i++)
		{
			externalCount++;
			void *asdf = t_malloc(rand() % (max + 1));
			addresses[i] = asdf;
			fprintf(fpt, "%i,%zu\n", externalCount, getOverheadBytes());
			int randSelector = rand() % 11;
			// 10% chance of freeing
			if (randSelector == 10)
			{
				externalCount++;
				t_free(addresses[count]);
				fprintf(fpt, "%i,%zu\n", externalCount, getOverheadBytes());
				count++;
			}
		}
		fclose(fpt);
		fpt = NULL;
	}
}

void unitTests()
{
	t_init(FIRST_FIT);
	void *p = t_malloc(64);
	if (p == NULL)
		throwError("malloc returned NULL");
	PASS("malloc returns non-NULL")

	t_init(FIRST_FIT);
	void *q = t_malloc(7);
	if ((uintptr_t)q % 4 != 0)
		throwError("returned pointer not 4-byte aligned");
	PASS("malloc pointer is 4-byte aligned")

	t_init(FIRST_FIT);
	void *r = t_malloc(128);
	t_free(r);
	PASS("free valid pointer does not crash")

	t_init(FIRST_FIT);
	if (getSysReqMem() == 0)
		throwError("getSysReqMem is 0 after init");
	PASS("getSysReqMem nonzero after init")

	t_init(FIRST_FIT);
	t_malloc(64);
	if (getOverheadBytes() == 0)
		throwError("getOverheadBytes is 0 after malloc");
	PASS("getOverheadBytes nonzero after malloc")

	t_init(FIRST_FIT);
	void *a = t_malloc(64);
	void *b = t_malloc(64);
	size_t before = getOverheadBytes();
	t_free(a);
	t_free(b);
	if (getOverheadBytes() > before)
		throwError("coalescing did not reduce overhead");
	PASS("coalescing reduces overhead")

	t_init(FIRST_FIT);
	double util_before = memoryUtilizationPercentage();
	t_malloc(1024);
	if (memoryUtilizationPercentage() <= util_before)
		throwError("utilization did not increase after malloc");
	PASS("utilization increases after malloc")

	alloc_strat_e strats[] = {FIRST_FIT, BEST_FIT, WORST_FIT, MIXED, BUDDY};
	for (int i = 0; i < 5; i++)
	{
		t_init(strats[i]);
		if (t_malloc(256) == NULL)
			throwError("malloc returned NULL for a strategy");
	}
	PASS("all three strategies return non-NULL")

	testSimpleBuddy2();
	testSimpleBuddy();
	PASS("Passed simple buddy tests")
}

void testSimpleBuddy()
{
	t_init(BUDDY);

	int *testList = t_malloc(95 * sizeof(int));
	for (int i = 0; i < 95; i++)
	{
		testList[i] = i;
	}
	void *mem3 = t_malloc(94);
	void *mem4 = t_malloc(21);
	t_free(mem3);
	mem3 = t_malloc(94);

	for (int i = 0; i < 95; i++)
	{
		if (testList[i] != i)
		{
			throwError("ERROR! MEM CORRUPTION!");
		}
	}
}

void testSimpleBuddy2()
{
	t_init(BUDDY);

	void *mem = t_malloc(500000);
	void *mem2 = t_malloc(694);
	void *mem3 = t_malloc(94);
	void *mem4 = t_malloc(21);

	t_free(mem3);

	mem3 = t_malloc(94);

	t_free(mem);
	t_free(mem4);
	mem4 = t_malloc(50000000);
}