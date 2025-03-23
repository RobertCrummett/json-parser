#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

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

////////////////////////////////////////////////////////////////////////////////
//                                  Containers
////////////////////////////////////////////////////////////////////////////////

// A token will be stored in this data structure.
// The goal of our lexer is to translate an array
// of these tokens. The start end end pointers record
// where in the original JSON file the token starts
// and ends --- this could be used for error formatting
// when we get to cross that bridge.
typedef struct json_token_t {
	json_token_identity_t identity;

	char *start;
	char *end;
	char *line_start;
	struct json_token_t *next;
} json_token_t;

typedef struct json_array_t {
	struct json_array_t *next;
	json_value_t *content;
} json_array_t;

typedef struct json_keyval_t {
	struct {
		char *ptr;
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
		char *string;
		int boolean;
		json_array_t *array;
		json_object_t *object;
	};
};

////////////////////////////////////////////////////////////////////////////////
//                                File Handling
////////////////////////////////////////////////////////////////////////////////

static void json_read_entire_file_to_cstr(const char *path, char **data, size_t *size) {
	// Make sure that the vaues of data and size are not NULL,
	// otherwise I will accidentally dereference a NULL pointers
	if (data == NULL || size == NULL) {
		fprintf(stderr, "The data (%p) and/or size (%p) cannot be NULL\n", data, (void *)size);
		return;
	}

	// Now set the initial values of data to NULL and size to 0
	// If these values are returned then there was an error.
	*data = NULL;
	*size = 0;

	// The `temp` storage will hold the result of `realloc` until
	// I have verified that `realloc` has not failed for some reason.
	// The `used` variable will hold the amount of the buffer we
	// have used while transferring the file contents to the data array.
	char *temp = NULL;
	size_t used = 0;

	// Here we open the file. I am opening in with the "rb" flags
	// to try and avoid shenanigans reading CRLF on Windows correctly.
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
		// Upon error, return NULL `data` and 0 `size`
		return;
	}

	// While all of the memory from the file has not been read, we will
	// continue reading. An EOF signal in the FILE will break us out of
	// this reading loop.
	//
	// Within the loop we will allocate a buffer for the file contents
	// in data. Then we will read the file contents into the buffer. If
	// the buffer is filled and the file contents not exhausted, we will
	// continue reallocate the buffer at a larger size and continue reading
	// it. When the file contents are evenutally exhausted we will break out
	// of the loop and resize the buffer such that it fits the true size of
	// the data.
	while (1) {
		if (used + JSON_READ_ENTIRE_FILE_CHUNK + 1 > *size) {
			*size = used + JSON_READ_ENTIRE_FILE_CHUNK + 1;

			// Check for numeric overflow
			if (*size <= used) {
				if (*data != NULL)
					free(*data);
				fclose(file);
				fprintf(stderr, "Failed reading the file into memory because the size overflowed the size_t type\n");
				// Upon error, return NULL `data` and 0 `size`
				*data = NULL;
				*size = 0;
				return;
			}

			// Allocate the buffer
			temp = realloc(*data, *size);
			if (temp == NULL) {
				if (*data != NULL)
					free(*data);
				fclose(file);
				fprintf(stderr, "Failed to reallocate data (size %zu bytes) to %zu bytes: %s\n", sizeof *data, *size, strerror(errno));
				// Upon error, return NULL `data` and 0 `size`
				*data = NULL;
				*size = 0;
				return;
			}
			*data = temp;
		}

		// With a resized buffer, go ahead and try to fill the buffer
		// with contents from the file. If the buffer is filled and the
		// file not exhausted, we will continue looping. Otherwise, the
		// file contents are exhausted and the EOF check at the end of
		// the scope will break us out of the loop.
		size_t n = fread(*data + used, 1, JSON_READ_ENTIRE_FILE_CHUNK, file);
		if (ferror(file)) {
			if (*data != NULL)
				free(*data);
			fclose(file);
			fprintf(stderr, "Failed to read chunk of %s into data: %s\n", path, strerror(errno));
			// Upon error, return NULL `data` and 0 `size`
			*data = NULL;
			*size = 0;
			return;
		}

		// Increment the amount of buffer space that has been used up
		used += n;

		// Break out of reading if the file contents are all in the data array
		if (feof(file))
			break;
	}
	fclose(file);

	// Finally, now that all of the data has been read from the file,
	// resize the buffer to fit the data size. Note that the data size
	// will not include the '\0' terminator that we need to end a c
	// style string, so `used + 1` is the final size of the data
	// container, and `used` is reported as the final size.
	temp = realloc(*data, used + 1);
	if (temp == NULL) {
		if (*data != NULL)
			free(*data);
		fprintf(stderr, "Failed to reallocate the data buffer to %zu bytes: %s\n", used + 1, strerror(errno));
		// Upon error, return NULL `data` and 0 `size`
		*data = NULL;
		*size = 0;
		return;
	}
	*data = temp;
	*size = used;
	(*data)[used] = '\0';
}

