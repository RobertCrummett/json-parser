#ifndef JSON_H
#define JSON_H

#include <stdio.h>

typedef enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_NULL
} json_type;

typedef struct json_value json_value;

char *json_read_entire_file_to_cstr(const char* path);

json_value *json_from_string(const char *string);
json_value *json_from_file(const char *path);

char* json_to_string(json_value *v);
void json_print(FILE* stream, json_value *v);
void json_pretty_print(FILE* stream, json_value *v);

void json_free(json_value **root);

json_value *json_get(json_value *obj, const char *key);
json_value *json_geti(json_value *arr, size_t index);
void json_set(json_value *obj, const char *key, json_value *val);
void json_seti(json_value *arr, size_t index, json_value *val);

json_value *json_new_string(const char *s);
json_value *json_new_number(double n);
json_value *json_new_boolean(int b);
json_value *json_new_null(void);
json_value *json_new_object(void);
json_value *json_new_array(void);

json_type json_query_type(json_value *v);
char *json_query_string(json_value *v);
double json_query_number(json_value *v);
int json_query_boolean(json_value *v);

#endif // JSON_H

#ifdef JSON_IMPLEMENTATION

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_DEFAULT_ARRAY_SIZE 4
#define JSON_DEFAULT_OBJECT_SIZE 4
#define JSON_READ_ENTIRE_FILE_CHUNK (1024*1024)

typedef struct json_value json_value;

typedef struct {
    char *key;
    json_value *val;
} kvp;

struct json_object {
    size_t count;
    size_t cap;
    kvp *items;
};

struct json_array {
    size_t count;
    size_t cap;
    json_value **items;
};

struct json_value {
    json_type type;
    union {
        struct json_object object;
        struct json_array array;
        char *string;
        double number;
        int boolean;
    };
};

static const char *json_buffer = {0};

json_value *json__parse_value();

static void json__skip_whitespace() {
    while(isspace(*json_buffer))
        json_buffer++;
}

static json_value *json__new_value(json_type t) {
    json_value *x = malloc(sizeof* x);
    x->type = t;
    if (t == JSON_OBJECT) {
        x->object.count = 0;
        x->object.cap = 4;
        x->object.items = malloc(JSON_DEFAULT_OBJECT_SIZE*sizeof *x->object.items);
    }
    if (t == JSON_ARRAY) {
        x->array.count = 0;
        x->array.cap = 4;
        x->array.items = malloc(JSON_DEFAULT_ARRAY_SIZE*sizeof *x->array.items);
    }
    return x;
}

static char *json__parse_string() {
    json_buffer++;
    const char *s = json_buffer;
    while(*json_buffer!='"' && !(*json_buffer == '\\' && *(json_buffer+1) == '"'))
        json_buffer++;
    size_t l = json_buffer - s;
    char* x = malloc(l + 1);
    for(size_t i = 0; i < l; i++)
        x[i] = s[i];
    x[l] = '\0';
    json_buffer++;
    return x;
}

static double json__parse_number() {
    char *e;
    double v = strtod(json_buffer, &e);
    json_buffer = e;
    return v;
}

static json_value *json__parse_array() {
    json_buffer++;
    json_value *x = json__new_value(JSON_ARRAY);
    json__skip_whitespace();
    if(*json_buffer == ']') {
        json_buffer++;
        return x;
    }
    while (1) {
        json__skip_whitespace();
        json_value *e = json__parse_value();
        x->array.items[x->array.count++] = e;
        if(x->array.count == x->array.cap)
            x->array.items = realloc(x->array.items,(x->array.cap *= 2)*sizeof*x->array.items);
        json__skip_whitespace();
        if(*json_buffer == ',') {
            json_buffer++;
            continue;
        } if (*json_buffer == ']') {
            json_buffer++;
            break;
        }
    }
    return x;
}

