/*
 *   nvram -- A tool to operate on extended nvram most modern PC chipsets offer.
 *
 *   Copyleft (c) 2007, Jan Kandziora <nvram@kandziora-ing.de>
 * 
 */

#define CVS_VERSION "$Id$"

#define _GNU_SOURCE

#ifdef HAS_LOCALE
#include <locale.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
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


/* Checksum algorithm strings are defined in config.c */
extern wchar_t *checksum_algorithms[];


/* Usage message. */
const wchar_t USAGE[] = L"USAGE: nvram [OPTIONS] probe\n"
"  nvram [OPTIONS] list\n"
"  nvram [OPTIONS] get identifier [identifier]...\n"
"  nvram [OPTIONS] set identifier value [identifier value]...\n"
"OPTIONS are\n"
"  --no-checksum-update (-c) -- NVRAM checksums will not be updated automatically\n"
"  --dry-run            (-d) -- no changes are actually written to NVRAM\n"
"  --verbose            (-v) -- raise log level so informational messages are printed\n"
"  --debug                   -- raise log level so informational and debug messages are printed\n"
"  --quiet              (-q) -- lower log level so only errors are printed\n"
"  --help                    -- Show this help\n"
"  --version                 -- Show version number and exit\n";

/* Version message. */
const wchar_t VERSION[] = L"nvram 0.1\n";


/* Calculate a NVRAM checksum. */
uint16_t calculate_checksum(map_field_t *map_field)
{
	unsigned int i;
	unsigned int checksum=0;

	/* Calculate checksum. Switch by algorithm. */
	switch (map_field->data.checksum.algorithm) {
		case CHECKSUM_ALGORITHM_STANDARD_SUM:
		case CHECKSUM_ALGORITHM_STANDARD_SHORT_SUM:
			/* Standard checksum is just the arithmetical sum of the noted field. */
			for (i=0; i < map_field->data.checksum.field_length; i++) {
				checksum+=nvram_read(map_field->data.checksum.field_position+i);
			}
			break;

		case CHECKSUM_ALGORITHM_NEGATIVE_SUM:
		case CHECKSUM_ALGORITHM_NEGATIVE_SHORT_SUM:
			/* Take the arithmetical sum of the negative of the noted field. */
			for (i=0; i < map_field->data.checksum.field_length; i++) {
				checksum-=nvram_read(map_field->data.checksum.field_position+i);
			}
			break;
	}

	/* Return the calculated checksum. */
	return (checksum & ((1<<(map_field->data.checksum.size * 8))-1));
}


/* Check all checksums in NVRAM. */
void command_check(settings_t *settings, struct list_head *mapping_list)
{
	struct list_head *pos;
	map_field_t      *map_field;
	unsigned int      argcnt=1;
	unsigned int      i, found, checksum;

	/* Any parameters? */
	if (argcnt == settings->argc) {
		/* No. Check all checksums. */
		/* Go through all map fields. */
		list_for_each(pos, mapping_list) {
			/* Get the current field. */
			map_field=list_entry(pos, map_field_t, list);

			/* Ignore non-checksum fields. */
			if (map_field->type == MAP_FIELD_TYPE_CHECKSUM) {
				/* Read checksum from NVRAM. */
				checksum=0;
				for (i=map_field->data.checksum.size; i > 0; i--) {
					checksum=checksum<<8 | nvram_read(map_field->data.checksum.position[i-1]);
				}	

				fwprintf(stdout, L"%ls ", map_field->name);

				/* Calculate checksum. */
				if (calculate_checksum(map_field) == checksum) {
					fwprintf(stdout, L"OK\n");
				} else {
					fwprintf(stdout, L"FAIL (0x%08x calculated vs. 0x%08x read)\n", calculate_checksum(map_field), checksum);
				}
			}
		}
	} else {
		/* Yes. Go through the remaining parameters. */
		while (argcnt < settings->argc)
		{
			found=0;

			/* Go through all map fields. */
			list_for_each(pos, mapping_list) {
				/* Get the current field. */
				map_field=list_entry(pos, map_field_t, list);

				/* Compare the current identifier with the input string from command line. */
				if (wcsmbscmp(map_field->name, settings->argv[argcnt]) == 0) {
					/* Found the identifier. */
					found=1;

					/* Ignore non-checksum fields. */
					if (map_field->type == MAP_FIELD_TYPE_CHECKSUM) {
						/* Read checksum from NVRAM. */
						checksum=0;
						for (i=map_field->data.checksum.size; i > 0; i--) {
							checksum=checksum<<8 | nvram_read(map_field->data.checksum.position[i-1]);
						}	

						fwprintf(stdout, L"%ls ", map_field->name);

						/* Calculate checksum. */
						if (calculate_checksum(map_field) == checksum) {
							fwprintf(stdout, L"OK\n");
						} else {
							fwprintf(stdout, L"FAIL (0x%08x calculated vs. 0x%08x read)\n", calculate_checksum(map_field), checksum);
						}
					}
				}
			}

			/* Check if the identifier was found. */
			if (!found) {
				/* No. Error. */
				fwprintf(stderr, L"nvram: unknown field %s.\n", settings->argv[argcnt]);
				exit(EXIT_FAILURE);
			}

			/* Next identifier. */
			argcnt++;
		}
	}
}

