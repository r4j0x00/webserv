#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hashmap.h"
#include "http.h"
#include "misc.h"

void index_handler(struct http_request * req, struct http_response * res) {
    string_t * response = read_file_cached("static/index.html");
    res->body = response->str;
    res->body_len = response->len;
    free(response);
}

void test_handler(struct http_request * req, struct http_response * res) {
    char * response = malloc(1024);
    char * name = query_get(req, "name");
    char * browser = header_get(req, "User-Agent");
    res->body_len = snprintf(response, 1023, "<h1>Hello, %s!</h1><br>Browser: %s", (name == NULL ? "World" : name), (browser == NULL ? "Unknown" : browser));
    res->body = response;
}

void test_post_handler(struct http_request * req, struct http_response * res) {
    char * name = post_get(req, "name");
    if(!name) {
        string_t * response = read_file("static/form.html");
        res->body = response->str;
        res->body_len = response->len;
        free(response);
        return;
    }

    char * response = malloc(1024);
    snprintf(response, 1023, "<h1>Hello, %s!</h1>", name);
    res->body = response;
    res->body_len = strlen(res->body);

    return;
}

int main() {
    init_cache();
    serv_ctx_t *ctx = create_server(8080);
    add_path(ctx, "/", index_handler);
    add_path(ctx, "/test", test_handler);
    add_path(ctx, "/test_post", test_post_handler);
    puts("Listening on port 8080...");
    serve_forever(ctx);
    return 0;
}