static json_value *json__parse_object() {
    json_buffer++;
    json_value *x = json__new_value(JSON_OBJECT);
    json__skip_whitespace();
    if(*json_buffer == '}') {
        json_buffer++;
        return x;
    }
    while (1) {
        json__skip_whitespace();

        if(*json_buffer != '"')
            return NULL;

        char *k = json__parse_string();
        json__skip_whitespace();

        if (*json_buffer != ':')
            return NULL;
        json_buffer++;
        json__skip_whitespace();
        json_value *v = json__parse_value();
        if(x->object.count == x->object.cap)
            x->object.items = realloc(x->object.items, (x->object.cap *= 2)*sizeof *x->object.items);
        x->object.items[x->object.count++] = (kvp){k,v};
        json__skip_whitespace();
        if(*json_buffer == ','){
            json_buffer++;
            continue;
        } 
        if (*json_buffer == '}') {
            json_buffer++;
            break;
        }
    }
    return x;
}

json_value *json__parse_value() {
    json__skip_whitespace();
    if (*json_buffer == '"') {
        char *s = json__parse_string();
        json_value *v = json__new_value(JSON_STRING);
        v->string = s;
        return v;
    }
    if (*json_buffer == '{')
        return json__parse_object();
    if(*json_buffer == '[')
        return json__parse_array();
    if(strncmp(json_buffer, "true", 4) == 0) {
        json_buffer += 4;
        json_value *v = json__new_value(JSON_BOOL);
        v->boolean = 1;
        return v;
    }
    if(strncmp(json_buffer, "false", 5) == 0) {
        json_buffer += 5;
        json_value *v = json__new_value(JSON_BOOL);
        v->boolean = 0;
        return v;
    }
    if (strncmp(json_buffer, "null", 4) == 0) {
        json_buffer+=4;
        return json__new_value(JSON_NULL);
    }
    if(*json_buffer == '-' || isdigit(*json_buffer)) {
        double n = json__parse_number();
        json_value *v = json__new_value(JSON_NUMBER);
        v->number = n;
        return v;
    }
    return NULL;
}

char *json_read_entire_file_to_cstr(const char* path) {
    char *data = NULL;

    FILE *fin = fopen(path, "rb");
    if (fin == NULL) {
        fprintf(stderr, "error, failed to open %s: %s:%d\n", path, __FILE__, __LINE__);
        return NULL;
    }

    char *temp = NULL;
    size_t n = 0;
    size_t size = 0;
    size_t used = 0;
    while (1) {
        if (used + JSON_READ_ENTIRE_FILE_CHUNK + 1 > size) {
            size = used + JSON_READ_ENTIRE_FILE_CHUNK + 1;

            if (size <= used) {
                if (data != NULL)
                    free(data);
                fclose(fin);
                fprintf(stderr, "error, failed reading %s into memory becase the file size overflowed the size_t type\n", path);
                return NULL;
            }

            temp = realloc(data, size);
            if (temp == NULL) {
                if (data != NULL)
                    free(data);
                fclose(fin);
                fprintf(stderr, "error, failed to reallocate data (size %zu bytes) to %zu byte container\n", sizeof *data, size);
                return NULL;
            }
            data = temp;
        }

        n = fread(data + used, 1, JSON_READ_ENTIRE_FILE_CHUNK, fin);
        if (ferror(fin)) {
            if (data != NULL)
                free(data);
            fclose(fin);
            fprintf(stderr, "error, failed to read chunk of %s into data\n", path);
            return NULL;
        }

        used += n;

        if (feof(fin))
            break;
    }
    fclose(fin);

    temp = realloc(data, used+1);
    if (temp == NULL) {
        if (data != NULL)
            free(data);
        fprintf(stderr, "error, failed to reallocate data (size %zu bytes) to %zu byte container\n", sizeof *data, size);
        return NULL;
    }
    data = temp;
    data[used] = '\0';
    return data;
}

json_value *json_from_string(const char *string) {
    json_buffer = string;
    return json__parse_value();
}

json_value *json_from_file(const char *path) {
    char *string = json_read_entire_file_to_cstr(path);
    if (!string)
        return NULL;
    json_buffer = string;
    json_value *value = json__parse_value();
    free(string);
    json_buffer = NULL;
    return value;
}