////////////////////////////////////////////////////////////////////////////////
//                                 List
////////////////////////////////////////////////////////////////////////////////

static json_array_t *json_array_xor(json_array_t *a, json_array_t *b) {
	return (json_array_t *)((uintptr_t)a ^ (uintptr_t)b);
}

static void json_array_next(json_array_t **previous, json_array_t **current) {
	json_array_t *temporary = json_array_xor(*previous, (*current)->next);
	*previous = *current;
	*current = temporary;
}

static json_array_t *json_create_array(void) {
	json_array_t *array = malloc(sizeof *array);
	if (array == NULL) {
		fprintf(stderr, "Failed to allocate a new array");
	}
	array->next = NULL;
	array->content = NULL;

	return array;
}

static void json_array_append(json_array_t *array, json_value_t *value) {
	if (array == NULL || value == NULL)
		return;

	// If there are no nodes in the array, we can simply let 
	// the value be the first element of the array
	if (array->next == NULL && array->content == NULL) {
		array->content = value;
		return;
	}

	// Now we can be sure a new array node needs to be allocated
	json_array_t *new = malloc(sizeof *new);
	if (new == NULL) {
		fprintf(stderr, "Failed to create a new array node!\n");
		return;
	}
	new->content = value;

	// If there is only one node in the array, then the node's
	// next pointer is NULL. In this case, make the new node's
	// next point to the array node and the array node's next point
	// to the new node
	if (array->next == NULL) {
		array->next = json_array_xor(new, array);
		new->next   = json_array_xor(new, array);
		return;
	}

	// If there is more than one node in the array, we need to
	// iterate down the array until we find the end (ie, where
	// the previous node equals the the current node)
	json_array_t *previous = array;
	json_array_t *current = array;

	// Make the first step down the linked list
	json_array_next(&previous, &current);

	// Iterate until you reach the end of the list
	while (previous != current)
		json_array_next(&previous, &current);

	// Now iterate one more time, so current points
	// at the second to last element and previous
	// points at the last element
	json_array_next(&previous, &current);

	// Now emplace the new node at the end of the list
	new->next = json_array_xor(new, previous);

	// Finally, link the new node with tht rest of the chain
	previous->next = json_array_xor(new, current);
}

static void json_print_array(json_array_t *array) {
	if (array == NULL)
		return;

	printf("[");

	// This is the empty array case
	if (array->content == NULL && array->next == NULL) {
		printf("]\n");
		return;
	}

	// This is the one node case
	if (array->next == NULL) {
		json_print(array->content);
		printf("]\n");
		return;
	}

	// This is the multi node case
	printf("\n");
	json_array_t *previous = array;
	json_array_t *current = array;

	// Now step down the linked-list,
	// each time printing the value of
	// the previous array node.
	json_array_next(&previous, &current);
	while (previous != current) {
		json_print(previous->content);
		printf(",\n");
		json_array_next(&previous, &current);
	}
	json_print(previous->content);
	printf("\n]\n");
}