/* List the available identifiers. */
void command_list(settings_t *settings, struct list_head *mapping_list)
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
			case MAP_FIELD_TYPE_CHECKSUM:
				/* Print checksum field. */
				fwprintf(stdout, L"checksum %ls %ls ",
					map_field->name,
					checksum_algorithms[map_field->data.checksum.algorithm]);

				for (i=0; i < map_field->data.checksum.size; i++) {
					fwprintf(stdout, L"0x%02x ", map_field->data.checksum.position[i]);
				}	

				fwprintf(stdout, L"0x%02x %d\n",
					map_field->data.checksum.field_position,
					map_field->data.checksum.field_length);
				break;

			case MAP_FIELD_TYPE_BYTEARRAY:
				/* Print bytearray field. */
				fwprintf(stdout, L"bytearray %ls 0x%02x %d\n",
					map_field->name,
					map_field->data.bytearray.position,
					map_field->data.bytearray.length);
				break;

			case MAP_FIELD_TYPE_STRING:
				/* Print string field. */
				fwprintf(stdout, L"string %ls 0x%02x %d\n",
					map_field->name,
					map_field->data.string.position,
					map_field->data.string.length);
				break;

			case MAP_FIELD_TYPE_BITFIELD:
				/* Print bitfield. */
				fwprintf(stdout, L"bitfield %ls %d ", map_field->name, map_field->data.bitfield.length);
				for (i=0; i < map_field->data.bitfield.length; i++) {
					fwprintf(stdout, L"0x%02x:%1d ",
						map_field->data.bitfield.position[i].byte,
						map_field->data.bitfield.position[i].bit);
				}
				for (i=0; i < (1<<map_field->data.bitfield.length); i++) {
					fwprintf(stdout, L"%ls ", map_field->data.bitfield.values[i]);
				}
				fwprintf(stdout, L"\n");
				break;

			default:
				/* Print ignored error message only if verbose. */
				if (settings->loglevel <= LOGLEVEL_INFO) {
					fwprintf(stderr, L"nvram: (ignored) unknown field type %d for field %ls in configuration.\n", map_field->type, map_field->name);
				}
		}
	}
}