void json_free(json_value **root) {
    if(!root || !*root)
        return; 

    json_value *r = *root;
    switch (r->type) {
        case JSON_OBJECT:
            for(size_t i = 0; i < r->object.count; i++) {
                free(r->object.items[i].key);
                json_free(&r->object.items[i].val);
            }
            free(r->object.items);
            break;
        case JSON_ARRAY:
            for(size_t i = 0; i < r->array.count; i++)
                json_free(&r->array.items[i]);
            free(r->array.items);
            break;
        case JSON_STRING:
            free(r->string);
            break;
        default:
            break;
    }
    free(r);
    *root = NULL;
}

void json_print(FILE *stream, json_value *v) {
    if (!v)
        return;

    switch (v->type) {
        case JSON_OBJECT:
            fprintf(stream, "{");
            for(size_t i = 0; i < v->object.count; i++) {
                fprintf(stream, "\"%s\":",v->object.items[i].key);
                json_print(stream, v->object.items[i].val);
                if(i+1 < v->object.count)
                    fprintf(stream, ",");
            }
            fprintf(stream, "}");
            break;
        case JSON_ARRAY:
            fprintf(stream, "[");
            for(size_t i = 0; i < v->array.count; i++) {
                json_print(stream, v->array.items[i]);
                if(i + 1 < v->array.count)
                    fprintf(stream, ",");
            }
            fprintf(stream, "]");
            break;
        case JSON_STRING:
            fprintf(stream, "\"%s\"", v->string);
            break;
        case JSON_NUMBER:
            fprintf(stream, "%g", v->number);
            break;
        case JSON_BOOL:
            fprintf(stream, v->boolean ? "true" : "false");
            break;
        case JSON_NULL:
            fprintf(stream, "null");
            break;
    }
}

static void json__pretty_printer(FILE* stream, json_value *v, int indent_level) {
    if (!v)
        return;

#define PRINT_INDENT for(int i=0; i<indent_level; i++) fprintf(stream, "    ")

    switch (v->type) {
        case JSON_OBJECT:
            fprintf(stream, "{\n");
            for(size_t i = 0; i < v->object.count; i++) {
                PRINT_INDENT;
                fprintf(stream, "    \"%s\": ", v->object.items[i].key);
                json__pretty_printer(stream, v->object.items[i].val, indent_level + 1);
                if(i + 1 < v->object.count)
                    fprintf(stream, ",");
                fprintf(stream, "\n");
            }
            PRINT_INDENT;
            fprintf(stream, "}");
            break;
        case JSON_ARRAY:
            fprintf(stream, "[\n");
            for(size_t i = 0; i < v->array.count; i++) {
                PRINT_INDENT;
                fprintf(stream, "    ");
                json__pretty_printer(stream, v->array.items[i], indent_level + 1);
                if(i + 1 < v->array.count)
                    fprintf(stream, ",");
                fprintf(stream, "\n");
            }
            PRINT_INDENT;
            fprintf(stream, "]");
            break;
        case JSON_STRING:
            fprintf(stream, "\"%s\"", v->string);
            break;
        case JSON_NUMBER:
            fprintf(stream, "%g", v->number);
            break;
        case JSON_BOOL:
            fprintf(stream, v->boolean ? "true" : "false");
            break;
        case JSON_NULL:
            fprintf(stream, "null");
            break;
    }

    #undef PRINT_INDENT
}

void json_pretty_print(FILE* stream, json_value *v) {
    json__pretty_printer(stream, v, 0);
    fprintf(stream, "\n");
}

static void json__append_string(char **buffer, size_t *buf_size, size_t *pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int needed = vsnprintf(NULL, 0, fmt, args);
    if (needed < 0) {
        va_end(args);
        return;
    }

    if (*pos + needed + 1 > *buf_size) {
        *buf_size = *pos + needed + 1;
        *buffer = realloc(*buffer, *buf_size);
        if (!*buffer) {
            va_end(args);
            return;
        }
    }

    vsnprintf(*buffer + *pos, *buf_size - *pos, fmt, args);
    *pos += needed;
    va_end(args);
}

