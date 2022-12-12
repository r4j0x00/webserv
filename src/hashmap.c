#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hashmap.h"

hashmap_t * hashmap_new() {
    hashmap_t * m = malloc(sizeof(hashmap_t));
    m->size = INITIAL_SIZE;
    m->entries = calloc(INITIAL_SIZE, sizeof(entry_t *));
    return m;
}

static int hashmap_hash(hashmap_t * in, char * key) {
    unsigned int hash = 0xdeadbeef;
    int c;
    char * x = key;
    while ((c = *key++))
        hash = hash * 31 + c;

    return hash % in->size;
}

entry_t * hashmap_entry_new(char * key, void * value, size_t size) {
    entry_t * e = malloc(sizeof(entry_t));
    e->key = strdup(key);
    e->value = malloc(size);
    memcpy(e->value, value, size);
    e->size = size;
    e->next = NULL;
    return e;
}

static void increase_size(hashmap_t * in) {
    int i;
    entry_t ** entries = in->entries;
    in->size *= 2;
    in->entries = calloc(in->size, sizeof(entry_t *));
    for(i = 0; i < in->size/2; i++) {
        entry_t * e = entries[i];
        if(e != NULL) {
            hashmap_put_entry(in, e);
            while(e->next != NULL) {
                e = e->next;
                hashmap_put_entry(in, e);
            }
        }
    }
    free(entries);
}

void hashmap_put_entry(hashmap_t * in, entry_t * e) {
    int bin = 0;
    int chain_length = 0;

    entry_t * next = NULL;
    entry_t * last = NULL;
    bin = hashmap_hash(in, e->key);
    next = in->entries[bin];

    while (next != NULL && next->key != NULL && strcmp(e->key, next->key) > 0) {
        last = next;
        next = next->next;
        chain_length++;
    }
    
    if(chain_length >= MAX_CHAIN_LENGTH) {
        increase_size(in);
        hashmap_put_entry(in, e);
        return;
    }

    entry_t * cur = last ? last : in->entries[bin];
    if(cur && !strcmp(e->key, cur->key)) {
        free(cur->value);
        cur->value = e->value;
        cur->size = e->size;
        return;
    }

    if(last == NULL) {
        e->next = in->entries[bin];
        in->entries[bin] = e;
    } else {
        e->next = next;
        last->next = e;
    }
}

void hashmap_put(hashmap_t * in, char * key, void * value, size_t size) {
    entry_t * e = hashmap_entry_new(key, value, size);
    hashmap_put_entry(in, e);
}

entry_t * hashmap_get(hashmap_t * in, char * key) {
    int bin = 0;
    entry_t * e = NULL;
    bin = hashmap_hash(in, key);
    e = in->entries[bin];
    while (e != NULL && e->key != NULL && strcmp(key, e->key) > 0) {
        e = e->next;
    }
    if(e == NULL || e->key == NULL || strcmp(key, e->key) != 0) {
        return NULL;
    } else {
        return e;
    }
}

void hashmap_iterate(hashmap_t * in, void (*f)(char * key, void * value, size_t size)) {
    int i;
    entry_t * e;
    for(i = 0; i < in->size; i++) {
        e = in->entries[i];
        while(e != NULL) {
            f(e->key, e->value, e->size);
            e = e->next;
        }
    }
}

entry_t * hashmap_first(hashmap_t * in) {
    int i;
    entry_t * e;
    for(i = 0; i < in->size; i++) {
        e = in->entries[i];
        if(e != NULL) {
            return e;
        }
    }
    return NULL;
}

entry_t * hashmap_next(hashmap_t * in, entry_t * e) {
    int i;
    int bin = 0;
    bin = hashmap_hash(in, e->key);
    if(e->next != NULL) {
        return e->next;
    }
    for(i = bin + 1; i < in->size; i++) {
        e = in->entries[i];
        if(e != NULL) {
            return e;
        }
    }
    return NULL;
}

void hashmap_free(hashmap_t * in) {
    int i;
    entry_t * e;
    entry_t * next;
    for(i = 0; i < in->size; i++) {
        e = in->entries[i];
        while(e != NULL) {
            next = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = next;
        }
    }
    free(in->entries);
    free(in);
}