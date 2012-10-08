#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "model.h"
#include "stacktrace.h"

#define MAX_TRACE_LEN 100

#define CONFIG_STACKTRACE
/** Print a backtrace of the current program state. */
void print_trace(void)
{
#ifdef CONFIG_STACKTRACE
	print_stacktrace(stdout);
#else
	void *array[MAX_TRACE_LEN];
	char **strings;
	int size, i;

	size = backtrace(array, MAX_TRACE_LEN);
	strings = backtrace_symbols(array, size);

	printf("\nDumping stack trace (%d frames):\n", size);

	for (i = 0; i < size; i++)
		printf("\t%s\n", strings[i]);

	free(strings);
#endif /* CONFIG_STACKTRACE */
}

void model_print_summary(void)
{
	model->print_summary();
}

void assert_hook(void)
{
	printf("Add breakpoint to line %u in file %s.\n",__LINE__,__FILE__);
}
