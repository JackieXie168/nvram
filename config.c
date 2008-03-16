/*
 *   config.c -- Read and interprete configuration files.
 *
 *   Copyright (c) 2007, Jan Kandziora <nvram@kandziora-ing.de>
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
wchar_t *hardware_types[] = {
	L"standard",
	L"intel",
	L"via82cxx",
	L"via823x",
	L"ds1685",
	(wchar_t *)NULL };

/* Recognized checksum algorithms. */
wchar_t *checksum_algorithms[] = {
	L"standard",
	L"short",
	L"negative_sum",
	L"negative_short",
	(wchar_t *)NULL };

/* Recognized loglevels. */
wchar_t *loglevels[] = {
	L"debug",
	L"info",
	L"warning",
	L"error",
	(wchar_t *)NULL };

/* Recognized commands. */
wchar_t *commands[] = {
	L"{",
	L"}",
	L"break",
	L"continue",
	L"or",
	L"and",
	L"fail",
	L"log",
	L"include",
	L"hardware",
	L"checksum",
	L"bytearray",
	L"string",
	L"bitfield",
	(wchar_t *)NULL };

#define COMMAND_KEYWORD_BLOCK_START  0
#define COMMAND_KEYWORD_BLOCK_END    1
#define COMMAND_KEYWORD_BREAK        2
#define COMMAND_KEYWORD_CONTINUE     3
#define COMMAND_KEYWORD_OR           4
#define COMMAND_KEYWORD_AND          5
#define COMMAND_KEYWORD_FAIL         6
#define COMMAND_KEYWORD_LOG          7
#define COMMAND_KEYWORD_INCLUDE      8
#define COMMAND_KEYWORD_HARDWARE     9
#define COMMAND_KEYWORD_CHECKSUM    10
#define COMMAND_KEYWORD_BYTEARRAY   11
#define COMMAND_KEYWORD_STRING      12
#define COMMAND_KEYWORD_BITFIELD    13


