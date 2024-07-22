#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <syslog.h>
#include<time.h>
#include<sys/time.h>
#include<stdbool.h>

#define PORT 9000
#define BUFFER_SIZE 2048
#define STR_SIZE 3*1024
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct thread_args {
    int th_count;
    int socket;
} Thread_args;

//linked list
typedef struct node {
    pthread_t th;
    struct node *next;
    int socket;
} Node;

static int _server_fd;
static FILE* _fd;
static int _th_count = 0;
Node *_head = NULL;
pthread_mutex_t fmux;
pthread_mutex_t tmux;


void free_resurces()
{
        // free all threads
        Node *next;
        Node *curr_th = _head;
        curr_th = _head;
        while (curr_th != NULL) {
                next = curr_th->next;
                close(next->socket);
                free(curr_th);
                curr_th = next;
        }


        // Close the socket
        if (_server_fd >= 0) {
                close(_server_fd);
        }
        fclose(_fd);
}


void handle_signal(int signal) 
{
        printf("Received signal %d, closing sockets...\n", signal);
        free_resurces();
        exit(0);
}



int setup_signal_handlers()
{
        struct sigaction sa;

        sa.sa_handler = handle_signal;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGTERM, &sa, NULL) != 0) {
                perror("Error setting up SIGINT handler");
                return -1;
        }

        if (sigaction(SIGINT, &sa, NULL) != 0) {
                perror("Error setting up SIGINT handler");
                return -1;
        }

        return 0;
}



void signalAlarmHandler(int sig)
{
        struct timeval tv;
        struct tm ptm;
        char time_string[40];
        long milliseconds;

        gettimeofday(&tv, NULL);

        localtime_r(&tv.tv_sec, &ptm);
        strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", &ptm);

        milliseconds = tv.tv_usec;

        printf("%s.%06ld %c%02d%02d\n", time_string, milliseconds, 
                        (ptm.tm_gmtoff < 0) ? '-' : '+',
                        abs(ptm.tm_gmtoff) / 3600,
                        (abs(ptm.tm_gmtoff) % 3600) / 60);

        pthread_mutex_lock(&fmux);
        fprintf(_fd, "timestamp:%s.%06ld %c%02d%02d\n", time_string, milliseconds, 
                        (ptm.tm_gmtoff < 0) ? '-' : '+',
                        abs(ptm.tm_gmtoff) / 3600,
                        (abs(ptm.tm_gmtoff) % 3600) / 60);
        fflush(_fd);   
        /* fseek(fd, 0, SEEK_SET); */
        pthread_mutex_unlock(&fmux);

        alarm(10);
}



/* int setBlockingMode(int sockfd)  */
/* { */
        /* int flags = fcntl(sockfd, F_GETFL, 0); */
        /* if (flags == -1) { */
                /* perror("fcntl F_GETFL"); */
                /* return -1; */
        /* } */
/*  */
        /* flags &= ~O_NONBLOCK; */
        /* if (fcntl(sockfd, F_SETFL, flags) == -1) { */
                /* perror("fcntl F_SETFL"); */
                /* return -1; */
        /* } */
/*  */
        /* return 0; */
/* } */
/*  */