/* Get data from nvram. */
void command_get(settings_t *settings, struct list_head *mapping_list)
{
	struct list_head *pos;
	map_field_t      *map_field;
	unsigned int      argcnt=1;
	unsigned int      i, found;
	unsigned char     nvram_data;
	unsigned int      bitfield_data;

	/* Go through the remaining parameters. */
	while (argcnt < settings->argc)
	{
		found=0;

		/* Go through all map fields. */
		list_for_each(pos, mapping_list) {
			/* Get the current field. */
			map_field=list_entry(pos, map_field_t, list);

			/* Compare the current identifier with the input string from command line. */
			if (wcsmbscmp(map_field->name, settings->argv[argcnt]) == 0) {
				/* Found the identifier. */
				found=1;

				/* Switch by field type. */
				switch (map_field->type) {
					case MAP_FIELD_TYPE_CHECKSUM:
						/* Print checksum data. */
						fwprintf(stdout, L"0x");
						for (i=map_field->data.checksum.size; i > 0; i--) {
							fwprintf(stdout, L"%02x",nvram_read(map_field->data.checksum.position[i-1]));
						}
						fwprintf(stdout, L"\n");
						break;

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

		/* Check if the identifier was found. */
		if (!found) {
			/* No. Error. */
			fwprintf(stderr, L"nvram: unknown field %s.\n", settings->argv[argcnt]);
			exit(EXIT_FAILURE);
		}

		/* Next identifier. */
		argcnt++;
	}
}


/* Set data in nvram. */
void command_set(settings_t *settings, struct list_head *mapping_list)
{
	struct list_head *pos;
	map_field_t      *map_field;
	unsigned int      argcnt=1;
	unsigned int      i, found, checksum;
	unsigned char    *nvram_bytearray;
	unsigned char     nvram_data;
	unsigned int      bitfield_data;

	/* Go through the remaining parameters. */
	while (argcnt < settings->argc)
	{
		found=0;

		/* Go through all map fields. */
		list_for_each(pos, mapping_list) {
			/* Get the current field. */
			map_field=list_entry(pos, map_field_t, list);

			/* Compare the current identifier with the input string from command line. */
			if (wcsmbscmp(map_field->name, settings->argv[argcnt]) == 0) {
				/* Found the identifier. */
				found=1;

				/* Switch by field type. */
				switch (map_field->type) {
					case MAP_FIELD_TYPE_CHECKSUM:
						/* Get data from command line. */
						argcnt++;
						if (argcnt >= settings->argc) {
							fwprintf(stderr, L"nvram: value for field %ls missing on command line.\n", map_field->name);
							exit(EXIT_FAILURE);
						}

						/* Print ignored error message only if verbose. */
						if (settings->loglevel <= LOGLEVEL_INFO) {
							fwprintf(stderr, L"nvram: (ignored) will no write checksum field %ls.\n", map_field->name);
						}	
						break;

					case MAP_FIELD_TYPE_BYTEARRAY:
						/* Get data from command line. */
						argcnt++;
						if (argcnt >= settings->argc) {
							fwprintf(stderr, L"nvram: value for field %ls missing on command line.\n", map_field->name);
							exit(EXIT_FAILURE);
						}

						/* Write bytearray to NVRAM. */
						nvram_bytearray=malloc(map_field->data.bytearray.length);
						if (convert_bytearray(nvram_bytearray, settings->argv[argcnt], map_field->data.bytearray.length) != NULL) {
							/* Ok. Now write in into nvram. */
							for (i=0; i < map_field->data.bytearray.length; i++) {
								nvram_write(map_field->data.bytearray.position+i, nvram_bytearray[i]);
							}
						} else {
							/* Not ok. */
							fwprintf(stderr, L"nvram: invalid value for field %ls on command line.\n", map_field->name);
							exit(EXIT_FAILURE);
						}
						free(nvram_bytearray);
						break;

					case MAP_FIELD_TYPE_STRING:
						/* Found the identifier. Get data from command line. */
						argcnt++;
						if (argcnt >= settings->argc) {
							fwprintf(stderr, L"nvram: value for field %ls missing on command line.\n",  map_field->name);
							exit(EXIT_FAILURE);
						}

						/* Write string to NVRAM. */
						if (strlen(settings->argv[argcnt]) > map_field->data.string.length) {
							/* String is longer than field length. */
							fwprintf(stderr, L"nvram: string value for field %ls too long.\n",  map_field->name);
							exit(EXIT_FAILURE);
						}	else if (strlen(settings->argv[argcnt]) == map_field->data.string.length) {
							/* String is equal the field length. */
							for (i=0; i < map_field->data.string.length; i++) {
								nvram_write(map_field->data.string.position+i, settings->argv[argcnt][i]);
							}	
						} else {
							/* String is shorter than the field length. */
							for (i=0; i < strlen(settings->argv[argcnt]); i++) {
								nvram_write(map_field->data.string.position+i, settings->argv[argcnt][i]);
							}
							nvram_write(map_field->data.string.position+i, 0);
						}
						break;

					case MAP_FIELD_TYPE_BITFIELD:
						/* Found the identifier. Get data from command line. */
						argcnt++;
						if (argcnt >= settings->argc) {
							fwprintf(stderr, L"nvram: value for field %ls missing on command line.\n",  map_field->name);
							exit(EXIT_FAILURE);
						}

						/* Get bitfield value from string value. */
						for (bitfield_data=0; bitfield_data < (1<<map_field->data.bitfield.length); bitfield_data++) {
							if (wcsmbscmp(map_field->data.bitfield.values[bitfield_data], settings->argv[argcnt]) == 0) {
								/* Found. */
								break;
							}
						}

						/* Check if found. */
						if (bitfield_data != (1<<map_field->data.bitfield.length)) {
							/* Found. Write bitfield data to NVRAM. */
							for (i=0; i < map_field->data.bitfield.length; i++) {
								/* Alter a single bit. */
								nvram_data=nvram_read(map_field->data.bitfield.position[i].byte);
								nvram_data&=~(1<<map_field->data.bitfield.position[i].bit);
								nvram_data|=(bitfield_data & (1<<i))?(1<<map_field->data.bitfield.position[i].bit):0;
								nvram_write(map_field->data.bitfield.position[i].byte, nvram_data);
							}
						} else {
							/* Not found. Ignore. */
							fwprintf(stderr, L"nvram: invalid value for field %ls on command line.\n", map_field->name);
							exit(EXIT_FAILURE);
						}
						break;

					default:
						fwprintf(stderr, L"nvram: unknown field type %d for field %ls in configuration.\n", map_field->type, map_field->name);
						exit(EXIT_FAILURE);
				}
			}
		}

		/* Check if the identifier was found. */
		if (!found) {
			/* No. Error. */
			fwprintf(stderr, L"nvram: unknown field %s.\n", settings->argv[argcnt]);
			exit(EXIT_FAILURE);
		}

		/* Next identifier. */
		argcnt++;
	}

	/* All fields altered. */
	/* Calculate and update checksums? */
	if (settings->update_checksums) {
		/* Go through all map fields. */
		list_for_each(pos, mapping_list) {
			/* Get the current field. */
			map_field=list_entry(pos, map_field_t, list);

			/* Ignore non-checksum fields. */
			if (map_field->type == MAP_FIELD_TYPE_CHECKSUM) {
				/* Calculate checksum. Write checksum to NVRAM. */
				checksum=calculate_checksum(map_field);
				for (i=0; i < map_field->data.checksum.size; i++) {
					nvram_write(map_field->data.checksum.position[i], (checksum >> (8*i)) & 0xff );
				}
			}
		}
	}

	/* Write NVRAM cache back to NVRAM if not dry-run. */
	if (settings->write_to_nvram) nvram_flush();
}


/* Main program. */
int main(int argc, char *argv[])
{
	LIST_HEAD(token_list);
	LIST_HEAD(nvram_mapping);
	hardware_t hardware_description;
	settings_t settings;
	int nvram_fd;
	int option_index;
	static struct option long_options[] = {
		{"help", 0, 0, '?'},
		{"no-checksum-update", 0, 0, 'c'},
		{"dry-run", 0, 0, 'd'},
		{"verbose", 0, 0, 'v'},
		{"quiet", 0, 0, 'q'},
		{"debug", 0, 0, 'g'},
		{"version", 0, 0, 'y'},
		{0, 0, 0, 0}
	};

	/* Setup defaults. */
	settings.write_to_nvram=1;
	settings.update_checksums=1;
	settings.loglevel=LOGLEVEL_WARNING;

	/* Lock the nvram utility against multiple invocation. */
	if ((nvram_fd=open(argv[0], O_RDONLY)) == -1) {
		perror("main, open nvram_util");
		exit(EXIT_FAILURE);
	}
	if (flock(nvram_fd, LOCK_EX) == -1) {
		perror("main, flock nvram_util");
		exit(EXIT_FAILURE);
	}

#ifdef HAS_LOCALE
	/* Use system locale instead of "C". */
	setlocale(LC_ALL, "");
#endif

	/* Parse command line options. */
	for (;;) {
		switch (getopt_long(argc, argv, "cdvq", long_options, &option_index)) {
			case 'c':
				settings.update_checksums=0;
				break;

			case 'd':
				settings.write_to_nvram=0;
				break;

			case 'v':
				settings.loglevel=LOGLEVEL_INFO;
				break;

			case 'g':
				settings.loglevel=LOGLEVEL_DEBUG;
				break;

			case 'q':
				settings.loglevel=LOGLEVEL_ERROR;
				break;

			case 'y':
				fwprintf(stderr, VERSION);
				exit(EXIT_FAILURE);

			case '?':
				fwprintf(stderr, USAGE);
				exit(EXIT_FAILURE);

			default: goto endopt;
		}
	}

endopt:
	/* Put remaining arguments into settings. */
	settings.argc=argc-optind;
	settings.argv=&argv[optind];

	/* Change directory to nvram configuration. */
	/* Silently ignore if failing. */
	chdir(CONFIG_DIRECTORY);

	/* Initialize hardware description. */
	hardware_description.type=HARDWARE_TYPE_STANDARD;

	/* Detect BIOS, system, and board info. */
	if (dmi_detect(&hardware_description) == -1) {
		fwprintf(stderr, L"nvram: hardware detection failed: %s.\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Read configuration file(s). */
	read_config(&settings, &token_list, &hardware_description, &nvram_mapping);
	
	/* Get mode from first parameter. */
	if (settings.argc < 1) {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	/* Switch by command. */
	if (strcmp(settings.argv[0], "probe") == 0) {
		/* DMI command */
		fwprintf(stdout, L"BIOS vendor: '%s'\nBIOS version: '%s'\nBIOS release date: '%s'\n", hardware_description.bios_vendor, hardware_description.bios_version, hardware_description.bios_release_date);
		fwprintf(stdout, L"System manufacturer: '%s'\nSystem productcode: '%s'\nSystem version: '%s'\n", hardware_description.system_manufacturer, hardware_description.system_productcode, hardware_description.system_version);
		fwprintf(stdout, L"Board manufacturer: '%s'\nBoard productcode: '%s'\nBoard version: '%s'\n", hardware_description.board_manufacturer, hardware_description.board_productcode, hardware_description.board_version);

	}	else if (strcmp(settings.argv[0], "check") == 0) {
		/* Check command. */
		/* Open NVRAM. */
		if (nvram_open(hardware_description.type) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_check(&settings, &nvram_mapping);

		/* Close NVRAM. */
		nvram_close();

	}	else if (strcmp(settings.argv[0], "list") == 0) {
		/* List command. */
		if (settings.argc != 1) {
			fwprintf(stderr, USAGE);
			exit(EXIT_FAILURE);
		}

		command_list(&settings, &nvram_mapping);

	} else if (strcmp(settings.argv[0], "get") == 0) {
		/* Get command. */
		if (settings.argc < 2) {
			fwprintf(stderr, USAGE);
			exit(EXIT_FAILURE);
		}

		/* Open NVRAM. */
		if (nvram_open(hardware_description.type) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_get(&settings, &nvram_mapping);
	
		/* Close NVRAM. */
		nvram_close();

	}	else if (strcmp(settings.argv[0], "set") == 0) {
		/* Set command. */
		if (settings.argc < 3) {
			fwprintf(stderr, USAGE);
			exit(EXIT_FAILURE);
		}

		/* Open NVRAM. */
		if (nvram_open(hardware_description.type) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_set(&settings, &nvram_mapping);
	
		/* Close NVRAM. */
		nvram_close();
	} else {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	return 0;
}
