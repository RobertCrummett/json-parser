#include <stddef.h>
#include "json.h"

int main(void) {
	/* We are going to begin by reading the contents of a file into a string
	   If the returned data is NULL and the size is 0, then there was some
	   sort of error reading the file into the string. */
	const char *path = "share/ex04.json";

	json_value_t *json = json_load(path);

	if (json != NULL)
		json_print(json);

	json_free(&json);
	return 0;
}
