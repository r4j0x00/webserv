#ifndef HTTP_H
#define HTTP_H
#include "hashmap.h"

typedef struct serv_ctx {
    int sockfd;
    int req_count;
    hashmap_t *mime_types;
    hashmap_t *paths;
} serv_ctx_t;

struct uri_t {
    char *path;
    hashmap_t *query;
};

struct http_request {
    char *method;
    struct uri_t *uri;
    char *version;
    hashmap_t *headers;
    hashmap_t *post_data;
};

struct http_response {
    char *version;
    int status;
    char *status_msg;
    hashmap_t *headers;
    char *body;
    size_t body_len;
};

void add_path(serv_ctx_t *, char *, void(*)(struct http_request *, struct http_response *));
serv_ctx_t * create_server(int);
void serve_forever(serv_ctx_t *);
char * query_get(struct http_request *, char *);
char * header_get(struct http_request *, char *);
char * post_get(struct http_request *, char *);

#endif