#include "assert_handler.h"

void assert_handler(const char *condition, const char *file, int line)
{
    fprintf(stderr, "Assertion failed: %s\n"
                    "File: %s\n"
                    "Line: %d\n",
                    condition, file, line);
    abort();
}