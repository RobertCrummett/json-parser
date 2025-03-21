#ifndef JSON_H
#define JSON_H

#include <stddef.h>

// This is a convenience function for printing JSON tokens
#ifndef JSON_FMT_TOKEN
#define JSON_FMT_TOKEN(t) ((int)(((t).end)-((t).start))),((t).start) 
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
	JSON_TOKEN_WHITESPACE
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
typedef struct json_token_t {
	json_token_identity_t identity;

	const char *start;
	const char *end;
	const char *line_start;
	struct json_token_t *next;
} json_token_t;

// The JSON value, once read into memory, will live in
// a conglomerate data structure of hash tables and arrays
// of the json_value_t. The dynamic arrays and hash
// table containers are defined below. They are pretty
// standard.
typedef struct json_value_t json_value_t;

typedef struct json_array_t {
	struct json_array_t *next;
	json_value_t *content;
} json_array_t;

typedef struct json_keyval_t {
	struct {
		const char *ptr;
		size_t size;
	} key;
	json_value_t *value;
} json_keyval_t;

typedef struct json_object_t {
	size_t count;
	size_t capacity;
	json_keyval_t *content;
} json_object_t;

struct json_value_t {
	json_value_identity_t identity;

	union {
		double number;
		struct {
			const char *ptr;
			size_t size;
		} string;
		int boolean;
		json_array_t *array;
		json_object_t *object;
	};
};

// This function can read a file into a string.
// Pretty simple, but necessary. It should be
// operating system independant, because it does
// not try to do any seek/rewind tricks - just
// fread into a buffer repeatedly.
extern void json_read_entire_file_to_cstr(const char *path, char **data, size_t *size);

// Lexer type functions. These functions are designed
// to produce and work with tokens
extern json_token_t *json_lexer(const char *json_data, size_t json_size);
extern void json_free_all_the_tokens(json_token_t **token);
extern void json_print_tokens(json_token_t *tokens);

// Parser type functions. This sucks up the tokens and outputs conglomerate data structures
extern json_value_t *json_parser(json_token_t *tokens);

// Conglomerate datastructure: JSON routines
extern void json_free(json_value_t **json);
extern void json_print(json_value_t *json);

// Datastructure: JSON array container routines
extern json_array_t* json_create_array(void);
extern void json_free_array(json_array_t **array);

extern void json_array_append(json_array_t *array, json_value_t *value);
extern void json_print_array(json_array_t *array);

// Datastructure: JSON object container routines
extern json_object_t* json_create_object(size_t capacity);
extern void json_free_object(json_object_t **object);

extern int json_object_set(json_object_t *object, const char *key, json_value_t *value);
extern json_value_t *json_object_get(json_object_t *object, const char *key);
extern void json_print_object(json_object_t *object);

#endif
