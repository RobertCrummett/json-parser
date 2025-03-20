#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

int main(int argc, char **argv) {
	// We are going to begin by reading the contents of a file into a string
	// If the returned data is NULL and the size is 0, then there was some
	// sort of error reading the file into the string.
	char *json_path = {0};
	if (argc == 2)
		json_path = argv[1];
	else
		json_path = "share/ex01.json";

	char *json_data = NULL;
	size_t json_size = 0;

	json_read_entire_file_to_cstr(json_path, &json_data, &json_size);
	if (json_data == NULL || json_size == 0) {
		fprintf(stderr, "Faild to read %s into memory: %s\n", json_path, strerror(errno));
		return 1;
	}

	// Tokenize the JSON string and output the contents into the token_array.
	// This function may fail while resizing the JSON token array.
	// If it does, it will a null array.
	json_token_t *tokens = json_lexer(json_data, json_size);
	if (tokens == NULL) {
		fprintf(stderr, "The JSON lexer failed to resize the dynamic array!\n");
		return 1;
	}
	
	// Print the value of the tokens to the standard output
	json_print(tokens);

	// Cleanup the token array at the end
	json_free_all_the_tokens(&tokens);

	return 0;
}
