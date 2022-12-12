#ifndef HASHMAP_H
#define HASHMAP_H
#include <sys/types.h>
typedef struct hashmap_entry {
    char * key;
    void * value;
    size_t size;
    struct hashmap_entry * next;
} entry_t;

typedef struct {
    int size;
    entry_t ** entries;
} hashmap_t;

#define INITIAL_SIZE (0x400)
#define MAX_CHAIN_LENGTH (8)

hashmap_t * hashmap_new();
void hashmap_free(hashmap_t *);
void hashmap_put(hashmap_t *, char *, void *, size_t);
entry_t * hashmap_get(hashmap_t *, char *);
static int hashmap_hash(hashmap_t *, char *);
entry_t * hashmap_entry_new(char *, void *, size_t);
static void increase_size(hashmap_t *);
void hashmap_iterate(hashmap_t *, void (*f)(char *, void *, size_t));
void hashmap_put_entry(hashmap_t *, entry_t *);
entry_t * hashmap_first(hashmap_t *);
entry_t * hashmap_next(hashmap_t *, entry_t *);

#endif