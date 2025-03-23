#ifndef JSON_H
#define JSON_H

// This is the default size of the hash tables that store JSON objects.
// Tune to your needs.
#ifndef JSON_OBJECT_DEFAULT_SIZE
#define JSON_OBJECT_DEFAULT_SIZE 4
#endif

// Size of each input chunk to be read and allocated for by json_read_entire_file_to_cstr
// https://stackoverflow.com/questions/14002954/c-how-to-read-an-entire-file-into-a-buffer
#ifndef JSON_READ_ENTIRE_FILE_CHUNK
#define JSON_READ_ENTIRE_FILE_CHUNK 2097152
#endif

// The JSON value, once read into memory, will live in
// a conglomerate data structure of hash tables and arrays
// of the json_value_t. The dynamic arrays and hash
// table containers are defined below. They are pretty
// standard.
typedef struct json_value_t json_value_t;

extern json_value_t *json_load(const char *path);
extern void json_free(json_value_t **json);
extern void json_print(json_value_t *json);

#endif
