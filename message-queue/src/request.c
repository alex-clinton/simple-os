/* request.c: Request structure */

#include "mq/request.h"

#include <stdlib.h>
#include <string.h>

/**
 * Create Request structure.
 * @param   method      Request method string.
 * @param   uri         Request uri string.
 * @param   body        Request body string.
 * @return  Newly allocated Request structure.
 */
Request * request_create(const char *method, const char *uri, const char *body) {
    Request* new_request = calloc(1, sizeof(Request));

    if(!new_request || !method || !uri) {
        return NULL;
    }
    new_request->method = strdup(method);
    new_request->uri = strdup(uri);

    if(body) {
        new_request->body = strdup(body);
    }

    return new_request;
}

/**
 * Delete Request structure.
 * @param   r           Request structure.
 */
void request_delete(Request *r) {
    if(r) {
        if(r->method) {
            free(r->method);
        }
        if(r->uri) {
            free(r->uri);
        }
        if(r->body) {
            free(r->body);
        }
        free(r);
    }
}

/**
 * Write HTTP Request to stream:
 *
 *  $METHOD $URI HTTP/1.0\r\n
 *  Content-Length: Length($BODY)\r\n
 *  \r\n
 *  $BODY
 *
 * @param   r           Request structure.
 * @param   fs          Socket file stream.
 */
void request_write(Request *r, FILE *fs) {
    if(r->body) {
        int content_length = strlen(r->body);
        fprintf(fs, "%s %s HTTP/1.0\r\n", r->method, r->uri);
        fprintf(fs, "Content-Length: %d\r\n", content_length);
        fprintf(fs, "\r\n");
        fprintf(fs, "%s", r->body);
    }
    else {
        fprintf(fs, "%s %s HTTP/1.0\r\n", r->method, r->uri);
        fprintf(fs, "\r\n");
    }
}