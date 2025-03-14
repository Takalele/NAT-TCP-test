#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
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
    char buf;

    msg("[+] Trying to establish connections: ");
    for (int i = 0; i < NCONNECTIONS; i++) {
        printf("%d ", i);
        fflush(stdout);
        tcpsessions[i] = do_connect(&dst_addr);
        if (tcpsessions[i] == -1) {
            return EXIT_FAILURE;
        }
    }
    printf("\n");

    msg("[+] All connections established\n");

    for (int i = 0; i < NCONNECTIONS; i++) {
        // This is not exact - we'll gradually drift
        // by the time it takes to write() and read()
        sleep(60);
        if (write(tcpsessions[i], "A", 1) != 1) {
            if (errno != ETIMEDOUT) {
                perror("write");
                return EXIT_FAILURE;
            }
            msg("[-] Connection %d is dead (write)\n", i);
            close(tcpsessions[i]);
            continue;
        }

        if (read(tcpsessions[i], &buf, 1) != 1) {
            if (errno != ETIMEDOUT) {
                perror("read");
                return EXIT_FAILURE;
            }
            msg("[-] Connection %d is dead (read)\n", i);
            close(tcpsessions[i]);
            continue;
        }
        msg("[+] Connection %d worked\n", i);

        close(tcpsessions[i]);
    }

    return 0;
}
