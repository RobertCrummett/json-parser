#ifndef JSON_H
#define JSON_H

#include <stddef.h>

#ifndef JSON_OBJECT_DEFAULT_CAPACITY
#define JSON_OBJECT_DEFAULT_CAPACITY 8
#endif

#ifndef JSON_ARRAY_DEFAULT_CAPACITY
#define JSON_ARRAY_DEFAULT_CAPACITY 4
#endif

#ifndef JSON_READ_VALUE_BUFFER_SIZE
#define JSON_READ_VALUE_BUFFER_SIZE 128ULL
#endif

// This macro will control the size of the small string optimization
#ifndef JSON_SMALL_STRING_OPTIMIZATION_SIZE
#define JSON_SMALL_STRING_OPTIMIZATION_SIZE 8
#endif

// This is a convenience function for printing JSON tokens
#ifndef JSON_FMT_TOKEN
#define JSON_FMT_TOKEN(t) ((int)(((t).end)-((t).start))),((t).start) 
#endif

// This is the default size for the number of tokens in the
// dynamic tokens array that the lexer will produce
#ifndef JSON_TOKEN_ARRAY_DEFAULT_SIZE
#define JSON_TOKEN_ARRAY_DEFAULT_SIZE 32
#endif

// Size of each input chunk to be read and allocated for by json_read_entire_file_to_cstr
// https://stackoverflow.com/questions/14002954/c-how-to-read-an-entire-file-into-a-buffer
#ifndef JSON_READ_ENTIRE_FILE_CHUNK
#define JSON_READ_ENTIRE_FILE_CHUNK 2097152
#endif

// This enum will encode the type of token
// that the lexer will produce. Each meaningful
// element of a JSON can only take one of these
// values at a time.
typedef enum {
	JSON_TOKEN_STRING = 200,
	JSON_TOKEN_NUMBER,
	JSON_TOKEN_CURLY_OPEN,
	JSON_TOKEN_CURLY_CLOSE,
	JSON_TOKEN_SQUARE_OPEN,
	JSON_TOKEN_SQUARE_CLOSE,
	JSON_TOKEN_COLON,
	JSON_TOKEN_COMMA,
	JSON_TOKEN_BOOLEAN,
	JSON_TOKEN_NULL,
	JSON_TOKEN_WHITESPACE,
} json_token_identity_t;

// This enum will store the value of the 
// JSON object that the parser emits.
typedef enum {
	JSON_OBJECT = 100,
	JSON_ARRAY,
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOLEAN,
	JSON_NULL
} json_value_identity_t;

// A token will be stored in this data structure.
// The goal of our lexer is to translate an array
// of these tokens. The start end end pointers record
// where in the original JSON file the token starts
// and ends --- this could be used for error formatting
// when we get to cross that bridge.
typedef struct json_token_t json_token_t;

struct json_token_t {
	json_token_identity_t identity;
	char *start;
	char *end;
	json_token_t *next;
};

typedef struct json_value_t json_value_t;

struct json_value_t {
	json_value_identity_t identity;
	json_value_t *next;

	double number;
	char *string;
	int boolean;
	json_value_t *key;
	json_value_t *value;
};

extern void json_read_entire_file_to_cstr(const char *path, char **data, size_t *size);
extern json_token_t *json_lexer(const char *json_data, size_t json_size);
extern void json_print(json_token_t *tokens);
extern json_value_t *json_parser(json_token_t *tokens);
extern void json_free_all_the_tokens(json_token_t **token);

#endif
