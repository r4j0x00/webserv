#ifndef MISC_H
#define MISC_H
typedef struct string {
    char *str;
    size_t len;
} string_t;

void init_cache();
string_t *read_file(char *);
string_t *read_file_cached(char *);
string_t * compress_data_gzip(char *, size_t);
string_t *string_new();
void string_append(string_t *, char *);
void string_append_data(string_t *, char *, size_t);
void string_free(string_t *);
string_t *string_from_data(char *, size_t);

#endif