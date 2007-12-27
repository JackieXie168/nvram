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
	(wchar_t *)NULL };

#define COMMAND_KEYWORD_INCLUDE   0
#define COMMAND_KEYWORD_HARDWARE  1
#define COMMAND_KEYWORD_BYTEARRAY 2
#define COMMAND_KEYWORD_STRING    3


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


#define NEXT_TOKEN \
  			pos = pos->next; \
				token=list_entry(pos, token_t, list);

#define ANY_TOKEN \
  			pos = pos->next; \
				if (pos == token_list) { \
					fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: Incomplete statement.\n", config_filename, token->line);\
					exit(EXIT_FAILURE); \
				} \
				token=list_entry(pos, token_t, list);

#define INTEGER_TOKEN \
				if (token_convert_integer(token) == -1) { \
					fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: Not a valid integer: %ls.\n", config_filename, token->line, token->data.string); \
					exit(EXIT_FAILURE); \
				}

#define EOL_TOKEN \
				if (token->type != TOKEN_TYPE_EOL) { \
					fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: Additional parameter %s in statement.\n", config_filename, token->line, token->data.string); \
				}


/* Read and tokenize config file. */
void read_config(struct list_head *token_list)
{
	struct list_head *pos;
	token_t          *token;
	char              config_filename[CONFIG_PATH_LENGTH_MAX+1];
	char              included_config_filename[CONFIG_PATH_LENGTH_MAX+1];
	FILE             *config_file;
	int               nesting_level=0;

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
		switch (token_convert_keyword(token, commands)) {
			case COMMAND_KEYWORD_INCLUDE:
				/* Check nesting level. */
				if (nesting_level > CONFIG_NESTING_MAX) {
					fwprintf(stderr, L"nvram: maximum nesting level reached in config file %s, line %d. Maybe a loop?\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Next token is a config file name. */
				ANY_TOKEN
				if (token->type != TOKEN_TYPE_STRING) {
					fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: Not a valid config file name.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Open included config file. */
				if (wcstombs(included_config_filename, token->data.string, CONFIG_PATH_LENGTH_MAX) == -1) {
					fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: Not a valid config file name.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}
				included_config_filename[CONFIG_PATH_LENGTH_MAX]='\0';
				if ((config_file=fopen(included_config_filename, "r")) == NULL) {
					fwprintf(stderr, L"nvram: include error in config file %s, line %d: %s.\n", config_filename, token->line, strerror(errno));
					exit(EXIT_FAILURE);
				}
				strncpy(config_filename, included_config_filename, CONFIG_PATH_LENGTH_MAX);

				/* Next token is the line end. */
				ANY_TOKEN

				/* Read included file and add the tokens below this include command. */
				token_tokenize_stream(config_file, token->list.next);

				/* Close the included config file. */
				fclose(config_file);

				/* Increase nesting level. */
				nesting_level++;
				break;

			case COMMAND_KEYWORD_HARDWARE:
				/* Next token is a hardware description. */
				ANY_TOKEN
				switch (token_convert_keyword(token, hardware_descriptions)) {
					case HARDWARE_DESCRIPTION_INTEL:
					case HARDWARE_DESCRIPTION_VIA82Cxx:
					case HARDWARE_DESCRIPTION_VIA823x:
					case HARDWARE_DESCRIPTION_DS1685:
						break;
					default:
						fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: Not a valid hardware description.\n", config_filename, token->line);
						exit(EXIT_FAILURE);
				}

				/* Next token is the line end. */
				ANY_TOKEN
				EOL_TOKEN
				break;

			case COMMAND_KEYWORD_BYTEARRAY:
			case COMMAND_KEYWORD_STRING:
				/* Next token is an identifier. */
				ANY_TOKEN
				if (token->type != TOKEN_TYPE_STRING) {
					fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: Not a valid identifier.\n", config_filename, token->line);
					exit(EXIT_FAILURE);
				}

				/* Next token is the integer position. */
				ANY_TOKEN
				INTEGER_TOKEN

				/* Next token is the integer length. */
				ANY_TOKEN
				INTEGER_TOKEN

				/* Next token is the line end. */
				ANY_TOKEN
				EOL_TOKEN
				break;

			default:
				fwprintf(stderr, L"nvram: syntax error in config file %s, line %d: No such keyword %ls.\n", config_filename, token->line, token->data.string);
				exit(EXIT_FAILURE);
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


/* Get hardware description from tokens. */
int get_hardware_description(struct list_head *token_list)
{
	struct list_head *pos;
	token_t          *token;

	/* Go through all tokens. */
	pos=token_list->next;
	while (pos != token_list) {
		/* Get the current token. */
		token=list_entry(pos, token_t, list);

		/* Skip EOF tokens. */
		if (token->type == TOKEN_TYPE_EOF) {
  		pos = pos->next;
			continue;
		}	

		/* This token is a command keyword. */
		switch (token->data.keyword) {
			case COMMAND_KEYWORD_HARDWARE:
				NEXT_TOKEN
				
				/* Return hardware index. */
				return (int)token->data.integer_number;

			case COMMAND_KEYWORD_BYTEARRAY:
			case COMMAND_KEYWORD_STRING:
				/* Skip identifier, position, length, and line end. */
				NEXT_TOKEN
				NEXT_TOKEN
				NEXT_TOKEN
				NEXT_TOKEN
				break;
		}

		/* Next token. */
		pos = pos->next;
	}

	/* No match. Return failure. */
	return -1;
}


/* List the available identifiers. */
void command_list(int argc, char *argv[], struct list_head *token_list)
{
	struct list_head *pos;
	token_t          *token;
	wchar_t          *identifier;
	long              position, length;

	/* List command has no further arguments. */
	if (argc != 2) {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	/* Go through all tokens. */
	pos=token_list->next;
	while (pos != token_list) {
		/* Get the current token. */
		token=list_entry(pos, token_t, list);

		/* Skip EOF tokens. */
		if (token->type == TOKEN_TYPE_EOF) {
  		pos = pos->next;
			continue;
		}

		/* This token is a command keyword. */
		switch (token->data.keyword) {
			case COMMAND_KEYWORD_INCLUDE:
				/* Skip pathname token and line end. */
				NEXT_TOKEN
				NEXT_TOKEN
				break;

			case COMMAND_KEYWORD_HARDWARE:
				/* Skip hardware type token and line end. */
				NEXT_TOKEN
				NEXT_TOKEN
				break;

			case COMMAND_KEYWORD_BYTEARRAY:
				/* Get identifier token. */
				NEXT_TOKEN
				identifier=token->data.string;

				/* Get position token. */
				NEXT_TOKEN
				position=token->data.integer_number; 

				/* Get length token. */
				NEXT_TOKEN
				length=token->data.integer_number; 
			
				/* Print bytearray. */
				fwprintf(stdout, L"bytearray %ls 0x%02x %d\n", identifier, position, length);

				/* Skip line end. */
				NEXT_TOKEN
				break;

			case COMMAND_KEYWORD_STRING:
				/* Get identifier token. */
				NEXT_TOKEN
				identifier=token->data.string;

				/* Get position token. */
				NEXT_TOKEN
				position=token->data.integer_number; 

				/* Get length token. */
				NEXT_TOKEN
				length=token->data.integer_number; 
				
				/* Print string. */
				fwprintf(stdout, L"string %ls 0x%02x %d\n", identifier, position, length);

				/* Skip line end. */
				NEXT_TOKEN
				break;
		}

		/* Next token. */
		pos = pos->next;
	}
}


/* Get data from nvram. */
void command_get(int argc, char *argv[], struct list_head *token_list)
{
	struct list_head *pos;
	token_t          *token;
	unsigned int      argcnt=2;
	wchar_t          *identifier;
	long              position, length;
	unsigned int      i;
	unsigned char     nvram_data;

	/* Go through the remaining parameters. */
	while (argcnt<argc)
	{
		/* Go through all tokens. */
		pos=token_list->next;
		while (pos != token_list) {
			/* Get the current token. */
			token=list_entry(pos, token_t, list);

			/* Skip EOF tokens. */
			if (token->type == TOKEN_TYPE_EOF) {
  			pos = pos->next;
				continue;
			}

			/* This token is a command keyword. */
			switch (token->data.keyword) {
				case COMMAND_KEYWORD_INCLUDE:
					/* Skip pathname token and line end. */
					NEXT_TOKEN
					NEXT_TOKEN
					break;

				case COMMAND_KEYWORD_HARDWARE:
					/* Skip hardware type token and line end. */
					NEXT_TOKEN
					NEXT_TOKEN
					break;

				case COMMAND_KEYWORD_BYTEARRAY:
					/* Get identifier token. */
					NEXT_TOKEN
					identifier=token->data.string;
				
					/* Get position token. */
					NEXT_TOKEN
					position=token->data.integer_number; 

					/* Get length token. */
					NEXT_TOKEN
					length=token->data.integer_number; 
	
					/* Compare the current identifier with the input string from command line. */
					if (wcsmbscmp(identifier, argv[argcnt]) == 0) {
						/* Found the identifier. Print data. */
						for (i=0;i<length;i++) {
							nvram_data=nvram_read(position+i);
							if (i < length-1) {
								fwprintf(stdout, L"%02x ",nvram_data);
							} else {
								fwprintf(stdout, L"%02x\n",nvram_data);
							}
						}
					}

					/* Skip line end. */
					NEXT_TOKEN
					break;

				case COMMAND_KEYWORD_STRING:
					/* Get identifier token. */
					NEXT_TOKEN
					identifier=token->data.string;
				
					/* Get position token. */
					NEXT_TOKEN
					position=token->data.integer_number; 

					/* Get length token. */
					NEXT_TOKEN
					length=token->data.integer_number; 
					
					/* Compare the current identifier with the input string from command line. */
					if (wcsmbscmp(identifier, argv[argcnt]) == 0) {
						/* Found the identifier. Print data. */
						for (i=0;i<length;i++) {
							nvram_data=nvram_read(position+i);
							if (nvram_data != 0) {
								fwprintf(stdout, L"%c", nvram_data);
							} else break;
						}
						fwprintf(stdout, L"\n");
					}

					/* Skip line end. */
					NEXT_TOKEN
					break;
			}

			/* Next token. */
			pos = pos->next;
		}

		/* Next identifier. */
		argcnt++;
	}
}


/* Set data in nvram. */
void command_set(int argc, char *argv[], struct list_head *token_list)
{
	struct list_head *pos;
	token_t          *token;
	unsigned int      argcnt=2;
	wchar_t          *identifier;
	long              position, length;
	unsigned int      i;
	unsigned char    *nvram_bytearray;

	/* Go through the remaining parameters. */
	while (argcnt<argc)
	{
		/* Go through all tokens. */
		pos=token_list->next;
		while (pos != token_list) {
			/* Get the current token. */
			token=list_entry(pos, token_t, list);

			/* Skip EOF tokens. */
			if (token->type == TOKEN_TYPE_EOF) {
  			pos = pos->next;
				continue;
			}

			/* This token is a command keyword. */
			switch (token->data.keyword) {
				case COMMAND_KEYWORD_INCLUDE:
					/* Skip pathname token and line end. */
					NEXT_TOKEN
					NEXT_TOKEN
					break;

				case COMMAND_KEYWORD_HARDWARE:
					/* Skip hardware type token and line end. */
					NEXT_TOKEN
					NEXT_TOKEN
					break;

				case COMMAND_KEYWORD_BYTEARRAY:
					/* Get identifier token. */
					NEXT_TOKEN
					identifier=token->data.string;
				
					/* Get position token. */
					NEXT_TOKEN
					position=token->data.integer_number; 

					/* Get length token. */
					NEXT_TOKEN
					length=token->data.integer_number; 

					/* Compare the current identifier with the input string from command line. */
					if (wcsmbscmp(identifier, argv[argcnt]) == 0) {
						/* Found the identifier. Get data from command line. */
						argcnt++;
						if (argcnt >= argc) {
							fwprintf(stderr, L"nvram: value for identifier %ls missing on command line.\n", identifier);
							exit(EXIT_FAILURE);
						}

						/* Write bytes. */
						nvram_bytearray=malloc(length);
						if (convert_bytearray(nvram_bytearray, argv[argcnt], length) != NULL) {
							/* Ok. Now write in into nvram. */
							for (i=0;i<length;i++) {
								nvram_write(position+i,nvram_bytearray[i]);
							}
						} else {
							/* Not ok. */
							fwprintf(stderr, L"nvram: ignored invalid value for identifier %ls on command line.\n", identifier);
						}
						free(nvram_bytearray);
					}

					/* Skip line end. */
					NEXT_TOKEN
					break;

				case COMMAND_KEYWORD_STRING:
					/* Get identifier token. */
					NEXT_TOKEN
					identifier=token->data.string;
				
					/* Get position token. */
					NEXT_TOKEN
					position=token->data.integer_number;

					/* Get length token. */
					NEXT_TOKEN
					length=token->data.integer_number; 
					
					/* Compare the current identifier with the input string from command line. */
					if (wcsmbscmp(identifier, argv[argcnt]) == 0) {
						/* Found the identifier. Get data from command line. */
						argcnt++;
						if (argcnt >= argc) {
							fwprintf(stderr, L"nvram: value for identifier %ls missing on command line.\n", identifier);
							exit(EXIT_FAILURE);
						}

						/* Write bytes. */
						if (strlen(argv[argcnt]) > length) {
							/* String is longer than field length. */
							fwprintf(stderr, L"nvram: string value for identifier %ls too long.\n", identifier);
							exit(EXIT_FAILURE);
						}	else if (strlen(argv[argcnt]) == length) {
							/* String is equal the field length. */
							for (i=0;i<length;i++) {
								nvram_write(position+i, argv[argcnt][i]);
							}	
						} else {
							/* String is shorter than the field length. */
							for (i=0;i<strlen(argv[argcnt]);i++) {
								nvram_write(position+i, argv[argcnt][i]);
							}
							nvram_write(position+i, 0);
						}
					}

					/* Skip line end. */
					NEXT_TOKEN
					break;
			}

			/* Next token. */
			pos = pos->next;
		}

		/* Next identifier. */
		argcnt++;
	}
}


/* Main program. */
int main(int argc, char *argv[])
{
	LIST_HEAD(token_list);

#ifdef HAS_LOCALE
	/* Use system locale instead of "C". */
	setlocale(LC_ALL, "");
#endif

	/* Read and tokenize configuration file */
	read_config(&token_list);

	/* Config file syntax is ok. */
	/* Get mode from first parameter. */
	if (argc < 2) {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	/* Check for list command. */
	if (strcmp(argv[1], "list") == 0) { command_list(argc, argv, &token_list); }
	else if (strcmp(argv[1], "get") == 0) {
		/* Open NVRAM. */
		if (nvram_open(get_hardware_description(&token_list)) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_get(argc, argv, &token_list);
	
		/* Close NVRAM. */
		nvram_close();
	}	else if (strcmp(argv[1], "set") == 0) {
		/* Open NVRAM. */
		if (nvram_open(get_hardware_description(&token_list)) == -1) {
			perror("nvram_open");
			exit(EXIT_FAILURE);
		}	

		command_set(argc, argv, &token_list);
	
		/* Close NVRAM. */
		nvram_close();
	} else {
		fwprintf(stderr, USAGE);
		exit(EXIT_FAILURE);
	}

	return 0;
}
