#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>

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
	JSON_TOKEN_LARGE_STRING = 200,
	JSON_TOKEN_SMALL_STRING,
	JSON_TOKEN_INTEGER,
	JSON_TOKEN_DOUBLE,
	JSON_TOKEN_CURLY_OPEN,
	JSON_TOKEN_CURLY_CLOSE,
	JSON_TOKEN_SQUARE_OPEN,
	JSON_TOKEN_SQUARE_CLOSE,
	JSON_TOKEN_COLON,
	JSON_TOKEN_COMMA,
	JSON_TOKEN_BOOL,
	JSON_TOKEN_NULL,
	JSON_TOKEN_WHITESPACE,
} json_token_identity;

// This enum will store the value of the 
// JSON object that the parser emits.
typedef enum {
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_INTEGER,
	JSON_NUMBER,
	JSON_LARGE_STRING,
	JSON_SHORT_STRING,
	JSON_BOOLEAN,
	JSON_NULL
} json_value_type;

// A token will be stored in this data structure.
// The goal of our lexer is to translate an array
// of these tokens. The start end end pointers record
// where in the original JSON file the token starts
// and ends --- this could be used for error formatting
// when we get to cross that bridge.
typedef struct {
	json_token_identity identity;
	char *start;
	char *end;
} json_token_t;

// This is the array of tokens produced by the lexer.
// When the lexer gets a new token, it simply pushes
// it onto the back of this array.
typedef struct {
	size_t count;
	size_t capacity;
	json_token_t *token;
} json_token_array_t;

typedef struct {
	char *key;
	json_value_t *value;
} json_object_keyval_t;

typedef struct {
	size_t count;
	size_t capacity;
	json_object_keyval_t *content;
} json_object_t;

typedef struct {
	size_t count;
	size_t capacity;
	json_value_t *content;
} json_array_t;

// This structure holds a generic JSON value. THis
// could be a JSON object, a JSON array, a number,
// a string, a boolean, or a NULL.
typedef struct {
	json_value_type type;
	union {
		// Beacuse I am using a 64-bit system, I will
		// use 8-bytes as the small string optimization
		// size. These strings will not have their
		// own memory, but will live in the object.
		// This choice is motivated by the fact that the
		// union will expand to the largest sized type in
		// it, which I anticipate are the pointers.
		// Thus, we will size the small strings such that
		// they are equal but not greater in size than
		// the pointers.
		// 
		// Strings larger than 8-bytes will go into
		// large string, and be allocated their own
		// space.
		// 
		// TODO: Check if a system is 64- or 32-bit
		// at compile time and use this to inform the
		// union what the optimal size is for the small
		// string optization tricks.
		char *large_string;
		char[JSON_SMALL_STRING_OPTIMIZATION_SIZE] small_string;

		bool boolean;
		int integer;
		double number;
		json_array_t* array;
		json_object_t* object;
	}
} json_value_t;

extern void json_read_entire_file_to_cstr(const char *path, char **data, size_t *size);
extern json_token_array_t *json_lexer(const char *json_data, size_t json_size);
extern void json_free_token_array(json_token_array_t **tokens);
extern void json_print(json_token_array_t *tokens);
extern json_value_t *json_parser(json_token_array_t *tokens);

#endif
