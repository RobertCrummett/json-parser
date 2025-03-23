#define _CRT_SECURE_NO_WARNINGS
#include <stddef.h>
#include "json.h"

int main(int argc, char **argv) {
	// We are going to begin by reading the contents of a file into a string
	// If the returned data is NULL and the size is 0, then there was some
	// sort of error reading the file into the string.
	const char *path = "../../../share/ex05.json";

	json_value_t *json = json_load(path);

	if (json != NULL)
		json_print(json);
	return 0;
}
