#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include "hashmap.h"
#include "misc.h"

hashmap_t * cache = NULL;

void init_cache() {
    if(cache == NULL)
        cache = hashmap_new();
}

string_t * read_file_cached(char * filename) {
    entry_t * entry = hashmap_get(cache, filename);
    if(entry)
        return string_from_data(entry->value, entry->size);
    string_t * s = read_file(filename);
    hashmap_put(cache, filename, s->str, s->len);
    return s;
}

string_t * read_file(char * filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }
    struct stat st;
    fstat(fd, &st);
    char * buf = calloc(1, st.st_size + 1);
    read(fd, buf, st.st_size);
    close(fd);
    string_t * s = string_from_data(buf, st.st_size);
    free(buf);
    return s;
}

string_t * compress_data_gzip(char * data, size_t len) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if(deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return NULL;

    strm.avail_in = len;
    strm.next_in = (Bytef *)data;
    strm.avail_out = compressBound(len) + 0x100;
    char * enc = malloc(strm.avail_out);
    strm.next_out = (Bytef *)enc;

    if(strm.next_out == NULL)
        return NULL;
    
    if(deflate(&strm, Z_FINISH) != Z_STREAM_END) {
        free(strm.next_out);
        return NULL;
    }

    string_t * out = string_from_data(enc, strm.total_out);
    free(enc);
    deflateEnd(&strm);

    return out;
}

string_t * string_new() {
    string_t * s = malloc(sizeof(string_t));
    s->len = 0;
    s->str = NULL;
    return s;
}

void string_append(string_t * s, char * str) {
    size_t len = strlen(str);
    s->str = realloc(s->str, s->len + len + 1);
    memcpy(s->str + s->len, str, len);
    s->len += len;
    s->str[s->len] = '\0';
}

void string_append_data(string_t * s, char * data, size_t len) {
    s->str = realloc(s->str, s->len + len + 1);
    memcpy(s->str + s->len, data, len);
    s->len += len;
    s->str[s->len] = '\0';
}

string_t * string_from_data(char * data, size_t len) {
    string_t * s = malloc(sizeof(string_t));
    s->len = len;
    s->str = malloc(len + 1);
    memcpy(s->str, data, len);
    s->str[len] = '\0';
    return s;
}

void string_free(string_t * s) {
    free(s->str);
    free(s);
}