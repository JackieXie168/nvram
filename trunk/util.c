/*
 *   util.c -- Utility functions.
 *
 *   Copyleft (c) 2007, Jan Kandziora <nvram@kandziora-ing.de>
 * 
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h> 
#include "util.h" 


/* Compare a wide string against a multibyte string. */
int wcsmbscmp(wchar_t *wcs, char *mbs)
{
	wchar_t  *wcs_p;
	wchar_t  wide_char;
	char     *mbs_p;
	int      i, consumed;

	/* Reset mbtowc shift state and string pointers. */
	mbtowc(NULL, NULL, 0);
	wcs_p=wcs;
	mbs_p=mbs;
	i=1;

	/* Loop through both strings. */
	for(;;) {
		/* Convert a single character. Fail on error. */
		consumed=mbtowc(&wide_char, mbs_p, MB_CUR_MAX);
		if (consumed == -1) return -i;

		/* Compare length. Return with "equal" if wcs and mbs both end here. */
		if ((*wcs_p == L'\0') && (consumed == 0)) return 0;
		if ((*wcs_p == L'\0') && (consumed > 0)) return -i;
		if ((*wcs_p != L'\0') && (consumed == 0)) return i;

		/* Compare latest char. */
		if (*wcs_p < wide_char) return -i;
		if (*wcs_p > wide_char) return i;

		/* Both equal. Check next char. */
		wcs_p++;
		mbs_p+=consumed;
		i++;
	}
}


/* Convert a string of hex noted bytes into a byte array. */
unsigned char *convert_bytearray(unsigned char *dst, const char *src, size_t length)
{
	const char    *src_p;
	unsigned char *dst_p;
	size_t         i;

	src_p=src;
	for (i=0, dst_p=dst; i<length; i++, dst_p++) {
		/* Scan or fail. */
		if (sscanf(src_p, "%2x", (unsigned int*) dst_p) != 1) return NULL;

		/* Check the second char was an hex digit. */
		src_p++;
		if (!isxdigit(*src_p)) return NULL;

		/* Check for space separator. */
		src_p++;
		if ((i < length-1) && (*src_p != ' ')) return NULL;

		/* Next chars. */
		src_p++;
	}

	/* Check there are no more chars. */
	if (*(src_p-1) != '\0') return NULL;

	/* All ok. */
	return dst;
}
