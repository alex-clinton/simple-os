/* chat.c : Chat Application */

#include "mq/client.h"
#include "mq/string.h"

#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

#include <termios.h>

#define BACKSPACE 127

/* Constants */
const char * TOPIC     = "testing";
const size_t NMESSAGES = 10;

/* Global Variables */
size_t tm = 0;
bool end = false;

void toggle_raw_mode() {
    static struct termios OriginalTermios = {0};
    static bool enabled = false;

    if (enabled) {
    	tcsetattr(STDIN_FILENO, TCSAFLUSH, &OriginalTermios);
    } 
    else {
        tcgetattr(STDIN_FILENO, &OriginalTermios);

        atexit(toggle_raw_mode);

        struct termios raw = OriginalTermios;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

        enabled = true;
    }
}

/* Threads */
void *input_thread(void * arg){
    char cmd[BUFSIZ] = "";
    char *c1,*c2,*c3;
    size_t i_index = 0;

    c1 = calloc(1,sizeof(char *));
    MessageQueue *mq = (MessageQueue *)arg;

    while(streq(cmd,"\n") || (!streq(c1,"/quit") && !streq(c1,"/q") ) ) {
        i_index = 0;
        c1=NULL,c2=NULL,c3=NULL;
        cmd[0] = 0;

        while(true) {
            char input_char = 0;
            read(STDIN_FILENO, &input_char, 1);
            // Process commands
            if (input_char == '\n') {				
                printf("\n");
                break;
            } 
            else if (input_char == BACKSPACE && i_index) {
                cmd[--i_index] = 0;
            } 
            else if (!iscntrl(input_char) && i_index < BUFSIZ) {
                cmd[i_index++] = input_char;
                cmd[i_index]   = 0;
            }
            printf("\r%-80s", "");			
	        printf("\r%s", cmd);		
	        fflush(stdout);
        }
        // If nothing is entered
        if (streq(cmd,"\n")) {         
          printf("Please enter a command\n");
          continue;
        }
        // Parse commands
        // c1 = command
        // c2 = topic
        // c3 = message (if needed)
        c1 = strtok(cmd," \n");
        if(c1) { c2 = strtok(NULL," \n"); }
        if(c2) { c3 = strtok(NULL,"\n"); }

        // Determine command
        if(streq(c1,"/pub")) {
            if(c2 && c3) {
                char* un = (char*)malloc(1+strlen(mq->name));
                sprintf(un,"(%s): ",mq->name);
                char * to_send = (char *) malloc(1 + strlen(un)+ strlen(c3) );

                strcpy(to_send, un);
                strcat(to_send, c3);

          	    mq_publish(mq, c2, to_send);
                free(to_send);
                free(un);
            }
            else {
                printf("USAGE: '/pub <topic> <body>'\n");
            }
        }
        else if(streq(c1,"/sub")) {
            if(c2 && !c3) {
                printf("Subscribing to '%s'\n",c2);
                mq_subscribe(mq,c2);
            }
            else {
                printf("USAGE: '/sub <topic>'\n");
            }
        }
        else if(streq(c1,"/unsub")) {
            if(c2 && !c3) {
                printf("Un-subscribing from '%s'\n",c2);
                mq_unsubscribe(mq,c2);
            }
            else {
                printf("USAGE: '/unsub <topic>'\n");
            }
        }
        else if(streq(c1,"/help")) {
            printf("/sub   <topic>     : Subscribe to <topic>\n");
            printf("/unsub <topic>     : Un-subscribe to <topic>\n");
            printf("/pub <topic> <msg> : Publish <msg>to <topic>\n");
            printf("/help              : Show list of commands\n");
            printf("/quit /q           : Exit application\n");
        }
        else if(streq(c1,"/q") || streq(c1,"/quit")) {
            printf("Goodbye\n");
            mq_stop(mq);
        }
        else {
            printf("Unknown command '%s'. Enter '/help' for a list of commands\n",c1);
        }
    }
    end = true;
    return NULL;
}

void *incoming_thread(void *arg) {
    MessageQueue *mq = (MessageQueue *)arg;
    while (!mq_shutdown(mq)) {
    	char *message = mq_retrieve(mq);
        if (message) {
            if(!strstr(message,mq->name) || strstr(message,mq->name)!=message+1) {
                printf("\rMessage from %-80s",message);
            }
        }
    }
    return NULL;
}

/* Other Functions */
void usage(int status) {
    printf("usage: `./chat.c <host> <port> <uname>`\n");
    exit(status);
}

/* Main execution */
int main(int argc, char *argv[]) {
    if(argc > 4) {usage(1);}
    toggle_raw_mode();
    /* Parse command-line arguments */
    char *name = getenv("USER");
    char *host = "localhost";
    char *port = "9620";

    if (argc > 1) { host = argv[1]; }
    if (argc > 2) { port = argv[2]; }
    if (argc > 3) { name = argv[3]; }
    if (!name)    { name = "echo_client_test";  }

    if(streq(host,"-h")){usage(0);}

    printf("------------> QChat <------------\n");
    printf("username=%s ; connection=%s:%s\n",name,host,port);
    printf("Enter '/help' for a list of commands\n");

    /* Create and start message queue */
    MessageQueue *mq = mq_create(name, host, port);
    mq_start(mq);

    Thread incoming;
    Thread input;

    thread_create(&input,NULL, input_thread, mq);
    thread_create(&incoming,NULL, incoming_thread, mq);
    thread_join(input, NULL);

    mq_delete(mq);
    return 0;
}