void* handle_client(void *arg)
{
        char buffer[BUFFER_SIZE] = {0};
        int found = 0;
        int count = 1;
        char str_to_write[STR_SIZE];
        int curr_written_bytes = 0;
        int curr_dbuf_size = 0;
        int new_size = 0;

        Thread_args th_arg = *((Thread_args*) arg);
        int th_n = th_arg.th_count;
        int curr_socket = th_arg.socket;

        syslog(LOG_INFO, "[CH%d]: start...", th_n);

        /* pthread_mutex_unlock(&tmux); */

        char *dbuf = calloc(1, sizeof(char));
        if (dbuf == NULL) {
                syslog(LOG_ERR, "[CH%d]: Failed to allocate memory for segment", th_n);
                return NULL;
        }


        for (;;) {
                found = 0;
                // Read data from the client
                int nread = read(curr_socket, buffer, BUFFER_SIZE);
                if (nread == 0) {
                        syslog(LOG_ERR, "[CH%d]: client has disconnected..", th_n);
                        break;
                }
                if (nread == -1) {
                        syslog(LOG_ERR, "[CH%d]: client read return error: %s", 
                                        th_n, strerror(errno));
                        exit(EXIT_FAILURE);
                }

                syslog(LOG_INFO, "[CH%d]: Received: bytes read:%d", th_n, nread);
                /* printf("Received: [%s] bytes read:%d\n", buffer, nread); */

                pthread_mutex_lock(&fmux);


                // search for \n in received buffer
                count++;
                /* syslog(LOG_INFO, "[CH%d]: search for \\n in received buffer. count:%d", th_n, count); */
                for (size_t i = 0; i < nread; i++) {
                        /* printf("analizing %c\n", buffer[i]); */
                        str_to_write[curr_written_bytes] = buffer[i];
                        curr_written_bytes++;

                        if (buffer[i] == '\n') {
                                found = 1;
                                /* syslog(LOG_INFO, "[CH%d]: Newline character found at position: %zu. writing to file", th_n, i); */

                                size_t tot_size = curr_dbuf_size + curr_written_bytes;

                                char* out_buf = calloc(tot_size, sizeof(char));
                                memcpy(out_buf, dbuf, curr_dbuf_size);
                                memcpy(out_buf + curr_dbuf_size*sizeof(char), str_to_write, curr_written_bytes);

                                syslog(LOG_INFO, "[CH%d]: WRITING to file: %s", _th_count, out_buf);
                                size_t elems = fwrite(out_buf, sizeof(char), tot_size, _fd);
                                if (elems != tot_size) {
                                        syslog(LOG_ERR, "[CH%d]: Failed to write data to file. writed:%zu, expected:%zu", th_n,
                                                        elems, tot_size);
                                        fclose(_fd);
                                        return NULL;
                                }

                                syslog(LOG_INFO, "[CH%d]: writed out_buf", th_n);
                                free(out_buf);



                                // clear static temp buf
                                memset(str_to_write, '0', STR_SIZE);
                                curr_written_bytes = 0;

                                // reset dbuf storage
                                curr_dbuf_size = 0;
                                dbuf = realloc(dbuf, 1);
                                if (dbuf == NULL) {
                                        syslog(LOG_ERR, "[CH%d]: Failed to realloc when reset dbuf storage", th_n);
                                        free(dbuf);
                                        return NULL;
                                }

                        }
                }



                // add to dbuf the new packet if there is no \n found
                if (curr_written_bytes != 0) {
                        syslog(LOG_INFO, "[CH%d]: data arrived has no EOL. reallocating %d", th_n, new_size);
                        new_size = curr_dbuf_size + curr_written_bytes;

                        syslog(LOG_INFO, "[CH%d]: reallocating [%d] bytes = curr_written_bytes [%d] + curr_dbuf_size [%d]", th_n, 
                                        new_size, curr_written_bytes, curr_dbuf_size);

                        dbuf = realloc(dbuf, new_size * sizeof(char));
                        if (dbuf == NULL) {
                                syslog(LOG_ERR, "[CH%d]: Failed to reallocate memory for store_buffer", th_n);
                                free(dbuf);
                                return NULL;
                        }

                        syslog(LOG_INFO, "[CH%d]: memcpy on new realloc dbuf content of str_to_write", th_n);
                        memcpy(dbuf + curr_dbuf_size * sizeof(char), str_to_write, curr_written_bytes);

                        curr_dbuf_size = new_size;
                }

                memset(str_to_write, '0', STR_SIZE);
                curr_written_bytes = 0;


                // Send a response back to the client

                if (found) {
                        // Determine the file size
                        fseek(_fd, 0, SEEK_END);   // Move to the end of the file
                        long filesize = ftell(_fd); // Get the current position in the file
                        fseek(_fd, 0, SEEK_SET);   // Move back to the start of the file
                        if (filesize == -1) {
                                syslog(LOG_ERR, "[CH%d]: Failed to determine file size", th_n);
                                fclose(_fd);
                                return NULL;
                        }
                        syslog(LOG_INFO, "[CH%d]: read filesize: %ld", th_n, filesize);

                        char* out_buf = malloc(filesize + sizeof(char));
                        memset(out_buf, 0, filesize + sizeof(char));
                        size_t elems = fread(out_buf, sizeof(char), filesize, _fd);
                        syslog(LOG_INFO, "[CH%d]: read back the file.. elems: %zu", th_n, elems);
                        if (elems != filesize) {
                                syslog(LOG_ERR, "[CH%d]: Failed to read data from file. read:%zu, expected:%zu", th_n,
                                                elems, filesize);
                                fclose(_fd);
                                return NULL;
                        }

                        out_buf[filesize -1 + sizeof(char)] = '\0';

                        syslog(LOG_INFO, "[CH%d]: send message back.. [%s]. size:%zu", 
                                        th_n, out_buf, strlen(out_buf));
                        ssize_t r = send(curr_socket, out_buf, strlen(out_buf), 0);
                        syslog(LOG_INFO, "[CH%d]: send ret: %zu, freeing out_buf..", th_n, r);
                        free(out_buf);

                }

                memset(buffer, 0, BUFFER_SIZE);

                pthread_mutex_unlock(&fmux);

        }

        free(dbuf);

        /* return NULL; */
}