static void json__to_string_helper(json_value *v, char **buffer, size_t *buf_size, size_t *pos) {
    if (!v)
        return;

    switch (v->type) {
        case JSON_OBJECT:
        json__append_string(buffer, buf_size, pos, "{");
        for (size_t i = 0; i < v->object.count; i++) {
                json__append_string(buffer, buf_size, pos, "\"%s\":", v->object.items[i].key);
                json__to_string_helper(v->object.items[i].val, buffer, buf_size, pos);
                if (i + 1 < v->object.count)
                    json__append_string(buffer, buf_size, pos, ",");
            }
            json__append_string(buffer, buf_size, pos, "}");
            break;
        case JSON_ARRAY:
            json__append_string(buffer, buf_size, pos, "[");
            for (size_t i = 0; i < v->array.count; i++) {
                json__to_string_helper(v->array.items[i], buffer, buf_size, pos);
                if (i + 1 < v->array.count)
                    json__append_string(buffer, buf_size, pos, ",");
            }
            json__append_string(buffer, buf_size, pos, "]");
            break;
        case JSON_STRING:
            json__append_string(buffer, buf_size, pos, "\"%s\"", v->string);
            break;
        case JSON_NUMBER:
            json__append_string(buffer, buf_size, pos, "%g", v->number);
            break;
        case JSON_BOOL:
            json__append_string(buffer, buf_size, pos, v->boolean ? "true" : "false");
            break;
        case JSON_NULL:
            json__append_string(buffer, buf_size, pos, "null");
            break;
    }
}

char* json_to_string(json_value *v) {
    if (!v)
        return NULL;

    size_t buf_size = 256;
    char *buffer = malloc(buf_size);
    if (!buffer)
        return NULL;

    size_t pos = 0;
    json__to_string_helper(v, &buffer, &buf_size, &pos);

    if (pos >= buf_size) {
        buffer = realloc(buffer, pos + 1);
        if (!buffer)
            return NULL;
    }
    buffer[pos] = '\0';

    return buffer;
}

json_value *json_get(json_value *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT)
        return NULL;

    for (size_t i = 0; i < obj->object.count; i++)
        if (strcmp(obj->object.items[i].key, key) == 0)
            return obj->object.items[i].val;

    return NULL;
}

json_value *json_geti(json_value *arr, size_t index) {
    if (!arr || arr->type != JSON_ARRAY)
        return NULL;

    if (index >= arr->array.count)
        return NULL;

    return arr->array.items[index];
}

void json_set(json_value *obj, const char *key, json_value *val) {
    if (!obj || obj->type != JSON_OBJECT)
        return;
    
    for (size_t i = 0; i < obj->object.count; i++)
        if (strcmp(obj->object.items[i].key, key) == 0) {
            json_free(&obj->object.items[i].val);
            obj->object.items[i].val = val;
            return;
        }
    
    if (obj->object.count == obj->object.cap)
        obj->object.items = realloc(obj->object.items, (obj->object.cap *= 2) * sizeof(kvp));
    obj->object.items[obj->object.count++] = (kvp){strdup(key), val};
}

void json_seti(json_value *arr, size_t index, json_value *val) {
    if (!arr || arr->type != JSON_ARRAY)
        return;

    if (index >= arr->array.count)
        return;

    json_free(&arr->array.items[index]);

    arr->array.items[index] = val;
}

json_value *json_new_string(const char *s) {
    json_value *v = json__new_value(JSON_STRING);
    v->string = strdup(s);
    return v;
}

json_value *json_new_number(double n) {
    json_value *v = json__new_value(JSON_NUMBER);
    v->number = n;
    return v;
}

json_value *json_new_boolean(int b) {
    json_value *v = json__new_value(JSON_BOOL);
    v->boolean = b ? 1 : 0;
    return v;
}

json_value *json_new_null(void) {
    return json__new_value(JSON_NULL);
}

json_value *json_new_object(void) {
    return json__new_value(JSON_OBJECT);
}

json_value *json_new_array(void) {
    return json__new_value(JSON_ARRAY);
}

json_type json_query_type(json_value *v) {
    return v->type;
}

char *json_query_string(json_value *v) {
    return v->string;
}

double json_query_number(json_value *v) {
    return v->number;
}

int json_query_boolean(json_value *v) {
    return v->boolean;
}

#endif // JSON_IMPLEMENTATION
