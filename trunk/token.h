#ifndef TOKEN_H
#define TOKEN_H

#include "list.h"


/* Buffer sizes for reading a stream into tokens. */
#define TOKEN_STREAM_BUFFER_SIZE_START     100
#define TOKEN_STREAM_BUFFER_SIZE_INCREMENT 50

/* Token types. */
#define TOKEN_TYPE_NULL         0
#define TOKEN_TYPE_EOF          1
#define TOKEN_TYPE_EOL          2
#define TOKEN_TYPE_STRING       3
#define TOKEN_TYPE_KEYWORD      4
#define TOKEN_TYPE_INTEGER      5
#define TOKEN_TYPE_INTEGER_PAIR 6

typedef struct {
	struct list_head list;
	unsigned int     type;
	unsigned int     line;
	union {
		wchar_t       *string;
		unsigned int   keyword;
		long           integer_number;
		struct {
			long first;
			long second;
		} integer_pair;
	} data;	
} token_t;

/* Token operations. */
token_t *token_new(void);
void token_destroy(token_t *ptr);
void token_destroy_list(token_t *ptr);
void token_tokenize_stream(FILE *stream, struct list_head *token_list);
int token_convert_keyword(token_t *token, wchar_t *keywords[]);
int token_convert_integer(token_t *token);
int token_convert_integer_pair(token_t *token);

#endif
