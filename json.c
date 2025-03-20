#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

// Here is the memory allocated for reading small values of memory
// out of JSON tokens and into JSON values
static char json_read_value_buffer[JSON_READ_VALUE_BUFFER_SIZE] = {0};

static void json_pop_a_token(json_token_t **tokens) {
	// NULL check - we do not want to dereference nothing!
	if (tokens == NULL || *tokens == NULL)
		return;

	json_token_t *temp = *tokens;
	*tokens = (*tokens)->next;
	free(temp);
}

static void json_pop_a_value(json_value_t **elements) {
	// NULL check - we do not want to dereference nothing!
	if (elements == NULL || *elements == NULL)
		return;

	json_value_t *temp = *elements;
	*elements = (*elements)->next;
	free(temp);
}

// Walk the linked list and free all of the nodes
void json_free_all_the_tokens(json_token_t **token) {
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

// Walk the linked list and recusively free all of the values
void json_free_all_the_values(json_value_t **elements) {
	// No need to dereference the NULL pointer
	// if it ends up being passed. Just silently
	// return and note that this function is not
	// scared of NULL's
	if (elements == NULL || *elements == NULL)
		return;
	
	// Ok so we are ready to rumble.
	// Now the way in which we free a `json_value_t` is
	// dependant on its identity. So we will check 
	// for an element's identity before doing anything
	// else.
	json_value_t* element = *elements;
	switch (element->identity) {
		case JSON_OBJECT:
			// First we need to free the key, which is a string value type
			json_free_all_the_values(&element->key);

			// Second we need to free the value. This could potentially
			// be a deep call.
			json_free_all_the_values(&element->value);

			// Now that we have cleared the key and the value associated
			// with this object, lets free the object node itself.
			json_pop_a_value(&element);
			break;
		case JSON_ARRAY:
			// We need to clear whatever is being held by the array before
			// we can clear the array. So first we clear the value container
			json_free_all_the_values(&element->value);

			// Now that we have cleared the value associated
			// with the array, lets free the array itself.
			json_pop_a_value(&element);
			break;
		case JSON_STRING:
			// This is the only other data member with auxillary information.
			// But in this case, we can be sure that we do not need to make
			// any recusive calls.
			free(element->string);
		
			// Now free the container
			json_pop_a_value(&element);
			break;
		// The NUMBER, BOOLEAN, and NULL json types are not responsible for
		// any memory other than themselves, so we can just pop them from the
		// value list and keep it moving...
		case JSON_NUMBER:
			json_pop_a_value(&element);
			break;
		case JSON_BOOLEAN:
			json_pop_a_value(&element);
			break;
		case JSON_NULL:
			json_pop_a_value(&element);
			break;
		default:
			fprintf(stderr, "Failed to free the elements because an unexpected value identifier was encountered. I received value code %d\n", element->identity);
			return;
	}

	// Now free the next element in a sequencial manner.
	// The NULL next pointer will act as the base case.
	json_free_all_the_values(&element);
}

static void json_append_token_to_list(json_token_t **head_of_list, json_token_t token) {
	// Abort mission if a NULL is passed
	// What kind of a weirdo would even try to do this!?
	if (head_of_list == NULL)
		return;

	// Make more space on the heap for this new token
	// we would like to append
	json_token_t *new_token = malloc(sizeof *new_token);
	if (new_token == NULL) {
		fprintf(stderr, "Failed to allocate a new node for the linked list! Deleting the entire linked list!!!\n");
		json_free_all_the_tokens(head_of_list);
		return;
	}

	new_token->identity = token.identity;
	new_token->start = token.start;
	new_token->end = token.end;
	new_token->next = NULL;

	// In this case, the list is of length zero. Logically,
	// that means that the token will become the new head of
	// this list.
	if (*head_of_list == NULL)
		*head_of_list = new_token;
	// Otherwise, the length of the list is non zero.
	// Iterate over the list elements until the end is found,
	// the append the `new_token`
	else {
		json_token_t *probe = *head_of_list;
		while (probe->next != NULL)
			probe = probe->next;
		probe->next = new_token;
	}
}

json_token_t *json_lexer(const char *json_data, size_t json_size) {
	// This is the JSON lexer. Its job is to translate
	// the string data in `json_data` into a list of tokens
	// that the parser can read. We will use a linked list
	// of tagged union types to do this, because that is
	// all I know how to do right now.

	// Tokens is the linked list we will return after lexing
	json_token_t *tokens = NULL;

	// Position is the place in the JSON c string `json_data` we are while parsing
	// Line is the line in the JSON file we are at while parsing
	// Line start is the position of the first character of the current line in the JSON file we are parsing
	size_t position = 0;
	size_t line = 1;
	char *line_start = (char *)json_data;

	// While we have not yet reached the end of the JSON file...
	while (*json_data != '\0' && position++ < json_size) {
		char current_character = *json_data++;

		// Create a new token to ultimately append to the list of tokens
		json_token_t current_token = {0};
		current_token.next = NULL;

		// Because the JSON can be lexed character by character,
		// that is what I will do! This switch case will encode all of my logic.
		switch (current_character) {
			case '{':
				current_token.identity = JSON_TOKEN_CURLY_OPEN;
				current_token.start = (char *)json_data - 1;
				current_token.end = (char *)json_data;
				break;
			case '}':
				current_token.identity = JSON_TOKEN_CURLY_CLOSE;
				current_token.start = (char *)json_data - 1;
				current_token.end = (char *)json_data;
				break;
			case '[':
				current_token.identity = JSON_TOKEN_SQUARE_OPEN;
				current_token.start = (char *)json_data - 1;
				current_token.end = (char *)json_data;
				break;
			case ']':
				current_token.identity = JSON_TOKEN_SQUARE_CLOSE;
				current_token.start = (char *)json_data - 1;
				current_token.end = (char *)json_data;
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
				current_token.identity = JSON_TOKEN_WHITESPACE;
				current_token.start = (char *)json_data - 1;
				current_token.end = (char *)json_data;
				break;
			case '"':
				// This points at the first character of the string
				current_token.identity = JSON_TOKEN_STRING;
				current_token.start = (char *)json_data;
				
				// Now continue stepping through the string until we
				// find a quotation mark '"' that is not escaped by
				// a backslash '\\'
				while (*json_data != '"' || *(json_data-1) == '\\')
					json_data++;
				json_data++;

				// Since we now point at the character AFTER the
				// closing quote, we need to rewind one to get the
				// proper ending location - the final quote.
				current_token.end = (char *)json_data - 1;
				break;
			case ':':
				current_token.identity = JSON_TOKEN_COLON;
				current_token.start = (char *)json_data - 1;
				current_token.end = (char *)json_data;
				break;
			case ',':
				current_token.identity = JSON_TOKEN_COMMA;
				current_token.start = (char *)json_data - 1;
				current_token.end = (char *)json_data;
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
				current_token.identity = JSON_TOKEN_NUMBER;
				current_token.start = (char *)json_data - 1;

				// The first four conditions being satisified means 
				// that we are dealing with some sort of fraction 
				// or exponential value. We just fall through in this
				// case.
				//
				// Otherwise, we are dealing with an ordinary
				// number until we hit something weird like a
				// '.', 'e', or 'E', or end of JSON number
				if (current_character == '-' && *json_data == '0')
					;
				else if (current_character == '0' && *json_data == '.')
					;
				else if (current_character == '0' && *json_data == 'e')
					;
				else if (current_character == '0' && *json_data == 'E')
					;
				else if (current_character >= '1' && current_character <= '9')
					// Loop over digits until non digit is found
					if (*json_data >= '0' && *json_data <= '9')
						json_data++;
				
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

				current_token.end = (char *)json_data;
				break;
			case 't':
			case 'f':
				current_token.identity = JSON_TOKEN_BOOLEAN;
				current_token.start = (char *)json_data - 1;

				// The end offset will depend on whether the
				// boolean value is true (+3) or false (+4)
				if (current_character == 't')
					json_data += 3;
				else if (current_character == 'f')
					json_data += 4;

				current_token.end = (char *)json_data;
				break;
			case 'n':
				current_token.identity = JSON_TOKEN_NULL;
				current_token.start = (char *)json_data - 1;
				json_data += 3;
				current_token.end = (char *)json_data;
				break;
			default:
				// If this situation is encountered, it means that an unexpected
				// character has been found in the file. The file in this case
				// cannot be a standard JSON file. Report this
				fprintf(stderr, "Read an unexpected character on line %zu of the JSON file:\nLine %zu of the JSON file:\n\n", line, line);
				while (*line_start != '\n' && *line_start != '\0') {
					fprintf(stderr, "%c", *line_start);
					line_start++;
				}
				fprintf(stderr, "\n\nMake sure only standard complient JSON files are used!\n");
				if (tokens != NULL)
					json_free_all_the_tokens(&tokens);
				return tokens;
		}

		// Rather than an array, I will use a linked list. This is because
		// I want to finish this project, not be here a million years.
		// TODO: Implement you lexer with dynamic arrays or other fast data structures
		json_append_token_to_list(&tokens, current_token);
		if (tokens == NULL) {
			fprintf(stderr, "Something went really wrong while trying to append the current token to the list of tokens!\n");
			return tokens;
		}
	}
	return tokens;
}

void json_print(json_token_t *token) {
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


void json_read_entire_file_to_cstr(const char *path, char **data, size_t *size) {
	// Make sure that the vaues of data and size are not NULL,
	// otherwise I will accidentally dereference a NULL pointers
	if (data == NULL || size == NULL) {
		fprintf(stderr, "The data (%p) and/or size (%p) cannot be NULL\n", data, size);
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

static void json_gobble_whitespace(json_token_t **tokens) {
	// NULL check - we do not want to dereference nothing!
	if (tokens == NULL || *tokens == NULL)
		return;

	// If the token is white space, we want to discard it
	// and continue to do this repeatedly until the next token
	// is not whitespace. However, if the end of the token stream
	// is encountered before the white space is finished, we need
	// to return NULL instead of freeing the token.
	while (*tokens != NULL && (*tokens)->identity == JSON_TOKEN_WHITESPACE)
		json_pop_a_token(tokens);
}


json_value_t *json_parser(json_token_t *tokens) {
	// Do nothing if passed nothing
	if (tokens == NULL)
		return NULL;

	// Destroy all preceeding whitespace, just in case
	json_gobble_whitespace(&tokens);

	// This is the legendary parser. It takes the output of the lexer, a
	// linked list of tokens, and produces another linked list --- but this
	// time, the nodes are `json_value_t` objects!
	json_value_t *elements = malloc(sizeof *elements);
	if (elements == NULL) {
		fprintf(stderr, "Failed to allocate a new value!\n");
		json_free_all_the_tokens(&tokens);
		return NULL;
	}

	switch (tokens->identity) {
		// This is the case when we parse JSON objects. This will
		// call `json_parser` recusively.
		case JSON_TOKEN_CURLY_OPEN:
			elements->identity = JSON_OBJECT;
			elements->next = NULL;
			elements->key = NULL;
			elements->value = NULL;

			// Get the next token after the opening curly
			json_pop_a_token(&tokens);
			
			// Eat up some whitespace!
			json_gobble_whitespace(&tokens);
			
			// If there are no elements in the object, then 
			// there should be a curly close here. Otherwise,
			// what we have is a key to parse!
			if (tokens->identity == JSON_TOKEN_CURLY_CLOSE)
				break;

			while (1) {
				// Once we determine that there is a string where it ought to be,
				// we can create it with the parser via recursive call.
				if (tokens->identity != JSON_TOKEN_STRING) {
					fprintf(stderr, "The object expected a string (key), but this was not found! We received an object with key code %d\n", tokens->identity);
					json_free_all_the_tokens(&tokens);
					json_free_all_the_values(&elements);
					return NULL;
				}

				elements->key = json_parser(tokens);
				
			}
			break;
		case JSON_TOKEN_SQUARE_OPEN:
			break;
		case JSON_TOKEN_STRING:
			break;
		case JSON_TOKEN_NUMBER:
			break;
		case JSON_TOKEN_BOOLEAN:
			break;
		case JSON_TOKEN_NULL:
			break;
		default:
			fprintf(stderr, "Encountered an unexpected token!\n");
			json_free_all_the_tokens(&tokens);
			return NULL;
	}

	return elements;
}
