/*
 *   token.c -- simple config tokenizer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "list.h"
#include "token.h"


/* Create a new NULL token. */
token_t *token_new(void)
{
	token_t *token;

	/* Get memory for token. */
	if ((token=malloc(sizeof(token_t))) == NULL) return NULL;

	/* Initialize token. */
	INIT_LIST_HEAD(&token->list);
	token->type=TOKEN_TYPE_NULL;

	return token;
}


/* Destroy (unlink & free) a single token. */
void token_destroy(token_t *ptr)
{
	list_del(&ptr->list);
	free(ptr);
}


/* Destroy (free all) a list of tokens. */
void token_destroy_list(token_t *ptr)
{
	struct list_head *pos, *temp;

	list_for_each_safe(pos, temp, &(ptr->list)) { 
		free(list_entry(pos, token_t, list));
	}	
}




/* Reads a stream and parse it into tokens. */
void token_tokenize_stream(FILE *stream, struct list_head *token_list)
{
	wchar_t           current_char;
	wchar_t          *buffer;
	unsigned int      buffer_size, buffer_current, line=1;
	token_t          *token;
	unsigned char     intermittend_eof=0;

	/* Allocate a buffer. */
	buffer_size=TOKEN_STREAM_BUFFER_SIZE_START * sizeof(wchar_t);
	buffer_current=0;
	if ((buffer=malloc(buffer_size)) == NULL) {
		perror("token_tokenize_stream, malloc");
		exit(EXIT_FAILURE);
	}
	buffer[0]=L'\0';

	/* Read from stream. */
	while (!intermittend_eof) {
		/* Get first char of line. */
		current_char=fgetwc(stream);
		if (current_char == WEOF) goto end_of_file;

		/* Eat up whitespace at line start (and intermittend newlines). */
		while (iswspace(current_char)) {
			if (current_char == L'\n') line++;
			current_char=fgetwc(stream);
			if (current_char == WEOF) goto end_of_file;
		};

		/* Ignore lines starting with a #. */
		if (current_char == L'#') {
			while (current_char != L'\n') {
				current_char=fgetwc(stream);
				if (current_char == WEOF) goto end_of_file;
			};

			/* Current char is a newline. Start next line with next char. */
			line++;
			continue;
		}

		/* Current char is the first non-whitespace, non-eof character outside a comment line. */
		while (!intermittend_eof && (current_char != L'\n')) {
			/* Read all non-whitespace into buffer. */
			while (!iswspace(current_char)) {
				/* Increase the buffer size if neccessary. */
				if (buffer_current == buffer_size-1) {
					buffer_size+=TOKEN_STREAM_BUFFER_SIZE_INCREMENT * sizeof(wchar_t);
					if ((buffer=realloc(buffer, buffer_size)) == NULL) {
						perror("token_tokenize_stream, realloc");
						exit(EXIT_FAILURE);
					}
				}

				/* Put the character into the buffer, end the string. */
				buffer[buffer_current]=current_char;
				buffer_current++;
				buffer[buffer_current]=L'\0';

				/* Get next char. */
				current_char=fgetwc(stream);
				if (current_char == WEOF) {
					intermittend_eof=1;
					break;
				}
			};

			/* Create new token for the string. */
			if ((token=token_new()) == NULL) {
				perror("token_tokenize_stream, token_new");
				exit(EXIT_FAILURE);
			}
			token->type=TOKEN_TYPE_STRING;
			token->line=line;
			token->data.string=buffer;

			/* Add it up to the token list. */
			list_add_tail(&token->list, token_list);

			/* Allocate a new buffer. */
			buffer_size=TOKEN_STREAM_BUFFER_SIZE_START * sizeof(wchar_t);
			buffer_current=0;
			if ((buffer=malloc(buffer_size)) == NULL) {
				perror("token_tokenize_stream, malloc");
				exit(EXIT_FAILURE);
			}
			buffer[0]=L'\0';

			/* Eat up whitespace between tokens. */
			while (iswspace(current_char) && (current_char != L'\n')) {
				current_char=fgetwc(stream);
				if (current_char == WEOF) {
					intermittend_eof=1;
					break;
				}
			};
		};

		/* Create new token for line end. */
		if ((token=token_new()) == NULL) {
			perror("token_tokenize_stream, token_new");
			exit(EXIT_FAILURE);
		}
		token->type=TOKEN_TYPE_EOL;
		token->line=line;

		/* Add it up to the token list. */
		list_add_tail(&token->list, token_list);

		/* Next line. */
		line++;
	};

end_of_file:
	/* Create new token for file end. */
	if ((token=token_new()) == NULL) {
		perror("token_tokenize_stream, token_new");
		exit(EXIT_FAILURE);
	}
	token->type=TOKEN_TYPE_EOF;
	token->line=line;

	/* Add it up to the token list. */
	list_add_tail(&token->list, token_list);

	/* Return tokens. */
	return;
}


