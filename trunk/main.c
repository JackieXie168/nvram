/*
 *   nvram -- A tool to operate on extended nvram most modern PC chipsets offer.
 *   Copyright (C) 2007 Jan Kandziora <jjj@gmx.de>
 */

#ifdef HAS_LOCALE
#include <locale.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "list.h"
#include "token.h"
#include "map.h"
#include "nvram_op.h"
#include "util.h"


/* Usage message. */
const wchar_t USAGE[] = L"USAGE: nvram list\n       nvram get identifier [identifier]...\n       nvram set identifier value [identifier value]...\n";


/* Config files. */
#define CONFIG_BASE_FILENAME "./nvram.conf"
#define CONFIG_PATH_LENGTH_MAX 1000
#define CONFIG_NESTING_MAX 100

/* Recognized commands. */
wchar_t *commands[] = {
	L"include",
	L"hardware",
	L"bytearray",
	L"string",
	L"bitfield",
	(wchar_t *)NULL };

#define COMMAND_KEYWORD_INCLUDE   0
#define COMMAND_KEYWORD_HARDWARE  1
#define COMMAND_KEYWORD_BYTEARRAY 2
#define COMMAND_KEYWORD_STRING    3
#define COMMAND_KEYWORD_BITFIELD  4


/* Recognized hardware_descriptions. */
wchar_t *hardware_descriptions[] = {
	L"intel",
	L"via82cxx",
	L"via823x",
	L"ds1685",
	(wchar_t *)NULL };

#define HARDWARE_DESCRIPTION_INTEL    0
#define HARDWARE_DESCRIPTION_VIA82Cxx 1
#define HARDWARE_DESCRIPTION_VIA823x  2
#define HARDWARE_DESCRIPTION_DS1685   3


typedef struct {
	int type;
} hardware_t;


#define NEXT_TOKEN \
  			pos = pos->next; \
				if (pos == token_list) { \
					fwprintf(stderr, L"nvram: error in config file %s, line %d: incomplete statement.\n", config_filename, token->line);\
					exit(EXIT_FAILURE); \
				} \
				token=list_entry(pos, token_t, list);

