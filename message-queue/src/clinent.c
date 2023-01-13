/* client.c: Message Queue Client */

#include "mq/client.h"
#include "mq/logging.h"
#include "mq/socket.h"
#include "mq/string.h"

/* Internal Constants */

#define SENTINEL "SHUTDOWN"

/* Internal Prototypes */

void * mq_pusher(void *);
void * mq_puller(void *);

/* External Functions */

/**
 * Create Message Queue withs specified name, host, and port.
 * @param   name        Name of client's queue.
 * @param   host        Address of server.
 * @param   port        Port of server.
 * @return  Newly allocated Message Queue structure.
 */
MessageQueue * mq_create(const char *name, const char *host, const char *port) {

    MessageQueue* mq = calloc(1, sizeof(MessageQueue));
    // Checking for valid arguments and that calloc succeeded
    if(!mq || !name || !host || !port) {
        return NULL;
    }

    // Put arguments into mq
    strcpy(mq->name, name);
    strcpy(mq->host, host);
    strcpy(mq->port, port);
    // Create the incoming and outgoing queues
    mq->outgoing = queue_create();
    mq->incoming = queue_create();
    // Check that the queues were successfully created
    if(!mq->outgoing || !mq->incoming) {
        return NULL;
    }
    return mq;
}

/**
 * Delete Message Queue structure (and internal resources).
 * @param   mq      Message Queue structure.
 */
void mq_delete(MessageQueue *mq) {
    // Ensure mq is valid
    if(!mq) {
        return;
    }
    // Delete the incoming and outgoing queues
    queue_delete(mq->incoming);
    queue_delete(mq->outgoing);
    // Free the mq structure
    free(mq);
}

/**
 * Publish one message to topic (by placing new Request in outgoing queue).
 * @param   mq      Message Queue structure.
 * @param   topic   Topic to publish to.
 * @param   body    Message body to publish.
 */
void mq_publish(MessageQueue *mq, const char *topic, const char *body) {
    // Build message to send
    char* method = "PUT";
    char concat_buf[BUFSIZ] = "/topic/";
    strcat(concat_buf, topic);
    // Create new request and put it in the outgoing queue
    Request* new_request = request_create(method, concat_buf, body);
    queue_push(mq->outgoing, new_request);
}

/**
 * Retrieve one message (by taking Request from incoming queue).
 * @param   mq      Message Queue structure.
 * @return  Newly allocated message body (must be freed).
 */
char * mq_retrieve(MessageQueue *mq) {
    // Retrieve the next incoming request
    Request *r = queue_pop(mq->incoming);
    // Str dup and return non sentinel messages
    if(r) {
        if(r->body) {
            if(!strstr(r->body, SENTINEL)) {
                // Delete the request after we have extracted the body
                char* msg = strdup(r->body);
                request_delete(r);
                return msg;
            }
        }
        request_delete(r);
    }
    return NULL;
}

/**
 * Subscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to subscribe to.
 **/
void mq_subscribe(MessageQueue *mq, const char *topic) {
    // Build message to send
    char* method = "PUT";
    char concat_buf[BUFSIZ] = "/subscription/";
    strcat(concat_buf, mq->name);
    strcat(concat_buf, "/");
    strcat(concat_buf, topic);
    // Create the request and put it in the outgoing queue
    Request* new_request = request_create(method, concat_buf, NULL);
    queue_push(mq->outgoing, new_request);
}

/**
 * Unubscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to unsubscribe from.
 **/
void mq_unsubscribe(MessageQueue *mq, const char *topic) {
    // Build message to send
    char * method = "DELETE";
    char buffer[BUFSIZ] = "/subscription/";
    strcat(buffer,mq->name);
    strcat(buffer,"/");
    strcat(buffer, topic);
    Request *r = request_create(method,buffer,NULL);
    queue_push(mq->outgoing,r);
}

/**
 * Start running the background threads:
 *  1. First thread should continuously send requests from outgoing queue.
 *  2. Second thread should continuously receive reqeusts to incoming queue.
 * @param   mq      Message Queue structure.
 */
void mq_start(MessageQueue *mq) {
    mq_subscribe(mq, SENTINEL);
    thread_create(&mq->thread1, NULL, mq_pusher, (void*)mq);
    thread_create(&mq->thread2, NULL, mq_puller, (void*)mq);
}

/**
 * Stop the message queue client by setting shutdown attribute and sending
 * sentinel messages
 * @param   mq      Message Queue structure.
 */
void mq_stop(MessageQueue *mq) {
    // Push a sentinel to force both puller and pusher unblock so they can terminate when needed
    mq_publish(mq, SENTINEL, SENTINEL);
    // Lock before undating shared shutdown variable
    mutex_lock(&mq->lock);
    mq->shutdown = true;
    mutex_unlock(&mq->lock);
    // Wait for the pusher and puller threads
    thread_join(mq->thread1, NULL);
    thread_join(mq->thread2, NULL);
}

/**
 * Returns whether or not the message queue should be shutdown.
 * @param   mq      Message Queue structure.
 */
bool mq_shutdown(MessageQueue *mq) {
    bool shutdown;
    // Lock before accessing shared shutdown variable
    mutex_lock(&mq->lock);
    shutdown = mq->shutdown;
    mutex_unlock(&mq->lock);
    return shutdown;
}

/* Internal Functions */

/**
 * Pusher thread takes messages from outgoing queue and sends them to server.
 **/
void * mq_pusher(void *arg) {
    MessageQueue* mq = (MessageQueue*) arg;

    while(!mq_shutdown(mq)) {
        Request* r = queue_pop(mq->outgoing);
        if(r) {
            // Try to open connection to server
            FILE* fs = socket_connect(mq->host, mq->port);
            if(fs) {
                // Write request to the server
                request_write(r, fs);
                // Read sever response (who cares, not me)
                char resp_buf[BUFSIZ];
                while(fgets(resp_buf, BUFSIZ, fs)) {} // Continues to read response until nothing is left
                fclose(fs);
            }
            // Delete the sent request
            request_delete(r);
        }
    }
    return NULL;
}

/**
 * Puller thread requests new messages from server and then puts them in
 * incoming queue.
 **/
void * mq_puller(void *arg) {
    MessageQueue* mq = (MessageQueue*) arg;

    while(!mq_shutdown(mq)) {
        // Build the GET request
        char get_uri[BUFSIZ] = "/queue/";
        strcat(get_uri, mq->name);
        Request* r = request_create("GET", get_uri, NULL);

        if(r) {
            // Try to open connection to server
            FILE* fs = socket_connect(mq->host, mq->port);
            if(fs) {
                request_write(r, fs);
                // Read response status
                char resp_buf[BUFSIZ];
                fgets(resp_buf, BUFSIZ, fs);
                // Check to see if the return status is ok
                if(!strstr(resp_buf, "200 OK")) {
                    // Close stream and try again
                    fclose(fs);
                    continue;
                }
                // Process headers to find the content length
                size_t resp_len;
                while(fgets(resp_buf, BUFSIZ, fs) && !streq(resp_buf, "\r\n")) {
                    sscanf(resp_buf, "Content-Length: %ld", &resp_len);
                }
                // Update the body of the old request with the response
                r->body = calloc(1,resp_len+1);
                fread(r->body, 1, resp_len, fs);
                // Cleanup the request if we don't receive a body
                if(!r->body) {
                    request_delete(r);
                }
                // If we have a valid body add the request to the incoming queue
                else {
                    queue_push(mq->incoming, r);
                }
                fclose(fs);
            }
        }
    }
    return NULL;
}