/* Match a token against an array of keywords. */
static int token_match_keywords(token_t *token, wchar_t *keywords[])
{
	int      counter=0, result=-1;
	wchar_t *keyword;

	/* Don't match tokens which are not a string. */
	if (token->type != TOKEN_TYPE_STRING) return -1;

	/* Try to match against any of the given keywords. */
	keyword=keywords[counter];
	while (keyword != NULL) {
		if (wcsncmp(keyword, token->data.string, wcslen(token->data.string)) == 0) result=counter;
		counter++; keyword=keywords[counter];
	}

	return result;
}


/* Convert token to keyword index. */
int token_convert_keyword(token_t *token, wchar_t *keywords[])
{
	int index;

	/* Get keyword index. */
	index=token_match_keywords(token, keywords);

	/* Convert the token. */
	if (index != -1) {
		/* Free the string storage. */
		free(token->data.string);

		/* Change token data to keyword index. */
		token->type=TOKEN_TYPE_KEYWORD;
		token->data.keyword=index;
	}

	/* Return the keyword index. */
	return index;
}


/* Convert token into integer. */
int token_convert_integer(token_t *token)
{
	wchar_t *remainder;
	long     result;

	/* Return ok for tokens which are already integers. */
	if (token->type == TOKEN_TYPE_INTEGER) return 0;

	/* Don't convert tokens which are not a string. */
	if (token->type != TOKEN_TYPE_STRING) return -1;
	
	/* Convert string to integer. */
	result=wcstol(token->data.string, &remainder, 0);
	if (*remainder == L'\0') {
		/* No remainder, ok. */
		/* Free the string storage. */
		free(token->data.string);

		/* Change token data to integer number. */
		token->type=TOKEN_TYPE_INTEGER;
		token->data.integer_number=result;

		return 0;
	} else return -1;
}


/* Convert token into integer pair separated by a colon. */
int token_convert_integer_pair(token_t *token)
{
	wchar_t *remainder;
	long     result_first, result_second;

	/* Return ok for tokens which are already integers. */
	if (token->type == TOKEN_TYPE_INTEGER_PAIR) return 0;

	/* Don't convert tokens which are not a string. */
	if (token->type != TOKEN_TYPE_STRING) return -1;
	
	/* Convert string to integer. */
	result_first=wcstol(token->data.string, &remainder, 0);
	if (*remainder == L':') {
		/* Colon follows, ok. */
		result_second=wcstol(remainder+1, &remainder, 0);
		if (*remainder == L'\0') {
			/* No remainder, ok. */
			/* Free the string storage. */
			free(token->data.string);

			/* Change token data to integer number. */
			token->type=TOKEN_TYPE_INTEGER_PAIR;
			token->data.integer_pair.first=result_first;
			token->data.integer_pair.second=result_second;
			return 0;
		}
	}

	/* Error. */
	return -1;
}