int main(int argc, char *argv[]) 
{
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        int run_as_daemon = 0;
        pid_t pid;
        int new_socket;


        // Set up signal handlers
        if (setup_signal_handlers()) {
                exit(EXIT_FAILURE);
        }

        openlog("aesdsocket", LOG_CONS | LOG_PID, LOG_USER);
        syslog(LOG_INFO, "aesdsocket start..");


        while ((opt = getopt(argc, argv, "d")) != -1) {
                switch (opt) {
                        case 'd':
                                run_as_daemon = 1;
                                break;
                        default:
                                printf("no option passed. continue with default mode\n");
                }
        }

        //open file to save data
        const char* filenameToWrite = "/var/tmp/aesdsocketdata";
        printf("try to open file %s..\n", filenameToWrite);
        _fd = fopen(filenameToWrite, "w+");
        if (!_fd) {
                printf("Failed to open the file: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        printf("file opened.\n");

        // Creating socket file descriptor
        if ((_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
                perror("socket failed");
                exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port 8080
        if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                &opt, sizeof(opt))) {
                perror("setsockopt");
                close(_server_fd);
                exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        printf("bind..\n");
        // Binding the socket to the port
        if (bind(_server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                perror("bind failed");
                close(_server_fd);
                exit(EXIT_FAILURE);
        }


        // Start listening for incoming connections
        syslog(LOG_INFO, "socket: listen..\n");
        if (listen(_server_fd, 3) < 0) {
                syslog(LOG_ERR, "listen fail");
                close(_server_fd);
                exit(EXIT_FAILURE);
        }

        if (run_as_daemon) {
                printf("forking process..\n");
                pid = fork();
                if (pid < 0) {
                        perror("fork failed");
                        exit(EXIT_FAILURE);
                }

                if (pid > 0) {
                        printf("child created with pid %d. fork succed. parent exiting..\n", pid);
                        exit(EXIT_SUCCESS);
                }
                
                if (pid == 0)
                        printf("child continue execution..\n");
        }

        // start writing timestamp each 10 seconds
        signal(SIGALRM, signalAlarmHandler);
        alarm(10);

        while (1) {

                syslog(LOG_INFO, "socket: accept connection..\n");
                // Accept an incoming connection
                new_socket = accept(_server_fd, 
                                    (struct sockaddr *)&address, 
                                    (socklen_t *)&addrlen);
                if (new_socket < 0) {
                        syslog(LOG_ERR, "accept fail");
                        close(_server_fd);
                        exit(EXIT_FAILURE);
                }

                syslog(LOG_INFO, "socket: connection accepted!\n");

                // launching separate threads for each connection
                /* pthread_mutex_lock(&tmux); */
                _th_count++;
                syslog(LOG_INFO, "launching client hanler thread %d...", _th_count);

                Node* n = malloc(sizeof(Node));
                n->socket = new_socket;

                Thread_args args = {
                        .socket = new_socket,
                        .th_count = _th_count
                };

                if (pthread_create(&(n->th), NULL, handle_client, (void*)&args) != 0) {
                        syslog(LOG_ERR, "Unable to create thread");
                        close(new_socket);
                        continue;
                }
                
                n->next = _head;
                _head = n;

                syslog(LOG_INFO, "thread %d launched", _th_count);
        }

        // join threads
        Node *curr_th = _head;
        while (curr_th != NULL) {
                if (pthread_join(curr_th->th, NULL) != 0) {
                        syslog(LOG_ERR, "Unable to join thread");
                }
                curr_th = curr_th->next;
        }

        free_resurces();
}

