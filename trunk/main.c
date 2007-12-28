/*
 *   nvram -- A tool to operate on extended nvram most modern PC chipsets offer.
 *
 *   Copyleft (c) 2007, Jan Kandziora <nvram@kandziora-ing.de>
 * 
 */

#ifdef HAS_LOCALE
#include <locale.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

#include "config.h"
#include "detect.h"
#include "list.h"
#include "map.h"
#include "nvram.h"
#include "nvram_op.h"
#include "token.h"
#include "util.h"


/* Usage message. */
const wchar_t USAGE[] = L"USAGE: nvram dmi\n       nvram list\n       nvram get identifier [identifier]...\n       nvram set identifier value [identifier value]...\n";


/* List the available identifiers. */
void command_list(int argc, char *argv[], struct list_head *mapping_list)
{
	struct list_head *pos;
	map_field_t      *map_field;
	long              i;

	/* Go through all map fields. */
	list_for_each(pos, mapping_list) {
		/* Get the current field. */
		map_field=list_entry(pos, map_field_t, list);

		/* Switch by field type. */
		switch (map_field->type) {
			case MAP_FIELD_TYPE_BYTEARRAY:
				/* Print bytearray field. */
				fwprintf(stdout, L"bytearray %ls 0x%02x %d\n", map_field->name, map_field->data.bytearray.position, map_field->data.bytearray.length);
				break;

			case MAP_FIELD_TYPE_STRING:
				/* Print string field. */
				fwprintf(stdout, L"string %ls 0x%02x %d\n", map_field->name, map_field->data.string.position, map_field->data.string.length);
				break;

			case MAP_FIELD_TYPE_BITFIELD:
				/* Print bitfield. */
				fwprintf(stdout, L"bitfield %ls %d ", map_field->name, map_field->data.bitfield.length);
				for (i=0; i < map_field->data.bitfield.length; i++) {
					fwprintf(stdout, L"0x%02x:%1d ", map_field->data.bitfield.position[i].byte, map_field->data.bitfield.position[i].bit);
				}
				for (i=0; i < (1<<map_field->data.bitfield.length); i++) {
					fwprintf(stdout, L"%ls ", map_field->data.bitfield.values[i]);
				}
				fwprintf(stdout, L"\n");
				break;

			default:
				fwprintf(stderr, L"nvram: unknown field type %d for field %ls in configuration.\n", map_field->type, map_field->name);
				exit(EXIT_FAILURE);
		}
	}
}


/* Get data from nvram. */
void command_get(int argc, char *argv[], struct list_head *mapping_list)
{
	struct list_head *pos;
	map_field_t      *map_field;
	unsigned int      argcnt=2;
	unsigned int      i;
	unsigned char     nvram_data;
	unsigned int      bitfield_data;

	/* Go through the remaining parameters. */
	while (argcnt<argc)
	{
		/* Go through all map fields. */
		list_for_each(pos, mapping_list) {
			/* Get the current field. */
			map_field=list_entry(pos, map_field_t, list);

			/* Compare the current identifier with the input string from command line. */
			if (wcsmbscmp(map_field->name, argv[argcnt]) == 0) {
				/* Found the identifier. */
				/* Switch by field type. */
				switch (map_field->type) {
					case MAP_FIELD_TYPE_BYTEARRAY:
						/* Print bytearray data. */
						for (i=0; i < map_field->data.bytearray.length; i++) {
							nvram_data=nvram_read(map_field->data.bytearray.position+i);
							if (i < map_field->data.bytearray.length-1) {
								fwprintf(stdout, L"%02x ", nvram_data);
							} else {
								fwprintf(stdout, L"%02x\n", nvram_data);
							}
						}
						break;

					case MAP_FIELD_TYPE_STRING:
						/* Print string data. */
						for (i=0; i < map_field->data.string.length; i++) {
							nvram_data=nvram_read(map_field->data.string.position+i);
							if (nvram_data != 0) {
								fwprintf(stdout, L"%c", nvram_data);
							} else break;
						}
						fwprintf(stdout, L"\n");
						break;
					case MAP_FIELD_TYPE_BITFIELD:
						/* Print bitfield data. */
						bitfield_data=0;
						for (i=0; i < map_field->data.bitfield.length; i++) {
							nvram_data=nvram_read(map_field->data.bitfield.position[i].byte);
							bitfield_data|=(nvram_data & (1<<map_field->data.bitfield.position[i].bit))?(1<<i):0;
						}
						fwprintf(stdout, L"%ls\n", map_field->data.bitfield.values[bitfield_data]);
						break;

					default:
						fwprintf(stderr, L"nvram: unknown field type %d for field %ls in configuration.\n", map_field->type, map_field->name);
						exit(EXIT_FAILURE);
				}
			}
		}

		/* Next identifier. */
		argcnt++;
	}
}


