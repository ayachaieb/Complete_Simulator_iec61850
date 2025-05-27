#ifndef ASSERT_HANDLER_H
#define ASSERT_HANDLER_H

#include <stdio.h>
#include <stdlib.h>

void assert_handler(const char *condition, const char *file, int line);

#ifdef NDEBUG
#define ASSERT(condition) ((void)0)
#else
#define ASSERT(condition) \
    if (!(condition)) { \
        assert_handler(#condition, __FILE__, __LINE__); \
    }
#endif

#endif  // ASSERT_HANDLER_H