#define NEXT_TOKEN \
 	pos = pos->next; \
	if (pos == token_list) { \
		fwprintf(stderr, L"nvram: error in config file %s, line %d: incomplete statement.\n", config_filename, token->line); \
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

#define REPLACE_ESCAPE(s) \
	for (k=0; (k < strlen(s) && (j < CONFIG_PATH_LENGTH_MAX)); k++, j++) { \
		if (mbtowc(&filename_buffer[j], &(s[k]), MB_CUR_MAX) == -1) { \
			fwprintf(stderr, L"nvram: error in config file %s, line %d: invalid character sequence in config file name.\n", config_filename, token->line); \
			exit(EXIT_FAILURE); \
		} \
	} \
	j--;

#define INVALID_ESCAPE \
	fwprintf(stderr, L"nvram: error in config file %s, line %d: invalid escape sequence in config file name.\n", config_filename, token->line); \
	exit(EXIT_FAILURE);

#define STATUS_FAILED  status=0;
#define STATUS_SUCCESS status=1;

#define LOG(level) if (settings->loglevel <= level)

/* Read config file. */
void read_config(settings_t *settings, struct list_head *token_list, hardware_t *hardware_description, struct list_head *nvram_mapping)
{
	FILE             *config_file;
	char              config_filename[CONFIG_PATH_LENGTH_MAX+1];
	wchar_t          *identifier;
	char              included_config_filename[CONFIG_PATH_LENGTH_MAX+1];
	wchar_t           filename_buffer[CONFIG_PATH_LENGTH_MAX+1];
	map_field_t      *map_field;
	int               block_level, block_nesting_level=0, include_nesting_level=0;
	struct list_head *pos, *map_pos;
	long              position, length, checksum_algorithm, checksum_position_count, loglevel;
	long              i, j, k;
	long              checksum_position[MAP_CHECKSUM_MAX_POSITIONS];
	token_t          *token;
	int               command_keyword;
	char              status=0, status_helper;

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
			include_nesting_level--;

			/* Next token. */
  		pos = pos->next;
			continue;
		}	

		/* This token must be a command keyword. */
		switch (command_keyword=token_convert_keyword(token, commands)) {
			case COMMAND_KEYWORD_BLOCK_START:
				/* Increase block nesting level. */
				block_nesting_level++;

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN

				/* Command succeded. */
				STATUS_SUCCESS
				break;

			case COMMAND_KEYWORD_BLOCK_END:
				/* Decrease block nesting level. */
				block_nesting_level--;

				/* Fail if more blocks were closed than opened. */
				if (block_nesting_level < 0) {
					fwprintf(stderr, L"nvram: error in config file %s, line %d: unbalanced }.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN
				break;

			/* Both break and continue leave the current block. */
			case COMMAND_KEYWORD_BREAK:
			case COMMAND_KEYWORD_CONTINUE:
				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN
			
				/* Decrease block nesting level. */
				block_nesting_level--;

				/* Complain if we are not in a block. */
				if (block_nesting_level < 0) {
					fwprintf(stderr, L"nvram: error in config file %s, line %d: break outside a {...} block.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Skip all following tokens until block end of the same level. */
				block_level=1;
				do {
					NEXT_TOKEN
					if (token->type == TOKEN_TYPE_STRING) {
						switch (token_convert_keyword(token, commands)) {
							case COMMAND_KEYWORD_BLOCK_START:
								block_level++;
								break;

							case COMMAND_KEYWORD_BLOCK_END:
								block_level--;
								break;
						}
					}	
				} while (block_level);

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN

				/* Break or continue? */
				switch (command_keyword) {
					case COMMAND_KEYWORD_BREAK:
						/* Break command succeeded, but this implies the block as a whole failed. */
						STATUS_FAILED
						break;

					case COMMAND_KEYWORD_CONTINUE:
						/* Continue command succeeded, block as a whole succeded. */
						STATUS_SUCCESS
						break;
				}	
				break;

			/* Both "or" and "and" test the status, then skip. */
			case COMMAND_KEYWORD_OR:
			case COMMAND_KEYWORD_AND:
				switch (command_keyword) {
					case COMMAND_KEYWORD_OR:
						status_helper=status;
						break;

					case COMMAND_KEYWORD_AND:
						status_helper=!status;
						break;
				}	

				/* Check if the previous include directive succeded. */
				if (status_helper) {
					/* Yes. Get the next token. This is a command. */
					NEXT_TOKEN
					NOT_EOL_TOKEN

					/* Check if next command is a block start. */
					if (token_convert_keyword(token, commands) != COMMAND_KEYWORD_BLOCK_START) {
						/* No. Skip all following tokens until line end */
						do {
							NEXT_TOKEN
						} while (token->type != TOKEN_TYPE_EOL);

						/* Next token is the next command. */
					} else {
						/* Yes. Skip all following tokens until block end of the same level. */
						block_level=1;
						do {
							NEXT_TOKEN
							if (token->type == TOKEN_TYPE_STRING) {
								switch (token_convert_keyword(token, commands)) {
									case COMMAND_KEYWORD_BLOCK_START:
										block_level++;
										break;

									case COMMAND_KEYWORD_BLOCK_END:
										block_level--;
										break;
								}
							}	
						} while (block_level);

						/* Next token is the line end. */
						NEXT_TOKEN
						EOL_TOKEN
					}
				}	
				break;

			case COMMAND_KEYWORD_FAIL:
				/* Fail immediately. */
				fwprintf(stderr, L"nvram: failed in config file %s, line %d.\n", config_filename, token->line);
				exit(EXIT_FAILURE);

			case COMMAND_KEYWORD_LOG:
				/* Next token is a loglevel. */
				NEXT_TOKEN
				switch (token_convert_keyword(token, loglevels)) {
					case LOGLEVEL_DEBUG:
					case LOGLEVEL_INFO:
					case LOGLEVEL_WARNING:
					case LOGLEVEL_ERROR:
						loglevel=token->data.integer_number;
						break;

					default:
						fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid loglevel.\n", config_filename, token->line);
						exit(EXIT_FAILURE);
				}

				/* All following tokens form of a message. */
				NEXT_TOKEN
				LOG(loglevel) fwprintf(stderr, L"nvram:");
				while (token->type != TOKEN_TYPE_EOL) {
					LOG(loglevel) fwprintf(stderr, L" %ls", token->data.string);
					NEXT_TOKEN
				}
				LOG(loglevel) {
					fwprintf(stderr, L"\n");

					/* Command succeded. */
					STATUS_SUCCESS
				}	else {
					/* Command failed. */
					STATUS_FAILED
				}
				break;

			case COMMAND_KEYWORD_INCLUDE:
				/* Check nesting level. */
				if (include_nesting_level > CONFIG_NESTING_MAX) {
					fwprintf(stderr, L"nvram: maximum nesting level reached in config file %s, line %d. Maybe a loop?\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Next token is a config file name. */
				NEXT_TOKEN
				if (token->type != TOKEN_TYPE_STRING) {
					fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid config file name.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Replace special sequences by DMI strings. */
				for (i=0, j=0; (i <= wcslen(token->data.string)) && (j < CONFIG_PATH_LENGTH_MAX); i++, j++) {
					/* Check for escape char. */
					if (token->data.string[i] == L'%') {
						/* Escape char found. Get next char. */
						i++;
						switch (token->data.string[i]) {
							case L'b':
								i++;
								switch (token->data.string[i]) {
									case L'm':
										/* Replace with BIOS vendor. */
										REPLACE_ESCAPE(hardware_description->bios_vendor)
										break;

									case L'v':
										/* Replace with BIOS version. */
										REPLACE_ESCAPE(hardware_description->bios_version)
										break;

									case L'r':
										/* Replace with BIOS release date. */
										REPLACE_ESCAPE(hardware_description->bios_release_date)
										break;
									default:
										INVALID_ESCAPE
								}
								break;

							case L's':
								i++;
								switch (token->data.string[i]) {
									case L'm':
										/* Replace with system manufacturer. */
										REPLACE_ESCAPE(hardware_description->system_manufacturer)
										break;

									case L'p':
										/* Replace with system product code. */
										REPLACE_ESCAPE(hardware_description->system_productcode)
										break;

									case L'v':
										/* Replace with system version. */
										REPLACE_ESCAPE(hardware_description->system_version)
										break;
									default:
										INVALID_ESCAPE
								}
								break;

							case L'm':
								i++;
								switch (token->data.string[i]) {
									case L'm':
										/* Replace with board manufacturer. */
										REPLACE_ESCAPE(hardware_description->board_manufacturer)
										break;

									case L'p':
										/* Replace with board product code. */
										REPLACE_ESCAPE(hardware_description->board_productcode)
										break;

									case L'v':
										/* Replace with board version. */
										REPLACE_ESCAPE(hardware_description->board_version)
										break;
									default:
										INVALID_ESCAPE
								}
								break;

							default:
								INVALID_ESCAPE
						}
					} else {
						/* Single char. */
						filename_buffer[j]=token->data.string[i];
					}	
				}

				/* Open included config file. */
				if (wcstombs(included_config_filename, filename_buffer, CONFIG_PATH_LENGTH_MAX) == -1) {
					fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid config file name.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}
				included_config_filename[CONFIG_PATH_LENGTH_MAX]='\0';

				if ((config_file=fopen(included_config_filename, "r")) == NULL) {
					/* Print ignored error message only if verbose. */
					LOG(LOGLEVEL_INFO) {
						fwprintf(stderr, L"nvram: (ignored) error opening include file %ls noted in config file %s, line %d: %s.\n", filename_buffer, config_filename, token->line, strerror(errno));
					}	

					/* Next token is the line end. */
					NEXT_TOKEN
					EOL_TOKEN

					/* Command failed. */
					STATUS_FAILED

					/* Break switch. */
					break;
				}
				strncpy(config_filename, included_config_filename, CONFIG_PATH_LENGTH_MAX);

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN

				/* Read included file and add the tokens below this include command. */
				token_tokenize_stream(config_file, token->list.next);

				/* Close the included config file. */
				fclose(config_file);

				/* Increase nesting level. */
				include_nesting_level++;

				/* Command succeded. */
				STATUS_SUCCESS
				break;

			case COMMAND_KEYWORD_HARDWARE:
				/* Next token is a hardware description. */
				NEXT_TOKEN
				switch (token_convert_keyword(token, hardware_types)) {
					case HARDWARE_TYPE_STANDARD:
					case HARDWARE_TYPE_INTEL:
					case HARDWARE_TYPE_VIA82Cxx:
					case HARDWARE_TYPE_VIA823x:
					case HARDWARE_TYPE_DS1685:
						hardware_description->type=token->data.integer_number;
						break;

					default:
						fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid hardware description.\n", config_filename, token->line);
						exit(EXIT_FAILURE);
				}

				/* Next token is the line end. */
				NEXT_TOKEN
				EOL_TOKEN

				/* Command succeded. */
				STATUS_SUCCESS
				break;

			/* All field commands start the same way. */
			case COMMAND_KEYWORD_CHECKSUM:
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

				/* Distinguish between the field types. */
				switch (command_keyword) {
					case COMMAND_KEYWORD_CHECKSUM:
						/* Next token is a checksum algorithm description. */
						NEXT_TOKEN
						switch (token_convert_keyword(token, checksum_algorithms)) {
							case CHECKSUM_ALGORITHM_STANDARD_SUM:
							case CHECKSUM_ALGORITHM_STANDARD_SHORT_SUM:
							case CHECKSUM_ALGORITHM_NEGATIVE_SUM:
							case CHECKSUM_ALGORITHM_NEGATIVE_SHORT_SUM:
								checksum_algorithm=token->data.integer_number;
								break;

							default:
								fwprintf(stderr, L"nvram: error in config file %s, line %d: not a valid checksum algorithm.\n", config_filename, token->line);
								exit(EXIT_FAILURE);
						}

						/* Read checksum positions. */
						/* Switch by checksum algorithm. */
						checksum_position_count=1;
						switch (checksum_algorithm) {
							case CHECKSUM_ALGORITHM_STANDARD_SUM: 
							case CHECKSUM_ALGORITHM_NEGATIVE_SUM:
								checksum_position_count++;
						}

						for (i=0; i < checksum_position_count; i++) \
						{
							/* Next token is a position of a part of the checksum. */
							NEXT_TOKEN
							INTEGER_TOKEN
							checksum_position[i]=token->data.integer_number;
						}

						/* Next token is the integer position of the area to checksum. */
						NEXT_TOKEN
						INTEGER_TOKEN
						position=token->data.integer_number;

						/* Next token is the integer length of the area to checksum. */
						NEXT_TOKEN
						INTEGER_TOKEN
						length=token->data.integer_number;

						/* Create new mapping entry for the token. */
						if ((map_field=map_field_new()) == NULL) {
							perror("read_config, map_field_new");
							exit(EXIT_FAILURE);
						}
						map_field->type=MAP_FIELD_TYPE_CHECKSUM;
						map_field->name=identifier;
						map_field->data.checksum.algorithm=checksum_algorithm;
						map_field->data.checksum.size=checksum_position_count;
						for (i=0; i < checksum_position_count; i++) map_field->data.checksum.position[i]=checksum_position[i];
						map_field->data.checksum.field_position=position;
						map_field->data.checksum.field_length=length;

						/* Add it up to the NVRAM mapping list. */
						list_add_tail(&map_field->list, nvram_mapping);

						/* Next token is the line end. */
						NEXT_TOKEN
						EOL_TOKEN

						/* Command succeded. */
						STATUS_SUCCESS
						break;

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

						/* Command succeded. */
						STATUS_SUCCESS
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

						/* Command succeded. */
						STATUS_SUCCESS
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

						/* Command succeded. */
						STATUS_SUCCESS
						break;
				}
				break;

			default:
				fwprintf(stderr, L"nvram: error in config file %s, line %d: no such keyword %ls.\n", config_filename, token->line, token->data.string);
				exit(EXIT_FAILURE);
		}

		/* Next token. */
  	pos = pos->next;
	}

	/* Fail if more blocks were opened than closed. */
	if (block_nesting_level > 0) {
		fwprintf(stderr, L"nvram: error in config file(s): unbalanced {.\n", config_filename, token->line);
		exit(EXIT_FAILURE);
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
