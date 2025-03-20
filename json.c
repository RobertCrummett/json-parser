#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

// This is the JSON lexer. Its job is to translate
// the string data in `json_data` into a token array,
// `token_array`.
json_token_array_t *json_lexer(const char *json_data, size_t json_size) {
	// Allocate space for an empty token array that will
	// hold all of the tokens in the JSON string
	json_token_array_t *tokens = malloc(sizeof *tokens);
	if (tokens == NULL) {
		fprintf(stderr, "Could not allocate space for the token array(%zu bytes): %s\n", sizeof *tokens, strerror(errno));
		return NULL;
	}
	tokens->count = 0;
	tokens->capacity = JSON_TOKEN_ARRAY_DEFAULT_SIZE;

	// Allocate some internal space in the array to put
	// our tokens. This will be expanded dynamically in the
	// event that the array fills up.
	tokens->token = malloc(JSON_TOKEN_ARRAY_DEFAULT_SIZE * sizeof *tokens->token);
	if (tokens->token == NULL) {
		fprintf(stderr, "Could not allocate the default number (%d) numbers of tokens (each of size %zu bytes): %s\n", JSON_TOKEN_ARRAY_DEFAULT_SIZE, sizeof *tokens->token, strerror(errno));
		free(tokens);
		return NULL;
	}

	// Now that space has been allocated for the tokens, it
	// is time to read the string data carefully and identify all of
	// our tokens.
	size_t position = 0;
	size_t line = 1;
	char *line_start = (char *)json_data;
	while (*json_data != '\0' && position < json_size) {
		position++;
		char current = *json_data++;
		json_token_t current_token = {0};

		// Because the JSON can be lexed character by character,
		// that is simply what I will do! This switch case will
		// encode all of my logic.
		switch (current) {
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
				current_token.identity = JSON_TOKEN_INTEGER;
				current_token.start = (char *)json_data - 1;

				// The first four conditions being satisified means 
				// that we are dealing with some sort of fraction 
				// or exponential value. We just fall through in this
				// case.
				//
				// Otherwise, we are dealing with an ordinary
				// number until we hit something weird like a
				// '.', 'e', or 'E', or end of JSON number
				if (current == '-' && *json_data == '0')
					;
				else if (current == '0' && *json_data == '.')
					;
				else if (current == '0' && *json_data == 'e')
					;
				else if (current == '0' && *json_data == 'E')
					;
				else if (current >= '1' && current <= '9')
					// Loop over digits until non digit is found
					if (*json_data >= '0' && *json_data <= '9')
						json_data++;
				
				// Question: are you a fraction?
				if (*json_data == '.') {
					current_token.identity = JSON_TOKEN_DOUBLE;
					json_data++;
					
					// Loop over digits until non digit is found
					while (*json_data >= '0' && *json_data <= '9')
						json_data++;
				}

				// Question: are you an exponential?
				if (*json_data == 'E' || *json_data == 'e') {
					current_token.identity = JSON_TOKEN_DOUBLE;
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
				current_token.identity = JSON_TOKEN_BOOL;
				current_token.start = (char *)json_data - 1;

				// The end offset will depend on whether the
				// boolean value is true (+3) or false (+4)
				if (current == 't')
					json_data += 3;
				else if (current == 'f')
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
				free(tokens->token);
				free(tokens);
				return NULL;
		}

		// Now that the token hash been created, we need to store it in
		// the array of tokens. First check that there is room in the array
		// to store it in.
		if (tokens->count >= tokens->capacity) {
			if (tokens->capacity == 0)
				tokens->capacity = JSON_TOKEN_ARRAY_DEFAULT_SIZE;
			else
				tokens->capacity *= 2;

			tokens->token = realloc(tokens->token, sizeof *tokens->token * tokens->capacity);

			if (tokens->token == NULL) {
				fprintf(stderr, "Failed to reallocate internal dynamic array of tokens during lexing to size %zu bytes: %s\n", 2 * sizeof *(tokens->token) * tokens->capacity, strerror(errno));
				free(tokens->token);
				free(tokens);
				return NULL;
			}
		}

		tokens->token[tokens->count++] = current_token;
	}

	return tokens;
}


void json_print(json_token_array_t *tokens) {
	// For each token in the tokens array, match its identity to
	// the proper print statement.
	for (size_t i = 0; i < tokens->count; i++)
		switch (tokens->token[i].identity) {
			case JSON_TOKEN_STRING:
				printf("\"%.*s\"", JSON_FMT_TOKEN(tokens->token[i]));
				break;
			case JSON_TOKEN_INTEGER:
			case JSON_TOKEN_DOUBLE:
			case JSON_TOKEN_CURLY_OPEN:
			case JSON_TOKEN_CURLY_CLOSE:
			case JSON_TOKEN_SQUARE_OPEN:
			case JSON_TOKEN_SQUARE_CLOSE:
			case JSON_TOKEN_COLON:
			case JSON_TOKEN_COMMA:
			case JSON_TOKEN_BOOL:
			case JSON_TOKEN_NULL:
			case JSON_TOKEN_WHITESPACE:
				printf("%.*s", JSON_FMT_TOKEN(tokens->token[i]));
				break;
			default:
				fprintf(stderr, "\nUnexpected identity code recieved: %d\n", tokens->token[i].identity);
				return;
		}
}


void json_free_token_array(json_token_array_t **tokens) {
	// Make sure we do not dereference a NULL pointer accidentially
	if (tokens == NULL || *tokens == NULL)
		return;
	// Free the internal data if it exists on the heap
	if ((*tokens)->token != NULL)
		free((*tokens)->token);
	// Free the token array and point it towards zero
	free(*tokens);
	*tokens = NULL;
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

json_value_t *json_parser(json_token_array_t *tokens) {
	// For each token in the tokens array, match its identity to
	// the proper print statement.
	for (size_t i = 0; i < tokens->count; i++)
		switch (tokens->token[i].identity) {
			case JSON_TOKEN_STRING:
				printf("\"%.*s\"", JSON_FMT_TOKEN(tokens->token[i]));
				break;
			case JSON_TOKEN_INTEGER:
			case JSON_TOKEN_DOUBLE:
			case JSON_TOKEN_CURLY_OPEN:
			case JSON_TOKEN_CURLY_CLOSE:
			case JSON_TOKEN_SQUARE_OPEN:
			case JSON_TOKEN_SQUARE_CLOSE:
			case JSON_TOKEN_COLON:
			case JSON_TOKEN_COMMA:
			case JSON_TOKEN_BOOL:
			case JSON_TOKEN_NULL:
			case JSON_TOKEN_WHITESPACE:
				printf("%.*s", JSON_FMT_TOKEN(tokens->token[i]));
				break;
			default:
				fprintf(stderr, "\nUnexpected identity code recieved: %d\n", tokens->token[i].identity);
				return;
		}
	return NULL;
}
