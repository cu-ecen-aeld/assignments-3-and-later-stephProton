#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[])
{
        openlog(NULL, 0, LOG_USER);

        int count;
        if (argc > 2) {
                for (count = 1; count < argc; count++) {
                        printf("argv[%d] = %s\n", count, argv[count]);
                }
        }
        else {
                syslog(LOG_DEBUG,"wrong args number");
                closelog();
                exit(EXIT_FAILURE);
        }

        char* filenameToWrite = strdup(argv[1]);
        char* strToWrite = strdup(argv[2]);

        syslog(LOG_DEBUG,"try to open file %s..", filenameToWrite);
        FILE *fd = fopen(filenameToWrite, "w+");
        if (!fd) {
                syslog(LOG_DEBUG,"Failed to open the file: %s", strerror(errno));
                closelog();
                exit(EXIT_FAILURE);
        }

        size_t ret = fwrite(strToWrite, strlen(strToWrite), sizeof(*strToWrite), fd);
           if (ret != sizeof(*strToWrite)) {
               fprintf(stderr, "fread() failed: %zu\n", ret);
               exit(EXIT_FAILURE);
           }

        syslog(LOG_DEBUG,"%s written", filenameToWrite);

        closelog();
        exit(EXIT_SUCCESS);
}

