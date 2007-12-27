#ifndef UTIL_H
#define UTIL_H

#include <wchar.h>


/* Utility operations. */
int wcsmbscmp(wchar_t *wcs, char *mbs);
unsigned char *convert_bytearray(unsigned char *dst, const char *src, size_t length);

#endif
