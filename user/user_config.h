#include <stdio.h>
#include <stdarg.h>

int os_printf_plus(const char *format, ...);
size_t strnlen(const char *s, size_t maxlen);

//#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define DEBUG_PRINTF(x, ...) { os_printf(x, ##__VA_ARGS__); }
#else
#define DEBUG_PRINTF(x, ...) do {} while(0)
#endif
