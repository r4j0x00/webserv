#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <stdbool.h>
#include <signal.h>
#include "http.h"
#include "hashmap.h"
#include "sock.h"
#include "misc.h"

typedef struct conn {
    int fd;
    serv_ctx_t * ctx;
} conn_t;

static void send_response(int fd, char * response) {
    send(fd, response, strlen(response), 0);
}

static void send_response_string(int fd, string_t * response) {
    send(fd, response->str, response->len, 0);
}

static bool inline req_end(char * buf, size_t len) {
    if(len < 4)
        return false;
    for(int i = 0; i < len-3; ++i)
        if(buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
            return true;
    return false;
}

static char * read_from_fd(int fd) {
    char * buf = malloc(1024);
    size_t s = 0;
    ssize_t tmp = 1024;
    struct pollfd pfd = {fd, POLLIN, 0};

    while(tmp == 1024) {
        if(poll(&pfd, 1, 500) <= 0)
            break;
        tmp = recv(fd, buf+s, 1024, 0);
        if(tmp <= 0)
            break;
        s += tmp;
        buf = realloc(buf, s+1024);
        if(req_end(&buf[s-tmp], tmp))
            break;
    }

    if(s == 0) {
        free(buf);
        return NULL;
    }

    buf = realloc(buf, s+1);
    buf[s] = '\0';
    return buf;
}

char * query_get(struct http_request * req, char * key) {
    entry_t * e = hashmap_get(req->uri->query, key);
    return (e == NULL ? NULL : (char *)e->value);
}

char * header_get(struct http_request * req, char * key) {
    entry_t * e = hashmap_get(req->headers, key);
    return (e == NULL ? NULL : (char *)e->value);
}

char * post_get(struct http_request * req, char * key) {
    if(!req->post_data)
        return NULL;
    entry_t * e = hashmap_get(req->post_data, key);
    return (e == NULL ? NULL : (char *)e->value);
}

static char * decode_uri(char * uri) {
    size_t len = strlen(uri);
    char * buf = malloc(len+1);
    char tmp[3];

    int i, j;
    for(i = 0, j = 0; i < len; i++, j++) {
        if(uri[i] == '%') {
            if(i+2 >= len)
                break;
            tmp[0] = uri[i+1];
            tmp[1] = uri[i+2];
            tmp[2] = '\0';
            buf[j] = strtol(tmp, NULL, 16);
            if(buf[j] == '\0')
                break;
            i += 2;
        } else {
            buf[j] = uri[i];
        }
    }
    buf[j] = '\0';
    return buf;
}

static struct uri_t * parse_uri(char * uri) {
    char * saveptr;
    struct uri_t * u = malloc(sizeof(struct uri_t));
    uri = strdup(uri);
    u->query = hashmap_new();
    char *key, *value;
    char *path = strtok_r(uri, "?", &saveptr);
    u->path = strdup(path);
    while(1) {
        key = strtok_r(NULL, "=", &saveptr);
        value = strtok_r(NULL, "&", &saveptr);
        if(key == NULL || value == NULL)
            break;
        value = decode_uri(value);
        hashmap_put(u->query, key, (void *)value, strlen(value) + 1);
        free(value);
    }
    free(uri);
    return u;
}

static void update_post_data(struct http_request * req, char * buf) {
    char *key, *value, *saveptr;
    key = buf;
    while(1) {
        key = strtok_r(key, "=", &saveptr);
        value = strtok_r(NULL, "&", &saveptr);
        if(key == NULL || value == NULL)
            break;
        value = decode_uri(value);
        hashmap_put(req->post_data, key, (void *)value, strlen(value) + 1);
        free(value);
        key = NULL;
    }
}

static struct http_request * parse_http_request(conn_t *conn, char * buf) {
    char *key, *value, *tmp_buf, *saveptr;
    struct http_request * req = calloc(1, sizeof(struct http_request));
    req->headers = hashmap_new();

    req->method = strtok_r(buf, " ", &saveptr);
    tmp_buf = strtok_r(NULL, " ", &saveptr);
    if(!req->method || !tmp_buf)
        return NULL;

    req->uri = parse_uri(tmp_buf);
    req->version = strtok_r(NULL, "\r\n", &saveptr);

    if(!req->version)
        return NULL;

    // printf("Method: %s, URI: %s, Version: %s, Req: %d\n", req->method, req->uri->path, req->version, conn->ctx->req_count);

    while(1) {
        key = strtok_r(NULL, ": ", &saveptr);
        value = strtok_r(NULL, "\r\n", &saveptr);
        if(key == NULL || value == NULL)
            break;
 
        key = key + 1;
        while(*value == ' ' || *value == '\t')
            value++;
        hashmap_put(req->headers, (void *)key, value, strlen(value)+1);
    }

    if(strcmp(req->method, "POST") == 0) {
        req->post_data = hashmap_new();
        char *content_length = header_get(req, "Content-Length");
        if(content_length != NULL && key != NULL) {
            while (*key == '\r' || *key == '\n' || *key == ' ' || *key == '\t')
                key++;
            int cl = atoi(content_length);
            char * tmp = strdup(key);
            size_t tmp_len = strlen(tmp);
            tmp = (cl > tmp_len) ? realloc(tmp, cl+1) : tmp;
            while(cl > tmp_len) {
                ssize_t read_size = read(conn->fd, tmp+tmp_len, cl-tmp_len);
                if(read_size < 0) {
                    perror("read");
                    free(tmp);
                    return NULL;
                }
                tmp_len += read_size;
                tmp[tmp_len] = '\0';
            }
            update_post_data(req, tmp);
            free(tmp);
        }
    }

    return req;
}

static void free_http_request(struct http_request * req) {
    hashmap_free(req->uri->query);
    free(req->uri->path);
    free(req->uri);
    hashmap_free(req->headers);
    if(req->post_data)
        hashmap_free(req->post_data);
    free(req);
}

static char * get_date() {
    time_t t = time(NULL);
    struct tm * tm = gmtime(&t);
    char * buf = malloc(128);
    strftime(buf, 128, "%a, %d %b %Y %H:%M:%S %Z", tm);
    return buf;
}

static char * get_status_message(int status) {
    switch(status) {
        case 200:
            return "OK";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 505:
            return "HTTP Version Not Supported";
        default:
            return "Unknown";
    }
}

static struct http_response * build_http_response(int status, char * content_type) {
    struct http_response * resp = calloc(1, sizeof(struct http_response));
    resp->headers = hashmap_new();
    resp->status = status;
    resp->status_msg = get_status_message(status);
    resp->version = "HTTP/1.1";

    char * date = get_date();
    hashmap_put(resp->headers, "Date", date, strlen(date) + 1);
    hashmap_put(resp->headers, "Content-Type", content_type, strlen(content_type) + 1);
    hashmap_put(resp->headers, "Server", "webserv 1.0", 12);
    free(date);

    return resp;
}

static void free_http_response(struct http_response * resp) {
    hashmap_free(resp->headers);
    free(resp->body);
    free(resp);
}

char * get_mime_type(serv_ctx_t *ctx, char * file_name) {
    entry_t *entry;
    char * ext = strrchr(file_name, '.');
    
    if(ext == NULL)
        return "text/plain";
    int ext_len = strlen(ext);
    if(ext_len < 2)
        return "text/plain";

    entry = hashmap_get(ctx->mime_types, ext+1);
    if(entry == NULL)
        return "text/plain";

    return entry->value; 
}

struct http_response * get_static_response(serv_ctx_t *ctx, struct http_request * req) {
    struct http_response * resp;
    char *file_path, *file_name;
    int numsep = 0;

    char * path = req->uri->path;
    size_t path_len = strlen(path);

    if(path_len > 265)
        return build_http_response(404, "text/html");

    for(int i = 0; i < path_len; ++i) {
        if(path[i] == '/') {
            numsep++;
            if(numsep == 2)
                file_name = path + i + 1;
        }
        if(numsep > 2)
            break;
    }

    file_path = path + 1;
    if(numsep != 2 || !(*file_name)) {
        resp = build_http_response(404, "text/html");
        return resp;
    }

    string_t * file_data = read_file(file_path);
    if(file_data == NULL) {
        resp = build_http_response(404, "text/html");
        return resp;
    }

    resp = build_http_response(200, get_mime_type(ctx, file_name));
    resp->body = file_data->str;
    resp->body_len = file_data->len;
    free(file_data);

    return resp;
}

static void encode_response_body(struct http_request * req, struct http_response * resp) {
    if(resp->body == NULL)
        return;

    char * content_encoding = header_get(req, "Accept-Encoding");
    if(content_encoding == NULL)
        return;
    
    if(strstr(content_encoding, "gzip")) {
        string_t * encoded = compress_data_gzip(resp->body, resp->body_len);
        if(encoded == NULL)
            return;
        free(resp->body);
        resp->body = encoded->str;
        resp->body_len = encoded->len;
        free(encoded);
        hashmap_put(resp->headers, "Content-Encoding", "gzip", 5);
    }

    return;
}

static struct http_response * get_response(serv_ctx_t *ctx, struct http_request * req) {
    struct http_response * resp;
    char content_length[32];

    if(strcmp(req->method, "GET") && strcmp(req->method, "POST")) {
        resp = build_http_response(405, "text/html");
        hashmap_put(resp->headers, "Connection", "close", 6);
        return resp;
    }

    if(strcmp(req->version, "HTTP/1.1")) {
        resp = build_http_response(505, "text/html");
        hashmap_put(resp->headers, "Connection", "close", 6);
        return resp;
    }

    if(!strncmp(req->uri->path, "/static/", 8)) {
        resp = get_static_response(ctx, req);
        goto return_resp;
    }

    entry_t *handler_entry = hashmap_get(ctx->paths, req->uri->path);
    if(handler_entry == NULL) {
        resp = build_http_response(404, "text/html");
        resp->body = strdup("<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>");
        resp->body_len = strlen(resp->body);
    } else {
        resp = build_http_response(200, "text/html");
        void(**path_handler)(struct http_request *, struct http_response *) = handler_entry->value;
        (*path_handler)(req, resp);
    }

    return_resp:
    encode_response_body(req, resp);
    hashmap_put(resp->headers, "Content-Length", content_length, snprintf(content_length, sizeof(content_length), "%zu", resp->body_len) + 1);
    return resp;
}

static string_t * build_http_response_string(struct http_response * resp) {
    char status[32];
    snprintf(status, sizeof(status), "%d", resp->status);
    string_t * str = string_new();
    string_append(str, resp->version);
    string_append(str, " ");
    string_append(str, status);
    string_append(str, " ");
    string_append(str, resp->status_msg);
    string_append(str, "\r\n");

    entry_t * entry;
    for(entry = hashmap_first(resp->headers); entry != NULL; entry = hashmap_next(resp->headers, entry)) {
        string_append(str, entry->key);
        string_append(str, ": ");
        string_append(str, entry->value);
        string_append(str, "\r\n");
    }

    string_append(str, "\r\n");
    string_append_data(str, resp->body, resp->body_len);

    return str;
}


static void handle_http_request(conn_t * conn) {
    int fd = conn->fd;
    entry_t * entry;
    serv_ctx_t * ctx = conn->ctx;
    char * buf = NULL;
    int req_count = 0, close_conn = 0;

    while(!close_conn && (buf = read_from_fd(fd)) != NULL) {
        req_count++;
        struct http_request * req = parse_http_request(conn, buf);
        if(req) {
            struct http_response * response = get_response(ctx, req);
            
            char * connection = header_get(req, "Connection");
            if(connection && !strcmp(connection, "close"))
                hashmap_put(response->headers, "Connection", "close", 6);
            
            entry = hashmap_get(response->headers, "Connection");
            if(entry == NULL)
                hashmap_put(response->headers, "Connection", "keep-alive", 11);
            else if(!strcmp(entry->value, "close"))
                close_conn = 1;
            
            string_t * resp_str = build_http_response_string(response);
            send_response_string(fd, resp_str);

            string_free(resp_str);
            free_http_response(response);
            free_http_request(req);
        } else {
            send_response(fd, "HTTP/1.1 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>");
        }
        free(buf);
    };
    if (req_count > 1)
        printf("Connection closed after %d requests\n", req_count);
    free(conn);
    close(fd);
}

static void init_mime_types(serv_ctx_t *ctx) {
    ctx->mime_types = hashmap_new();
    hashmap_put(ctx->mime_types, "html", "text/html", 10);
    hashmap_put(ctx->mime_types, "css", "text/css", 9);
    hashmap_put(ctx->mime_types, "js", "application/javascript", 23);
    hashmap_put(ctx->mime_types, "png", "image/png", 10);
    hashmap_put(ctx->mime_types, "jpg", "image/jpeg", 11);
    hashmap_put(ctx->mime_types, "jpeg", "image/jpeg", 11);
    hashmap_put(ctx->mime_types, "gif", "image/gif", 10);
    hashmap_put(ctx->mime_types, "ico", "image/x-icon", 13);
    hashmap_put(ctx->mime_types, "svg", "image/svg+xml", 14);
    hashmap_put(ctx->mime_types, "ttf", "font/ttf", 9);
    hashmap_put(ctx->mime_types, "woff", "font/woff", 10);
    hashmap_put(ctx->mime_types, "woff2", "font/woff2", 11);
    hashmap_put(ctx->mime_types, "eot", "font/eot", 9);
    hashmap_put(ctx->mime_types, "otf", "font/otf", 9);
}

serv_ctx_t * create_server(int port) {
    signal(SIGPIPE, SIG_IGN);
    serv_ctx_t * ctx = malloc(sizeof(serv_ctx_t));
    ctx->sockfd = initialize_sock(port);
    ctx->req_count = 0;
    ctx->paths = hashmap_new();
    init_mime_types(ctx);
    return ctx;
}

void add_path(serv_ctx_t * ctx, char * path, void(*path_handler)(struct http_request *, struct http_response *)) {
    hashmap_put(ctx->paths, path, (void *)&path_handler, sizeof(void *));
}

void serve_forever(serv_ctx_t * ctx) {
    int fd;
    pthread_t thread;
    while (1) {
        fd = accept_new_conn(ctx->sockfd);
        conn_t * conn = malloc(sizeof(conn_t));
        conn->fd = fd;
        conn->ctx = ctx;
        pthread_create(&thread, NULL, (void *)handle_http_request, (void *)conn);
        // handle_http_request(conn);
        ctx->req_count++;
    }

    close(ctx->sockfd);
    hashmap_free(ctx->paths);
    hashmap_free(ctx->mime_types);
    free(ctx);
}