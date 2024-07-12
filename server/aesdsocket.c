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

#define PORT 9000
#define BUFFER_SIZE 2048
#define STR_SIZE 3*1024
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


static int server_fd, new_socket;
static FILE* fd;

void handle_signal(int signal) {
    printf("Received signal %d, closing sockets...\n", signal);

    // Close client socket if open
    if (new_socket >= 0) {
        close(new_socket);
    }

    // Close server socket if open
    if (server_fd >= 0) {
        close(server_fd);
    }

    fclose(fd);

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


int setBlockingMode(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }

    flags &= ~O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) 
{
        int count = 1;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        char buffer[BUFFER_SIZE] = {0};

        char str_to_write[STR_SIZE];
        int curr_written_bytes = 0;
        int curr_dbuf_size = 0;
        int new_size = 0;
        int start = 1;
        int found = 0;
        int run_as_daemon = 0;
        pid_t pid;


        // Set up signal handlers
        if (setup_signal_handlers()) {
                exit(EXIT_FAILURE);
        }


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
        fd = fopen(filenameToWrite, "w+");
        if (!fd) {
                printf("Failed to open the file: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        printf("file opened.\n");

        // Creating socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
                perror("socket failed");
                exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port 8080
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                &opt, sizeof(opt))) {
                perror("setsockopt");
                close(server_fd);
                exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        printf("bind..\n");
        // Binding the socket to the port
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                perror("bind failed");
                close(server_fd);
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

                // Create a new session and become the session leader
                if (setsid() == -1) {
                        exit(EXIT_FAILURE);
                }

                switch (fork()) {
                        case -1: 
                                perror("fork failed");
                                exit(EXIT_FAILURE);
                        case 0: 
                                printf("child continue execution..\n");
                                break; // Grandchild process continues
                        default: 
                                printf("child created. fork succed. parent exiting..\n");
                                _exit(EXIT_SUCCESS); // Child process exits
                }

                // Optionally, change the working directory to root and set file permissions
                chdir("/");
                umask(1);

                // Close all open file descriptors
                for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
                        close(fd);
                }

                // Redirect standard input, output, and error to /dev/null
                open("/dev/null", O_RDWR); // stdin
                dup(0); // stdout
                dup(0); // stderr
        }

        while (1) {

                // Start listening for incoming connections
                if (listen(server_fd, 3) < 0) {
                        perror("listen");
                        close(server_fd);
                        exit(EXIT_FAILURE);
                }


                printf("accept conn..\n");
                // Accept an incoming connection
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
                                                (socklen_t *)&addrlen)) < 0) {
                        perror("accept");
                        close(server_fd);
                        exit(EXIT_FAILURE);
                }

                if (setBlockingMode(new_socket) == -1) {
                        close(new_socket);
                        close(server_fd);
                        exit(EXIT_FAILURE);
                }


                char *dbuf = calloc(1, sizeof(char));
                if (dbuf == NULL) {
                        perror("Failed to allocate memory for segment");
                        return EXIT_FAILURE;
                }

                printf("start read loop..\n");

                for (;;) {
                        found = 0;
                        // Read data from the client
                        int nread = read(new_socket, buffer, BUFFER_SIZE);
                        if (nread == 0) {
                                printf("client has disconnected..\n");
                                break;
                        }

                        printf("Received: bytes read:%d\n", nread);
                        /* printf("Received: [%s] bytes read:%d\n", buffer, nread); */

                        printf("processing..\n");

                        // search for \n in received buffer
                        count++;
                        printf("search for \\n in received buffer. count:%d\n", count);
                        for (size_t i = 0; i < nread; i++) {
                                /* printf("analizing %c\n", buffer[i]); */
                                str_to_write[curr_written_bytes] = buffer[i];
                                curr_written_bytes++;

                                if (buffer[i] == '\n') {
                                        found = 1;
                                        printf("Newline character found at position: %zu. writing to file\n", i);

                                        size_t tot_size = curr_dbuf_size + curr_written_bytes;

                                        char* out_buf = calloc(tot_size, sizeof(char));
                                        memcpy(out_buf, dbuf, curr_dbuf_size);
                                        memcpy(out_buf + curr_dbuf_size*sizeof(char), str_to_write, curr_written_bytes);

                                        printf("writing out_buf to file..\n");
                                        size_t elems = fwrite(out_buf, sizeof(char), tot_size, fd);
                                        if (elems != tot_size) {
                                                printf("Failed to write data to file. writed:%zu, expected:%zu",
                                                                elems, tot_size);
                                                fclose(fd);
                                                return EXIT_FAILURE;
                                        }

                                        printf("writed out_buf\n");
                                        free(out_buf);



                                        // clear static temp buf
                                        memset(str_to_write, '0', STR_SIZE);
                                        curr_written_bytes = 0;

                                        // reset dbuf storage
                                        curr_dbuf_size = 0;
                                        dbuf = realloc(dbuf, 1);
                                        if (dbuf == NULL) {
                                                perror("Failed to realloc when reset dbuf storage");
                                                free(dbuf);
                                                return EXIT_FAILURE;
                                        }

                                }
                        }


                        // add to dbuf the new packet if there is no \n found
                        if (curr_written_bytes != 0) {
                                printf("data arrived has no EOL. reallocating %d\n", new_size);
                                new_size = curr_dbuf_size + curr_written_bytes;

                                printf("reallocating [%d] bytes = curr_written_bytes [%d] + curr_dbuf_size [%d]\n", 
                                                new_size, curr_written_bytes, curr_dbuf_size);

                                dbuf = realloc(dbuf, new_size * sizeof(char));
                                if (dbuf == NULL) {
                                        perror("Failed to reallocate memory for store_buffer");
                                        free(dbuf);
                                        return EXIT_FAILURE;
                                }

                                printf("memcpy on new realloc dbuf content of str_to_write\n");
                                memcpy(dbuf + curr_dbuf_size * sizeof(char), str_to_write, curr_written_bytes);

                                curr_dbuf_size = new_size;
                        }

                        memset(str_to_write, '0', STR_SIZE);
                        curr_written_bytes = 0;


                        // Send a response back to the client

                        if (found) {
                                // Determine the file size
                                fseek(fd, 0, SEEK_END);   // Move to the end of the file
                                long filesize = ftell(fd); // Get the current position in the file
                                fseek(fd, 0, SEEK_SET);   // Move back to the start of the file
                                if (filesize == -1) {
                                        perror("Failed to determine file size");
                                        fclose(fd);
                                        return EXIT_FAILURE;
                                }
                                printf("read filesize: %ld\n", filesize);

                                char* out_buf = malloc(filesize + sizeof(char));
                                memset(out_buf, 0, filesize + sizeof(char));
                                size_t elems = fread(out_buf, sizeof(char), filesize, fd);
                                printf("read back the file.. elems: %zu\n", elems);
                                if (elems != filesize) {
                                        printf("Failed to read data from file. read:%zu, expected:%zu",
                                                        elems, filesize);
                                        fclose(fd);
                                        return EXIT_FAILURE;
                                }

                                out_buf[filesize -1 + sizeof(char)] = '\0';

                                printf("send message back.. [%s]\n", out_buf);
                                ssize_t r = send(new_socket, out_buf, strlen(out_buf), 0);
                                printf("send ret: %zu, freeing out_buf..\n", r);
                                free(out_buf);

                        }

                        memset(buffer, 0, BUFFER_SIZE);

                }

                free(dbuf);
        }

        // Close the socket
        close(new_socket);
        close(server_fd);
        fclose(fd);
}
