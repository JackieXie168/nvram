/*
 *   detect.c -- Detect mainboard and BIOS version from DMI.
 *
 *   Copyleft (c) 2007, Jan Kandziora <nvram@kandziora-ing.de>
 * 
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

#include "detect.h"
#include "nvram.h"


/* DMI record. */
typedef struct
{
	uint8_t  type;
	uint8_t  size;
	uint16_t handle;
	char     data[];
} dmi_record_t;


/* Get a string pointer from a DMI structure. */
const char *dmi_string(dmi_record_t *dmi_record, int position, char *limit)
{
	char *string_pointer;
	char  string_number;

	/* Get string number. */
	string_number=dmi_record->data[position];

	/* Return pointer to a zero (resembling an empty string) if that string is not existent. */
  if (!string_number) return &(dmi_record->data[position]);

	/* Seek behind dmi record. */
	string_pointer=(char*)dmi_record + dmi_record->size;

	/* Scan for a certain string. */ 
	while (--string_number) {
		/* Fix for broken DMI entry. */
		if (string_pointer >= limit) return NULL;

		/* Seek for next string. */ 
		string_pointer+=strlen(string_pointer)+1;
	}

	/* Return the valid const string. */
	return (char const* const) string_pointer;
}


/* Cook a dmi string. */
char *dmi_string_cook(char *s)
{
	char *p, *q;

	/* Replace the character / by %. */
	while ((p=strchr(s,'/')) != NULL) *p='%';

	/* Remove any leading whitespace. */
	p=s, q=s;
	while ((*p != '\0') && isspace(*p)) p++;
	while (*p != '\0') *q=*p, q++, p++;
	*q='\0';

	/* Remove any trailing whitespace. */
	p=s;
	if (*p != '\0') {
		do p++; while (*p != '\0');
		p--;
		while (isspace(*p)) p--;
		p++;
		*p='\0';
	}

	/* Return changed string. */
	return s;
}


/* Detect mainboard and bios version from DMI table. */
int dmi_detect(settings_t *settings, hardware_t *hardware)
{
  char          buffer[16];
	off_t         position;
	uint32_t      base;
	uint16_t      count, size;
	char         *dmi_table;
	char         *dmi_record, *next_dmi_record;
	unsigned int  i;
	char          found_bios_info=0, found_system_info=0, found_board_info=0;
	int           result=-1;
	int           mem;

	/* Open main memory as a file. */
	if ((mem=open("/dev/mem", O_RDONLY)) == -1) goto detect_open_fail; 

 	/* Seek to start of BIOS ROM (mirrored into RAM from 896k to 1M). */
  if (lseek(mem, 0xE0000L, SEEK_SET) == -1) goto detect_dmi_pointer_fail;

	/* Scan BIOS ROM for DMI pointer. */
	/* This pointer starts at a paragraph, so we can read blocks of 16 byte. */
	for (position=0; position < 0x1FFF; position++) {
		/* Read a record. */
		if (read(mem, buffer, 16) != 16) goto detect_dmi_pointer_fail;

		/* Check for DMI pointer. */
		if (memcmp(buffer, "_DMI_", 5) == 0) {
			/* Found. */
			break;
		}	
	}

	/* Check if the DMI pointer was found. */
	if (position == 0x2000) goto detect_dmi_pointer_fail;
			
	/* Get the properties of the DMI table. */
	size=*((uint16_t*) &buffer[6]);
	base=*((uint32_t*) &buffer[8]);
	count=*((uint16_t*) &buffer[12]);

	/* Seek to start of DMI table. */
	if (lseek(mem, (long)base, SEEK_SET) == -1) goto detect_dmi_pointer_fail;

	/* Read DMI table. */
	dmi_table=malloc(size);
	if (read(mem, dmi_table, size) != size) goto detect_dmi_table_fail;

#ifdef DEBUG
	fwprintf(stderr, L"DMI table found: base: 0x%X, size: %d, count: %d\n", base, size, count);
#endif	

	/* From here, result is always "ok". */
	result=0;

	/* Go though all records. */
	dmi_record=dmi_table;
	for (i=0; i < count; i++) {
		/* Fix for broken DMI table. Break if record count is leading us out of DMI table space. */
		if ((int)dmi_record >= ((int)dmi_table+size-4)) break;

#ifdef DEBUG
		fwprintf(stderr, L"DMI data block %3d at offset 0x%03X: type %3d, size %d\n", i, (dmi_record-dmi_table), ((dmi_record_t*)dmi_record)->type, ((dmi_record_t*)dmi_record)->size );
#endif               

 		/* Get address of next DMI record. */
		next_dmi_record=dmi_record+((dmi_record_t*)dmi_record)->size;
		while (next_dmi_record[0] || next_dmi_record[1]) next_dmi_record++;
		next_dmi_record+=2;

		/* Check for BIOS info record. */
		if (!found_bios_info && ((dmi_record_t*)dmi_record)->type == 0) {
			/* Found BIOS info. */
			found_bios_info++;
			hardware->bios_vendor=strdup(dmi_string((dmi_record_t*)dmi_record, 0, next_dmi_record));
			hardware->bios_version=strdup(dmi_string((dmi_record_t*)dmi_record, 1, next_dmi_record));
			hardware->bios_release_date=strdup(dmi_string((dmi_record_t*)dmi_record, 4, next_dmi_record));

			/* "Cook" strings if desired. */
			if (!settings->dmi_raw) {
				dmi_string_cook(hardware->bios_vendor);
				dmi_string_cook(hardware->bios_version); 
				dmi_string_cook(hardware->bios_release_date);
			}
		}

		/* Check for system info record. */
		if (!found_board_info && ((dmi_record_t*)dmi_record)->type == 1) {
			/* Found system info. */
			found_system_info++;

			hardware->system_manufacturer=strdup(dmi_string((dmi_record_t*)dmi_record, 0, next_dmi_record));
			hardware->system_productcode=strdup(dmi_string((dmi_record_t*)dmi_record, 1, next_dmi_record));
			hardware->system_version=strdup(dmi_string((dmi_record_t*)dmi_record, 2, next_dmi_record));

			/* "Cook" strings if desired. */
			if (!settings->dmi_raw) {
				dmi_string_cook(hardware->system_manufacturer);
				dmi_string_cook(hardware->system_productcode);
				dmi_string_cook(hardware->system_version);
			}
		}

		/* Check for board info record. */
		if (!found_board_info && ((dmi_record_t*)dmi_record)->type == 2) {
			/* Found board info. */
			found_board_info++;

			hardware->board_manufacturer=strdup(dmi_string((dmi_record_t*)dmi_record, 0, next_dmi_record));
			hardware->board_productcode=strdup(dmi_string((dmi_record_t*)dmi_record, 1, next_dmi_record));
			hardware->board_version=strdup(dmi_string((dmi_record_t*)dmi_record, 2, next_dmi_record));

			/* "Cook" strings if desired. */
			if (!settings->dmi_raw) {
				dmi_string_cook(hardware->board_manufacturer);
				dmi_string_cook(hardware->board_productcode); 
				dmi_string_cook(hardware->board_version);
			}
		}

		/* Seek to next DMI record. */
		dmi_record=next_dmi_record;
	}

detect_dmi_table_fail:
	free(dmi_table);
detect_dmi_pointer_fail:
	close(mem);
detect_open_fail:
	return result;
}