#define INTEGER_TOKEN \
				if (token_convert_integer(token) == -1) { \
					fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid integer: %ls.\n", config_filename, token->line, token->data.string); \
					exit(EXIT_FAILURE); \
				}

#define EOL_TOKEN \
				if (token->type != TOKEN_TYPE_EOL) { \
					fwprintf(stderr, L"nvram: error in config file %s, line %d: additional parameter %s in statement.\n", config_filename, token->line, token->data.string); \
				}

#define NOT_EOL_TOKEN \
				if (token->type == TOKEN_TYPE_EOL) { \
					fwprintf(stderr, L"nvram: error in config file %s, line %d: incomplete statement.\n", config_filename, token->line);\
					exit(EXIT_FAILURE); \
				}

/* Read config file. */
void read_config(struct list_head *token_list, hardware_t *hardware_description, struct list_head *nvram_mapping)
{
	FILE             *config_file;
	char              config_filename[CONFIG_PATH_LENGTH_MAX+1];
	wchar_t          *identifier;
	char              included_config_filename[CONFIG_PATH_LENGTH_MAX+1];
	map_field_t      *map_field;
	int               nesting_level=0;
	struct list_head *pos, *map_pos;
	long              position, length, i;
	token_t          *token;
	int               command_keyword;

	/* Read and tokenize the main config file. */
	strcpy(config_filename, CONFIG_BASE_FILENAME);
	if ((config_file=fopen(config_filename, "r")) == NULL) {
		fwprintf(stderr, L"nvram: error loading main config file %s: %s.\n", config_filename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	token_tokenize_stream(config_file, token_list);
	fclose(config_file);

	/* Check syntax of tokenized configuration and convert keywords and integers. */
	pos=token_list->next;
	while (pos != token_list) {
		/* Get the current token. */
		token=list_entry(pos, token_t, list);

		/* Check for EOF (e.g. from an included file) */
		if (token->type == TOKEN_TYPE_EOF) {
			/* Decrease nesting level. */
			nesting_level--;

			/* Next token. */
  		pos = pos->next;
			continue;
		}	

		/* This token must be a command keyword. */
		command_keyword=token_convert_keyword(token, commands);
		switch (command_keyword) {
			case COMMAND_KEYWORD_INCLUDE:
				/* Check nesting level. */
				if (nesting_level > CONFIG_NESTING_MAX) {
					fwprintf(stderr, L"nvram: maximum nesting level reached in config file %s, line %d. Maybe a loop?\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Next token is a config file name. */
				NEXT_TOKEN
				if (token->type != TOKEN_TYPE_STRING) {
					fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid config file name.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Open included config file. */
				if (wcstombs(included_config_filename, token->data.string, CONFIG_PATH_LENGTH_MAX) == -1) {
					fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid config file name.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}
				included_config_filename[CONFIG_PATH_LENGTH_MAX]='\0';
				if ((config_file=fopen(included_config_filename, "r")) == NULL) {
					fwprintf(stderr, L"nvram: include error in config file %s, line %d: %s.\n", config_filename, token->line, strerror(errno));
					exit(EXIT_FAILURE);
				}
				strncpy(config_filename, included_config_filename, CONFIG_PATH_LENGTH_MAX);

				/* Next token is the line end. */
				NEXT_TOKEN

				/* Read included file and add the tokens below this include command. */
				token_tokenize_stream(config_file, token->list.next);

				/* Close the included config file. */
				fclose(config_file);

				/* Increase nesting level. */
				nesting_level++;
				break;

			case COMMAND_KEYWORD_HARDWARE:
				/* Next token is a hardware description. */
				NEXT_TOKEN
				switch (token_convert_keyword(token, hardware_descriptions)) {
					case HARDWARE_DESCRIPTION_INTEL:
					case HARDWARE_DESCRIPTION_VIA82Cxx:
					case HARDWARE_DESCRIPTION_VIA823x:
					case HARDWARE_DESCRIPTION_DS1685:
						hardware_description->type=token->data.integer_number;
						break;
					default:
						fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid hardware description.\n", config_filename, token->line);
						exit(EXIT_FAILURE);
				}

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN
				break;

			/* All field commands start the same way. */
			case COMMAND_KEYWORD_BYTEARRAY:
			case COMMAND_KEYWORD_STRING:
			case COMMAND_KEYWORD_BITFIELD:
				/* Next token is an identifier. */
				NEXT_TOKEN
				NOT_EOL_TOKEN
				identifier=token->data.string;

				/* Check if this identifier already exists. */
				list_for_each(map_pos, nvram_mapping) {
					map_field=list_entry(map_pos, map_field_t, list);
					if (wcscmp(map_field->name, identifier) == 0) {
						fwprintf(stderr, L"nvram: error in config file %s, line %d: identifier %ls already used.\n", config_filename, token->line, identifier);
						exit(EXIT_FAILURE);
					}
				}

				/* Next switch distiguishes between the various field types. */
				break;

			default:
				fwprintf(stderr, L"nvram: error in config file %s, line %d: no such keyword %ls.\n", config_filename, token->line, token->data.string);
				exit(EXIT_FAILURE);
		}

		/* Distinguish between the field types. */
		switch (command_keyword) {
			case COMMAND_KEYWORD_BYTEARRAY:
				/* Next token is the integer position. */
				NEXT_TOKEN
				INTEGER_TOKEN
				position=token->data.integer_number;

				/* Next token is the integer length. */
				NEXT_TOKEN
				INTEGER_TOKEN
				length=token->data.integer_number;

				/* Create new mapping entry for the token. */
				if ((map_field=map_field_new()) == NULL) {
					perror("read_config, map_field_new");
					exit(EXIT_FAILURE);
				}
				map_field->type=MAP_FIELD_TYPE_BYTEARRAY;
				map_field->name=identifier;
				map_field->data.bytearray.position=position;
				map_field->data.bytearray.length=length;

				/* Add it up to the NVRAM mapping list. */
				list_add_tail(&map_field->list, nvram_mapping);

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN
				break;

			case COMMAND_KEYWORD_STRING:
				/* Next token is the integer position. */
				NEXT_TOKEN
				INTEGER_TOKEN
				position=token->data.integer_number;

				/* Next token is the integer length. */
				NEXT_TOKEN
				INTEGER_TOKEN
				length=token->data.integer_number;

				/* Create new mapping entry for the token. */
				if ((map_field=map_field_new()) == NULL) {
					perror("read_config, map_field_new");
					exit(EXIT_FAILURE);
				}
				map_field->type=MAP_FIELD_TYPE_STRING;
				map_field->name=identifier;
				map_field->data.string.position=position;
				map_field->data.string.length=length;

				/* Add it up to the NVRAM mapping list. */
				list_add_tail(&map_field->list, nvram_mapping);

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN
				break;

			case COMMAND_KEYWORD_BITFIELD:
				/* Next token is the bit count. */
				NEXT_TOKEN
				INTEGER_TOKEN
				length=token->data.integer_number;

				/* Check the bit count is at most the maximum bit count. */
				if ((length < 1) || (length > MAP_BITFIELD_MAX_BITS)) {
					fwprintf(stderr, L"nvram: error in config file %s, line %d: number of bits in a bitfield has to be between 1 and %d.\n", config_filename, token->line, MAP_BITFIELD_MAX_BITS);
					exit(EXIT_FAILURE);
				}

				/* Create new mapping entry for the token. */
				if ((map_field=map_field_new()) == NULL) {
					perror("read_config, map_field_new");
					exit(EXIT_FAILURE);
				}
				map_field->type=MAP_FIELD_TYPE_BITFIELD;
				map_field->name=identifier;
				map_field->data.bitfield.length=length;

				/* Next tokens have to be integer pairs. */
				for (i=0; i < length; i++) {
					NEXT_TOKEN
					if (token_convert_integer_pair(token) == -1) {
						fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid integer pair: %ls.\n", config_filename, token->line, token->data.string);
						exit(EXIT_FAILURE);
					}
					if ((token->data.integer_pair.second < 0) || (token->data.integer_pair.second > 7)) {
						fwprintf(stderr, L"nvram: error in config file %s, line %d: bit number must be between 0 and 7.\n", config_filename, token->line);
						exit(EXIT_FAILURE);
					}

					/* Set bit position. */
					map_field->data.bitfield.position[i].byte=token->data.integer_pair.first;
					map_field->data.bitfield.position[i].bit=token->data.integer_pair.second;
				}

				/* Next tokens are arbitrary strings. */
				for (i=0; i < (1<<length); i++) {
					NEXT_TOKEN
					NOT_EOL_TOKEN

					/* Set value. */
					map_field->data.bitfield.values[i]=token->data.string;
				}

				/* Add it up to the NVRAM mapping list. */
				list_add_tail(&map_field->list, nvram_mapping);

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN
				break;
		}

		/* Next token. */
  	pos = pos->next;
	}

#ifdef DEBUG
	list_for_each(pos, token_list) {
		token=list_entry(pos, token_t, list);
		switch (token->type) {
			case TOKEN_TYPE_KEYWORD:
				fwprintf(stderr, L"K:%d ", token->data.keyword);
				break;
			case TOKEN_TYPE_INTEGER:
				fwprintf(stderr, L"I:%d ", token->data.integer_number);
				break;
			case TOKEN_TYPE_INTEGER_PAIR:
				fwprintf(stderr, L"IP:%d/%d ", token->data.integer_pair.first, token->data.integer_pair.second);
				break;
			case TOKEN_TYPE_STRING:
				fwprintf(stderr, L"S:%ls ", token->data.string);
				break;
			case TOKEN_TYPE_EOL:
				fwprintf(stderr, L"\n");
				break;
			case TOKEN_TYPE_EOF:
				fwprintf(stderr, L"---EOF---\n");
				break;
			default:
				fwprintf(stderr, L"??? ");
		}		
	}
#endif

}


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

						/* Write bytes. */
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

						/* Write bytes. */
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

	/* Read configuration file(s). */
	read_config(&token_list, &hardware_description, &nvram_mapping);

	/* Get mode from first parameter. */
	if (argc < 2) {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	/* Check for list command. */
	if (strcmp(argv[1], "list") == 0) { command_list(argc, argv, &nvram_mapping); }
	else if (strcmp(argv[1], "get") == 0) {
		/* Open NVRAM. */
		if (nvram_open(hardware_description.type) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_get(argc, argv, &nvram_mapping);
	
		/* Close NVRAM. */
		nvram_close();
	}	else if (strcmp(argv[1], "set") == 0) {
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