/* Set data in nvram. */
void command_set(int argc, char *argv[], struct list_head *mapping_list)
{
	struct list_head *pos;
	map_field_t      *map_field;
	unsigned int      argcnt=2;
	unsigned int      i;
	unsigned char    *nvram_bytearray;
	unsigned char     nvram_data;
	unsigned int      bitfield_data;

	/* Go through the remaining parameters. */
	while (argcnt<argc)
	{
		/* Go through all map fields. */
		list_for_each(pos, mapping_list) {
			/* Get the current field. */
			map_field=list_entry(pos, map_field_t, list);

			/* Compare the current identifier with the input string from command line. */
			if (wcsmbscmp(map_field->name, argv[argcnt]) == 0) {
				/* Found the identifier. */
				/* Switch by field type. */
				switch (map_field->type) {
					case MAP_FIELD_TYPE_BYTEARRAY:
						/* Get data from command line. */
						argcnt++;
						if (argcnt >= argc) {
							fwprintf(stderr, L"nvram: value for field %ls missing on command line.\n", map_field->name);
							exit(EXIT_FAILURE);
						}

						/* Write bytearray to NVRAM. */
						nvram_bytearray=malloc(map_field->data.bytearray.length);
						if (convert_bytearray(nvram_bytearray, argv[argcnt], map_field->data.bytearray.length) != NULL) {
							/* Ok. Now write in into nvram. */
							for (i=0; i < map_field->data.bytearray.length; i++) {
								nvram_write(map_field->data.bytearray.position+i, nvram_bytearray[i]);
							}
						} else {
							/* Not ok. */
							fwprintf(stderr, L"nvram: ignored invalid value for field %ls on command line.\n", map_field->name);
						}
						free(nvram_bytearray);
						break;

					case MAP_FIELD_TYPE_STRING:
						/* Found the identifier. Get data from command line. */
						argcnt++;
						if (argcnt >= argc) {
							fwprintf(stderr, L"nvram: value for field %ls missing on command line.\n",  map_field->name);
							exit(EXIT_FAILURE);
						}

						/* Write string to NVRAM. */
						if (strlen(argv[argcnt]) > map_field->data.string.length) {
							/* String is longer than field length. */
							fwprintf(stderr, L"nvram: string value for field %ls too long.\n",  map_field->name);
							exit(EXIT_FAILURE);
						}	else if (strlen(argv[argcnt]) == map_field->data.string.length) {
							/* String is equal the field length. */
							for (i=0; i < map_field->data.string.length; i++) {
								nvram_write(map_field->data.string.position+i, argv[argcnt][i]);
							}	
						} else {
							/* String is shorter than the field length. */
							for (i=0; i < strlen(argv[argcnt]); i++) {
								nvram_write(map_field->data.string.position+i, argv[argcnt][i]);
							}
							nvram_write(map_field->data.string.position+i, 0);
						}
						break;

					case MAP_FIELD_TYPE_BITFIELD:
						/* Found the identifier. Get data from command line. */
						argcnt++;
						if (argcnt >= argc) {
							fwprintf(stderr, L"nvram: value for field %ls missing on command line.\n",  map_field->name);
							exit(EXIT_FAILURE);
						}

						/* Get bitfield value from string value. */
						for (bitfield_data=0; bitfield_data < (1<<map_field->data.bitfield.length); bitfield_data++) {
							if (wcsmbscmp(map_field->data.bitfield.values[bitfield_data], argv[argcnt]) == 0) {
								/* Found. */
								break;
							}
						}

						/* Check if found. */
						if (bitfield_data != (1<<map_field->data.bitfield.length)) {
							/* Found. Write bitfield data to NVRAM. */
							for (i=0; i < map_field->data.bitfield.length; i++) {
								nvram_data=nvram_read(map_field->data.bitfield.position[i].byte);
								nvram_data&=~(1<<map_field->data.bitfield.position[i].bit);
								nvram_data|=(bitfield_data & (1<<i))?(1<<map_field->data.bitfield.position[i].bit):0;
								nvram_write(map_field->data.bitfield.position[i].byte, nvram_data);
							}
						} else {
							/* Not found. Ignore. */
							fwprintf(stderr, L"nvram: ignored invalid value for field %ls on command line.\n", map_field->name);
						}
						break;

					default:
						fwprintf(stderr, L"nvram: unknown field type %d for field %ls in configuration.\n", map_field->type, map_field->name);
						exit(EXIT_FAILURE);
				}
			}
		}

		/* Next identifier. */
		argcnt++;
	}
}


/* Main program. */
int main(int argc, char *argv[])
{
	LIST_HEAD(token_list);
	LIST_HEAD(nvram_mapping);
	hardware_t hardware_description;

#ifdef HAS_LOCALE
	/* Use system locale instead of "C". */
	setlocale(LC_ALL, "");
#endif

	/* Initialize hardware description. */
	hardware_description.type=HARDWARE_DESCRIPTION_STANDARD;

	/* Detect BIOS, system, and board info. */
	if (dmi_detect(&hardware_description) == -1) {
		fwprintf(stderr, L"nvram: hardware detection failed: %s.\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Read configuration file(s). */
	read_config(&token_list, &hardware_description, &nvram_mapping);
	
	/* Get mode from first parameter. */
	if (argc < 2) {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	/* Switch by command. */
	if (strcmp(argv[1], "dmi") == 0) {
		/* DMI command */
		fwprintf(stdout, L"BIOS vendor: %s\nBIOS version: %s\nBIOS release date: %s\n", hardware_description.bios_vendor, hardware_description.bios_version, hardware_description.bios_release_date);
		fwprintf(stdout, L"System manufacturer: %s\nSystem productcode: %s\nSystem version: %s\n", hardware_description.system_manufacturer, hardware_description.system_productcode, hardware_description.system_version);
		fwprintf(stdout, L"Board manufacturer: %s\nBoard productcode: %s\nBoard version: %s\n", hardware_description.board_manufacturer, hardware_description.board_productcode, hardware_description.board_version);
	}	else if (strcmp(argv[1], "list") == 0) {
		/* List command. */
		command_list(argc, argv, &nvram_mapping);
	} else if (strcmp(argv[1], "get") == 0) {
		/* Get command. */
		/* Open NVRAM. */
		if (nvram_open(hardware_description.type) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_get(argc, argv, &nvram_mapping);
	
		/* Close NVRAM. */
		nvram_close();
	}	else if (strcmp(argv[1], "set") == 0) {
		/* Set command. */
		/* Open NVRAM. */
		if (nvram_open(hardware_description.type) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_set(argc, argv, &nvram_mapping);
	
		/* Close NVRAM. */
		nvram_close();
	} else {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	return 0;
}