static void json_free_array(json_array_t **array) {
	if (array == NULL || *array == NULL)
		return;

	json_array_t *temporary = NULL;
	json_array_t *previous = *array;
	json_array_t *current = *array;

	// Make the first step down the linked list
	json_array_next(&previous, &current);

	// When the previous is the same as the pointer,
	// we know we have hit the end of the list.
	// This is the due to the invariant condition
	// of the XOR operation. While we have not
	// yet hit the end, free the trailing node (previous).
	while (previous != current) {
		temporary = previous;
		json_array_next(&previous, &current);
		// Recursively free whatever the array actually holds
		json_free(&temporary->content);
		free(temporary);
	}

	// Now that we are done freeing the array,
	// only the final node remains. Delete that sucker.
	free(previous);
	*array = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//                              Hash Table
////////////////////////////////////////////////////////////////////////////////

static uint64_t json_hash_string_view(const char *view, size_t size) {
	// This is the FNV-1a hash algorithm for 64-bit hashes.
	// Simple and effective is why I choose to use it.

	// Recommended start value for FNV-1a hash algorithm
	uint64_t hash = 1099511628211ULL;
	const uint8_t *data = (const uint8_t *)view;

	// For each octet of data to be hashed...
	for (size_t i = 0; i < size; i++)
		hash = (hash ^ data[i]) * 14695981039346656037ULL; // The magic number is the FNV-1a 64-bit offset basis
	return hash;
}

static int json_comp_string_view(const char *view_a, const char *view_b, size_t size) {
	// Compare the value of two string views character by character
	for (size_t i = 0; i < size; i++)
		if (view_a[i] != view_b[i])
			return 0;
	return 1;
}

static double json_object_current_load(json_object_t *object) {
	// This function returns the current load of the
	// hash table, which is defined as the amount of
	// elements in the table (count) divided by the
	// amount of room (capacity) in the table
	return (double)object->count / object->capacity;
}

static int json_object_needs_to_expand(json_object_t *object) {
	// If the load factor of the object exceeds 0.75,
	// or whatever hardcoded threshold that is set, then
	// we should expand the table. Otherwise return false.
	return (json_object_current_load(object) > 0.75) ? 1 : 0;
}

static json_object_t* json_create_object(size_t capacity) {
	// Create a new object that is completely empty of size `capacity`

	// Allocate the new object
	json_object_t* object = malloc(sizeof *object);
	if (object == NULL) {
		fprintf(stderr, "Failed to create a new object: %s\n", strerror(errno));
		return NULL;
	}
	object->count = 0;
	object->capacity = capacity;

	// Allocate the contents that the object manages.
	json_keyval_t* content = calloc(capacity, sizeof *content);
	if (content == NULL) {
		fprintf(stderr, "Failed to create the content of the new object: %s\n", strerror(errno));
		free(object);
		return NULL;
	}
	object->content = content;

	// Make sure that all of the keys point to NULL
	// so that we indicate that they are all empty.
	for (size_t i = 0; i < capacity; i++)
		object->content[i].key.ptr = NULL;

	return object;
}

static void json_free_object(json_object_t **object) {
	// Free the entire object, recursively calling JSON free to
	// free the key-value contents
	if (object == NULL || *object == NULL)
		return;

	for (size_t i = 0; i < (*object)->capacity; i++) {
		// Check if the value exists - if it does,
		// free it. We do this by first freeing
		// the key string and then freeing the
		// value.
		if ((*object)->content[i].key.ptr != NULL) {
			free((*object)->content[i].key.ptr);
			json_free(&(*object)->content[i].value);
		}
	}

	// Now free the array holding all of the freed key value pairs
	free((*object)->content);

	// Now free the object
	free(*object);
}

static int json_object_expand(json_object_t *object) {
	// First, a new capacity for the table is selected.
	// If the object capacity is 0 or 1, then we cannot
	// scale it up - we need to reset the size to some
	// minimum default value. Otherwise, simply scale
	// the capacity by a fixed constant.
	size_t new_capacity = object->capacity < 2 ? 16 : 2 * object->capacity;

	// Check for wrap around in size_t
	if (new_capacity <= object->capacity) {
		fprintf(stderr, "table capacity is too large to fit in size_t\n");
		return 1;
	}

	// Allocate memory for the new hash table. This storage will
	// replace the table's current contents once the values inside
	// the current storage are rehashed into this new table
	json_keyval_t *new_content = calloc(new_capacity, sizeof *new_content);
	if (new_content == NULL) {
		fprintf(stderr, "Failed to allocate new space during the table rehashing\n");
		return 1;
	}

	// Linearly step through object contens and rehash each
	// key value pair. Then place the rehashed key
	// value pair into the new table space.
	for (size_t j = 0; j < object->capacity; j++) {
		json_keyval_t bucket = object->content[j];

		// Test if the key exists
		if (bucket.key.ptr != NULL) {
			// Compute the 64-bit hash of the bucket's key to get
			// an index into the content array
			uint64_t hash = json_hash_string_view(bucket.key.ptr, bucket.key.size);
			uint64_t index = hash % new_capacity;

			// Insert the bucket into the new content memory at the new index
			while (1) {
				// Check if the bucket is space is available
				if (new_content[index].key.ptr == NULL) {
					new_content[index] = bucket;
					break;
				} else
					// The hash collided.
					// This cannot possibly be a duplicate key value because the contents
					// we are moving into the new table are table entries themselves.
					//
					// Collision resolution is currently accomplished via linear probing
					index = (index + 1) % new_capacity; 
			}
		}
	}
	// Remove the old table contents and replace
	// the with the rehashed, expanded contents.
	free(object->content);
	object->capacity = new_capacity;
	object->content = new_content;
	return 0;
}

static int json_object_set(json_object_t *object, char *key, json_value_t *value) {
	// Since we are adding elements to the table, we first need
	// to see whether or not the table needs to expand
	if (json_object_needs_to_expand(object))
		if (json_object_expand(object)) { 
			fprintf(stderr, "Failed to expand the JSON object\n");
			return 1;
		}

	// Compute the 64-bit hash of the bucket's key to get
	// an index into the table content
	uint64_t hash = json_hash_string_view(key, strlen(key));
	uint64_t index = hash % object->capacity;

	// Now that we have a hash and know that there is adequate room
	// in the table, we can put the key-value pair into the table
	while (1) {
		if (object->content[index].key.ptr == NULL) {
			object->content[index].key.ptr = key;
			object->content[index].key.size = strlen(key);
			object->content[index].value = value; 
			object->count++;
			break;
		} else {
			if (json_comp_string_view(object->content[index].key.ptr, key, object->content[index].key.size))
				// The key is a duplicate. No need to do anything.
				break;
			else
				// The hash collided.
				// Collision resolution is currently accomplished via linear probing
				index = (index + 1) % object->capacity; 
		}
	}

	return 0;
}

static json_value_t *json_object_get(json_object_t *object, char *key) {
	// Compute the 64-bit hash of the key to get an index into the table
	uint64_t hash = json_hash_string_view(key, strlen(key));
	uint64_t index = hash % object->capacity;

	// Search for the value in the table
	while (1) {
		// If the value indexed is NULL at any point, then the value
		// does not exist in the table. Return NULL in this case.
		if (object->content[index].key.ptr == NULL)
			return NULL;
		else {
			if (json_comp_string_view(object->content[index].key.ptr, key, object->content[index].key.size))
				// The key exists and we have found it. Return the value.
				return object->content[index].value;
			else
				// The key may exist, but this is not the correct value.
				// The hash has collided. Continue search by linearly probing.
				index = (index + 1) % object->capacity; 
		}
	}
}

static void json_print_object(json_object_t *object) {
	if (object == NULL)
		return;

	// This is the empty object
	if (object->count == 0)
		printf("{}\n");

	// Otherwise, there are elements
	printf("{\n");

	// We will loop over the entire hash table and
	// print the keys & values that are present
	for (size_t i = 0; i < object->capacity; i++) {
		if (object->content[i].key.ptr != NULL) {
			printf("\t%.*s : ", (int)object->content[i].key.size, object->content[i].key.ptr);
			json_print(object->content[i].value);
			printf("\n");
		}
	}
	printf("}\n");
}

////////////////////////////////////////////////////////////////////////////////
//                                Lexer
////////////////////////////////////////////////////////////////////////////////

static void json_free_all_the_tokens(json_token_t **token) {
	// No need to dereference the NULL pointer
	// if it ends up being passed. Just silently
	// return and note that this function is not
	// scared of NULL's
	if (token == NULL || *token == NULL)
		return;

	json_token_t *now   = *token;
	json_token_t *later = now->next;

	// Now step through each node one by one, checking
	// that neither the current nor the previous nodes
	// are NULL pointers. If either are, there is only 
	// one node remaining in the list. Previous should
	// point to it. We will now free it.
	while (later != NULL) {
		free(now);
		now = later;
		later = now->next;
	}

	// Finally, free the last node and return the
	// NULL pointer so that there is no confusion 
	// we did our job.
	free(now);
	*token = NULL;
}

static void json_append_token_to_list(json_token_t **head_of_list, json_token_t *token) {
	// Abort mission if a NULL is passed
	// What kind of a weirdo would even try to do this!?
	if (head_of_list == NULL)
		return;

	// In this case, the list is of length zero. Logically,
	// that means that the token will become the new head of
	// this list.
	if (*head_of_list == NULL)
		*head_of_list = token;
	// Otherwise, the length of the list is non zero.
	// Iterate over the list elements until the end is found,
	// the append the `new_token`
	else {
		json_token_t *probe = *head_of_list;
		while (probe->next != NULL)
			probe = probe->next;
		probe->next = token;
	}
}

static json_token_t *json_lexer(const char *json_data, size_t json_size) {
	// This is the JSON lexer. Its job is to translate
	// the string data in `json_data` into a list of tokens
	// that the parser can read. We will use a linked list
	// of tagged union types to do this, because that is
	// all I know how to do right now.

	json_token_t *tokens = NULL;

	// Position is the place in the JSON c string `json_data` we are while parsing
	// Line is the line in the JSON file we are at while parsing
	// Line start is the position of the first character of the current line in the JSON file we are parsing
	size_t line = 1;
	char *line_start = (char *)json_data;

	// While we have not yet reached the end of the JSON file...
	while (*json_data != '\0' && json_size-- != 0) {
		// Create a new token to ultimately append to the list of tokens
		json_token_t *token = malloc(sizeof *token);
		if (token == NULL) {
			fprintf(stderr, "Failed to allocate a new token: %s\n", strerror(errno));
			json_free_all_the_tokens(&tokens);
			return NULL;
		}
		token->next = NULL;

		// Because the JSON can be lexed character by character,
		// that is what I will do! This switch case will encode all of my logic.
		switch (*json_data) {
			case '{':
				token->identity = JSON_TOKEN_CURLY_OPEN;
				token->start = (char *)json_data;
				token->end = (char *)++json_data;
				token->line_start = line_start;
				break;
			case '}':
				token->identity = JSON_TOKEN_CURLY_CLOSE;
				token->start = (char *)json_data;
				token->end = (char *)++json_data;
				token->line_start = line_start;
				break;
			case '[':
				token->identity = JSON_TOKEN_SQUARE_OPEN;
				token->start = (char *)json_data;
				token->end = (char *)++json_data;
				token->line_start = line_start;
				break;
			case ']':
				token->identity = JSON_TOKEN_SQUARE_CLOSE;
				token->start = (char *)json_data;
				token->end = (char *)++json_data;
				token->line_start = line_start;
				break;
			case '\n':
				// First ncrement the line number and pointer to the start of
				// the line for error handline purposes, then fall through to
				// the other whitespace cases
				line++;
				line_start = (char *)json_data;
			case ' ':
			case '\t':
			case '\r':
				token->identity = JSON_TOKEN_WHITESPACE;
				token->start = (char *)json_data;
				token->end = (char *)++json_data;
				token->line_start = line_start;
				break;
			case '"':
				// We are pointing at the first quote - not
				// the first character of the string. First
				// increment the pointer, then we are at the
				// starting location
				token->identity = JSON_TOKEN_STRING;
				token->start = (char *)++json_data;
				
				// Now continue stepping through the string until we
				// find a quotation mark '"' that is not escaped by
				// a backslash '\\'
				while (*json_data != '"' || *(json_data-1) == '\\')
					json_data++;

				// We now point at the final quote. This is the
				// end of the string.
				token->end = (char *)json_data++;
				token->line_start = line_start;
				break;
			case ':':
				token->identity = JSON_TOKEN_COLON;
				token->start = (char *)json_data;
				token->end = (char *)++json_data;
				token->line_start = line_start;
				break;
			case ',':
				token->identity = JSON_TOKEN_COMMA;
				token->start = (char *)json_data;
				token->end = (char *)++json_data;
				token->line_start = line_start;
				break;
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				token->identity = JSON_TOKEN_NUMBER;
				token->start = (char *)json_data;

				// The first four conditions being satisified means 
				// that we are dealing with some sort of fraction 
				// or exponential value. We just fall through in this
				// case.
				//
				// Otherwise, we are dealing with an ordinary
				// number until we hit something weird like a
				// '.', 'e', or 'E', or end of JSON number
				if (*json_data == '-' && json_data[1] == '0')
					json_data++;
				else if (*json_data == '0' && json_data[1] == '.')
					json_data++;
				else if (*json_data == '0' && json_data[1] == 'e')
					json_data++;
				else if (*json_data == '0' && json_data[1] == 'E')
					json_data++;
				else if (*json_data >= '1' && *json_data <= '9') {
					json_data++;
					// Loop over digits until non digit is found
					while (*json_data >= '0' && *json_data <= '9')
						json_data++;
				}

				// Question: are you a fraction?
				if (*json_data == '.') {
					json_data++;
					
					// Loop over digits until non digit is found
					while (*json_data >= '0' && *json_data <= '9')
						json_data++;
				}

				// Question: are you an exponential?
				if (*json_data == 'E' || *json_data == 'e') {
					json_data++;

					// Check for optional sign
					if (*json_data == '-' || *json_data == '+')
						json_data++;

					// Loop over digits until non digit is found
					while (*json_data >= '0' && *json_data <= '9')
						json_data++;
				}

				token->end = (char *)json_data;
				token->line_start = line_start;
				break;
			case 't':
			case 'f':
				token->identity = JSON_TOKEN_BOOLEAN;
				token->start = (char *)json_data;

				// The end offset will depend on whether the
				// boolean value is true (+4) or false (+5)
				if (*json_data == 't')
					json_data += 4;
				else if (*json_data == 'f')
					json_data += 5;

				token->end = (char *)json_data;
				token->line_start = line_start;
				break;
			case 'n':
				token->identity = JSON_TOKEN_NULL;
				token->start = (char *)json_data;
				json_data += 4;
				token->end = (char *)json_data;
				token->line_start = line_start;
				break;
			default:
				// If this situation is encountered, it means that an unexpected
				// character has been found in the file. The file in this case
				// cannot be a standard JSON file. Report this

				fprintf(stderr, "Read an unexpected character on line %zu of the JSON file:\nLine %zu of the JSON file:\n\n", line, line);

				// Print the entire contents of the current line
				while (*line_start != '\n' && *line_start != '\0')
					fprintf(stderr, "%c", *line_start++);

				fprintf(stderr, "\n\nMake sure only standard complient JSON files are used!\n");

				if (tokens != NULL)
					json_free_all_the_tokens(&tokens);

				return NULL;
		}

		// Rather than an array, I will use a linked list. This is because
		// I want to finish this project, not be here a million years.
		//
		// TODO: Use a tail pointer or a dynamic array to accelerate appending
		// TODO: Ditch the concept of a chain altogether and 

		json_append_token_to_list(&tokens, token);
	}

	return tokens;
}

static void json_print_tokens(json_token_t *token) {
	// For each token, we will test its identity and print its value
	// to the standard output accordingly.
	while (token != NULL) {
		switch (token->identity) {
			case JSON_TOKEN_STRING:
				printf("\"%.*s\"", JSON_FMT_TOKEN(*token));
				break;
			case JSON_TOKEN_NUMBER:
			case JSON_TOKEN_CURLY_OPEN:
			case JSON_TOKEN_CURLY_CLOSE:
			case JSON_TOKEN_SQUARE_OPEN:
			case JSON_TOKEN_SQUARE_CLOSE:
			case JSON_TOKEN_COLON:
			case JSON_TOKEN_COMMA:
			case JSON_TOKEN_BOOLEAN:
			case JSON_TOKEN_NULL:
			case JSON_TOKEN_WHITESPACE:
				printf("%.*s", JSON_FMT_TOKEN(*token));
				break;
			default:
				fprintf(stderr, "\nUnexpected identity code recieved: %d\n", token->identity);
				return;
		}

		// And do not forget to step forward one token in the list!
		token = token->next;
	}
}

static char *json_token_identity_string(json_token_t *token) {
	// The NULL token case
	if (token == NULL)
		return NULL;
	
	switch (token->identity) {
		case JSON_TOKEN_STRING:
			return "string";
		case JSON_TOKEN_NUMBER:
			return "number";
		case JSON_TOKEN_CURLY_OPEN:
			return "open curly \"{\"";
		case JSON_TOKEN_CURLY_CLOSE:
			return "closed curly \"}\"";
		case JSON_TOKEN_SQUARE_OPEN:
			return "open square \"[\"";
		case JSON_TOKEN_SQUARE_CLOSE:
			return "closed curly \"]\"";
		case JSON_TOKEN_COLON:
			return "colon \":\"";
		case JSON_TOKEN_COMMA:
			return "comma \",\"";
		case JSON_TOKEN_BOOLEAN:
			return "boolean";
		case JSON_TOKEN_NULL:
			return "null";
		case JSON_TOKEN_WHITESPACE:
			return "whitespace";
		default:
			return "unknown identity";
	}
}

static void json_print_unexpected_token(json_token_t *token, const char *anticipated) {
	fprintf(stderr, "An unexpected token was encountered! Current line:\n\n");
	// Print the entire contents of the current line
	while (*token->line_start != '\n' && *token->line_start != '\0') {
		fprintf(stderr, "%c", *token->line_start);
		token->line_start++;
	}
	fprintf(stderr, "\n\nWe expected to find a %s token, the key of a key-value pair in the current object, but instead we found a %s token\n", anticipated, json_token_identity_string(token));
}

////////////////////////////////////////////////////////////////////////////////
//                                Parser
////////////////////////////////////////////////////////////////////////////////

static void json_gobble_whitespace(json_token_t **tokens) {
	// Make sure NULL was not passed in. Although
	// this is not unreasonable, we do not want to
	// accidentally dereference it.
	if (tokens == NULL || *tokens == NULL)
		return;

	// While the identity of the current token is
	// a whitespace and the token list is not exhausted,
	// iterate the token list
	//
	// Finally, when a non whitespace token is found, the
	// FOLLOWING token will be returned.

	while (*tokens != NULL && (*tokens)->identity == JSON_TOKEN_WHITESPACE)
		*tokens = (*tokens)->next;
}

static char *json_string_view_to_cstring(char *start, char *end) {
	// First allocate enough mempory to hold all of the characters in the original
	// string AND a final null terminator (+1)
	size_t size = end - start;
	char *cstring = malloc((size + 1) * sizeof *cstring);
	if (cstring == NULL) {
		fprintf(stderr, "Failed to allocate space for a new cstring: %s\n", strerror(errno));
		return NULL;
	}

	// Copy the contents of the string view (start) into the new
	// memory of the c style string. There is a maximum of size
	// characters in the view, so do not copy any more than that.
	strncpy(cstring, start, size);
	cstring[size] = '\0';
	return cstring;
}

static json_value_t *json_parser(json_token_t **tokens) {
	// If there are no tokens, there is no work to do
	if (tokens == NULL || *tokens == NULL)
		return NULL;

	// And if there is only whitespace tokens, there is
	// no work to do
	json_gobble_whitespace(tokens);
	if (*tokens == NULL)
		return NULL;

	// There must be productive tokens to parse at this point.
	// Create a new element that will store the contents held by the tokens we parse

	json_value_t *element = malloc(sizeof *element);
	if (element == NULL) {
		fprintf(stderr, "Failed to allocate space for a new element of the JSON structure: %s\n", strerror(errno));
		return NULL;
	}

	// Parse the token according to its identity
	switch ((*tokens)->identity) {
		case JSON_TOKEN_CURLY_OPEN:
			element->identity = JSON_OBJECT;
			element->object = json_create_object(JSON_OBJECT_DEFAULT_SIZE);

			// Now step to the next token and eat up all
			// of the optional whitespace
			*tokens = (*tokens)->next;
			json_gobble_whitespace(tokens);

			// Check if the object is empty. If it is, break
			// out of the switch case without stepping to the next token.
			// We step to the next token at the very end of this function.
			if ((*tokens)->identity == JSON_TOKEN_CURLY_CLOSE)
				break;

			while (1) {
				// We expect to see a key. On the happy path,
				// we will simply copy the contents of the string view in the token
				// into the key of our object.
				if ((*tokens)->identity != JSON_TOKEN_STRING) {
					json_print_unexpected_token(*tokens, "string");
					json_free(&element);
					return NULL;
				}
				char *key = json_string_view_to_cstring((*tokens)->start, (*tokens)->end);
				if (key == NULL) {
					fprintf(stderr, "Failed to allocate the string view as a c style string on the heap: %s\n", strerror(errno));
					json_free(&element);
					return NULL;
				}

				// Proceeding past the key, there should be (optional) whitespace,
				// followed by a colon, followed by some more (optional) whitespace,
				// and finally the value
				*tokens = (*tokens)->next;
				json_gobble_whitespace(tokens);
				if ((*tokens)->identity != JSON_TOKEN_COLON) {
					json_print_unexpected_token(*tokens, "colon \":\"");
					json_free(&element);
					free(key);
					return NULL;
				}
				*tokens = (*tokens)->next;
				json_gobble_whitespace(tokens);

				json_value_t *value = json_parser(tokens);
				if (value == NULL) {
					fprintf(stderr, "Failed to parse the tokens.\nNULL returned from `json_parser` while parsing the elements of a JSON object.\nBubbling up...\n");
					json_free(&element);
					free(key);
					return NULL;
				}

				// Now put the key-value pair into the JSON object. Management of the memory will
				// be up to the `element` now instead of the key and value. In fact, we should forget
				// we even have value and key pointers at this point.
				if (json_object_set(element->object, key, value)) {
					fprintf(stderr, "Failed to emplace the current key-value pair into the object.\n");
					json_free(&element);
					json_free(&value);
					free(key);
					return NULL;
				}

				// Now test if there are more values in the object or if the contents are complete.
				json_gobble_whitespace(tokens);
				if ((*tokens)->identity == JSON_TOKEN_COMMA) {
					*tokens = (*tokens)->next;
					json_gobble_whitespace(tokens);
					continue;
				} else if ((*tokens)->identity == JSON_TOKEN_CURLY_CLOSE)
					break;
				else {
					json_print_unexpected_token(*tokens, "comma \",\" or closing brace \"}\"");
					json_free(&element);
					return NULL;
				}
			}
			break;
		case JSON_TOKEN_SQUARE_OPEN:
			element->identity = JSON_ARRAY;
			element->array = json_create_array();

			// Now step to the next token and eat up all
			// of the optional whitespace
			*tokens = (*tokens)->next;
			json_gobble_whitespace(tokens);

			// Check if the array is empty. If it is, break
			// out of the switch case without stepping to the next token.
			// We step to the next token at the very end of this function.
			if ((*tokens)->identity == JSON_TOKEN_SQUARE_CLOSE)
				break;

			while (1) {
				// We expect to see a value. So we can recursively call
				// the parsing function to give use the value;
				json_value_t *value = json_parser(tokens);
				if (value == NULL) {
					fprintf(stderr, "Failed to parse the tokens.\nNULL returned from `json_parser` while parsing the elements of a JSON array.\nBubbling up...\n");
					json_free(&element);
					return NULL;
				}

				// Now add the new value to the array
				json_array_append(element->array, value);

				// Now test if there are more values in the array or if the contents are complete.
				json_gobble_whitespace(tokens);
				if ((*tokens)->identity == JSON_TOKEN_COMMA) {
					*tokens = (*tokens)->next;
					continue;
				} else if ((*tokens)->identity == JSON_TOKEN_SQUARE_CLOSE)
					break;
				else {
					json_print_unexpected_token(*tokens, "comma \",\" or closing bracket \"]\"");
					json_free(&element);
					return NULL;
				}
			}
			break;

		case JSON_TOKEN_STRING:
			element->identity = JSON_STRING;
			element->string = json_string_view_to_cstring((*tokens)->start, (*tokens)->end);
			if (element->string == NULL) {
				fprintf(stderr, "Failed to allocate the string view as a c style string on the heap: %s\n", strerror(errno));
				json_free(&element);
				return NULL;
			}
			break;

		case JSON_TOKEN_NUMBER:
			// Currently, I allocate and free every time
			// that I need to create a number! This is inefficient,
			// but I have not figured out a more reliable way
			//
			// TODO: Transfer char* to string without allocating heap space
			element->identity = JSON_NUMBER;
			char *string = json_string_view_to_cstring((*tokens)->start, (*tokens)->end);
			if (string == NULL) {
				fprintf(stderr, "Failed to allocate the string view as a c style string on the heap: %s\n", strerror(errno));
				json_free(&element);
				return NULL;
			}
			element->number = strtod(string, NULL);
			free(string);
			break;

		case JSON_TOKEN_BOOLEAN:
			element->identity = JSON_BOOLEAN;
			size_t size = (*tokens)->end - (*tokens)->start;

			if (size == 4 && strncmp((*tokens)->start, "true", size) == 0)
				element->boolean = 1;
			else if (size == 5 && strncmp((*tokens)->start, "false", size) == 0)
				element->boolean = 0;
			else {
				fprintf(stderr, "Failed to parse a boolean token.\nExpected \"true\" or \"false\", but got \"%.5s\".\n", (*tokens)->start);
				json_free(&element);
				return NULL;
			}
			break;

		case JSON_TOKEN_NULL:
			element->identity = JSON_NULL;
			break;

		default:
			fprintf(stderr, "Unexpected identity code encountered while parsing. Token type is \"%s\"\n"
					"This most likely means that the JSON file is not in standard format.\n", json_token_identity_string(*tokens));
			json_print_unexpected_token(*tokens, "[null, boolean, number, string, open braces, open bracket]");
			json_free(&element);
			return NULL;
	}

	// And do not forget to step forward one token in the list!
	*tokens = (*tokens)->next;
	return element;
}

void json_free(json_value_t **json) {
	// This is the user interface level free function designed to
	// free heterogeneous JSON data structures (arbitrarily nested
	// `json_value_t`).
	if (json == NULL || *json == NULL)
		return;

	// Based on what type of node we are dealing with, suballocate
	// the job of freeing accordingly and recusively call ourselves
	// until we hit strings, numbers, null, or booleans.
	switch ((*json)->identity) {
		case JSON_OBJECT:
			json_free_object(&(*json)->object);
			break;
		case JSON_ARRAY:
			json_free_array(&(*json)->array);
			break;
		case JSON_STRING:
			if ((*json)->string != NULL) {
				free((*json)->string);
				(*json)->string = NULL;
			}
			free(*json);
			break;
		case JSON_NUMBER:
			free(*json);
			break;
		case JSON_BOOLEAN:
			free(*json);
			break;
		case JSON_NULL:
			free(*json);
			break;
		default:
			// DO NOT return the NULL pointer in the case of an unexpected value.
			// This is so that we can examine it and check as to whether or not
			// there is any dynamically allocated memory associated with it.
			fprintf(stderr, "Failed trying to free the JSON value because an unexpected `json_value_t` identity code was encountered: identity = %d\n", (*json)->identity);
			return;
	}
		
	// Finally, return the NULL pointer
	*json = NULL;
}

// This is a simple printing function for heterogeneous JSON
// data structures, `json_value_t`'s. This function does not
// yet support pretty printing, i.e., tabs and such for nesting.
//
// TODO: Implement pretty printing.
void json_print(json_value_t *json) {
	if (json == NULL)
		return;

	switch (json->identity) {
		case JSON_OBJECT:
			json_print_object(json->object);
			break;
		case JSON_ARRAY:
			json_print_array(json->array);
			break;
		case JSON_STRING:
			printf("%s", json->string);
			break;
		case JSON_NUMBER:
			printf("%lf", json->number);
			break;
		case JSON_BOOLEAN:
			printf("%s", json->boolean ? "true" : "false");
			break;
		case JSON_NULL:
			printf("null");
			break;
		default:
			break;
	}
}

json_value_t *json_load(const char *path) {
	char *data = NULL;
	size_t size = 0;

	json_read_entire_file_to_cstr(path, &data, &size);
	if (data == NULL && size == 0) {
		fprintf(stderr, "Failed to read %s into memory: %s\n", path, strerror(errno));
		return NULL;
	}

	// Tokenize the JSON string and output the contents into the token_array.
	// This function may fail while resizing the JSON token array.
	// If it does, it will a null array.
	json_token_t *tokens = json_lexer(data, size);
	if (tokens == NULL) {
		fprintf(stderr, "The JSON lexer failed to tokenize %s: %s\n", (char *)path, strerror(errno));
		free(data);
		return NULL;
	}
	json_token_t* head = tokens;
	
	// Parse the token stream and output a conglomerate data structure
	// holding all of the information from the JSON
 	json_value_t *json = json_parser(&tokens);
	if (json == NULL) {
		fprintf(stderr, "The JSON lexer failed to parse %s: %s\n", (char *)path, strerror(errno));
		json_free_all_the_tokens(&head);
		free(data);
		return NULL;
	}

	// Cleanup time
	json_free_all_the_tokens(&head);
	free(data);
	return json;
}
