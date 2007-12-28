/*
 *   config.c -- Read and interprete configuration files.
 *
 *   Copyleft (c) 2007, Jan Kandziora <nvram@kandziora-ing.de>
 * 
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "config.h"
#include "detect.h"
#include "list.h"
#include "map.h"
#include "token.h"
#include "util.h"


/* Recognized hardware descriptions. */
wchar_t *hardware_descriptions[] = {
	L"standard",
	L"intel",
	L"via82cxx",
	L"via823x",
	L"ds1685",
	(wchar_t *)NULL };

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
					case HARDWARE_DESCRIPTION_STANDARD:
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
