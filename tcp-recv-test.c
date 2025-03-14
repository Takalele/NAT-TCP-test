#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "util.h"

#define DEFAULT_IP "130.225.254.111"

int do_connect(struct sockaddr_in *dst) {
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == -1) {
        perror("socket");
        return -1;
    }

    if (connect(s, (struct sockaddr *) dst, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        return -1;
    }

/* for faster debugging
    // specifies the maximum amount of time in milliseconds
    // that transmitted data may remain unacknowledged before
    // TCP will forcibly close the corresponding connection
    // and return ETIMEDOUT to the application.
    if (setsockopt(s, IPPROTO_TCP, TCP_USER_TIMEOUT,
                (int []){ 10 * 1000 }, sizeof(int)) == -1) {
        perror("setsockopt TCP_USER_TIMEOUT");
        return -1;
    }
*/

    return s;
}

#define NCONNECTIONS 130

int main(int argc, char *argv[]){
    const char *ip_address = DEFAULT_IP;
    
    if (argc >= 2) {
        ip_address = argv[1];
    }

    struct sockaddr_in dst_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(31415),
    };

    // Convert IP string to binary format
    if (inet_pton(AF_INET, ip_address, &dst_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", ip_address);
        return EXIT_FAILURE;
    }

    int tcpsessions[NCONNECTIONS] = {0};
    int serversleeptime[NCONNECTIONS] = {0};
    char aint[64] = {0};
    int ret;
    time_t startup_time = time(NULL);


    msg("[+] Trying to establish connections: ");
    for (int i = 0; i < NCONNECTIONS; i++) {
        serversleeptime[i] = (i+1) * 60;
        printf("%d (%ds), ", i, serversleeptime[i]);
        fflush(stdout);
        tcpsessions[i] = do_connect(&dst_addr);
        if (tcpsessions[i] == -1) {
            return EXIT_FAILURE;
        }
        snprintf(aint, sizeof(aint) - 1, "%d\n", serversleeptime[i]);
        write(tcpsessions[i], aint, strnlen(aint, sizeof aint));
    }

    printf("\n");
    msg("[+] All connections established\n");

    // select() loop
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int openconnections = 0;
        int maxfd = -1;

        for (int i = 0; i < NCONNECTIONS; i++) {
            if (tcpsessions[i] == -1)
                continue;
            FD_SET(tcpsessions[i], &rfds);
            maxfd = tcpsessions[i] > maxfd ? tcpsessions[i] : maxfd;
            openconnections++;
        }
        if (openconnections == 0) {
            msg("[+] No more alive connections left\n");
            return EXIT_SUCCESS;
        }

        msg("[+] Waiting for the server to close a connection\n");
        msg("[+] Open connections: %d\n", openconnections);

        ret = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            perror("select");
            return EXIT_FAILURE;
        } else if (ret) {
            for (int i = 0; i < NCONNECTIONS; i++) {
                if (tcpsessions[i] != -1 && FD_ISSET(tcpsessions[i], &rfds)) {
                    ssize_t readret = read(tcpsessions[i], aint, sizeof(aint)-1);
                    if (readret == -1) {
                        msg("[-] Shouldn't happen:\n"
                                "readret: %ld\n"
                                "errno: %d (%m)\n"
                                "i: %d\n"
                                "tcpsessions[i]: %d\n"
                                "aint: %s\n",
                                readret, errno, i, tcpsessions[i], aint
                              );
                    } else if (readret > 0) {
                        aint[readret] = '\0';
                        time_t alivetime = time(NULL) - startup_time;
                        msg("[+] Connection %d returned after %ldm %lds: %s\n",
                               i, alivetime/60, alivetime%60, aint);
                    } else {
                        msg("[-] Shouldn't happen. Connection %d\n", i);
                    }

                    close(tcpsessions[i]);
                    tcpsessions[i] = -1;
                }
            }
        } else {
            msg("[+] No data\n");
        }
    }

    return 0;
}
