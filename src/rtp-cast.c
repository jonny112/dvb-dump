
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RTP_HAEDER_SIZE 12
#define FRAME_SIZE 188
#define BUFFER_FRAMES 7

int main(int argc, char **argv) {
    int sock, pos;
    uint64_t t;
    unsigned short seq;
    unsigned char buff[RTP_HAEDER_SIZE + FRAME_SIZE * BUFFER_FRAMES];
    struct sockaddr_in addr;
    struct timespec ts;
    
    if (argc < 3) {
        fprintf(stderr, "Usage: rtpcast <address> <port>\n");
        return 1;
    }
    
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Could not open socket");
        return 1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    if (inet_aton(argv[1], &addr.sin_addr) == 0) {
        fprintf(stderr, "Failed to set address\n");
        return 1;
    }
    
    memset(&buff, 0, RTP_HAEDER_SIZE);
    buff[0] = 0x80;
    buff[1] = 33;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    srandom(ts.tv_sec ^ ts.tv_nsec);
    seq = random();
    
    pos = RTP_HAEDER_SIZE;
    while (read(STDIN_FILENO, buff + pos, FRAME_SIZE) == FRAME_SIZE) {
        pos += FRAME_SIZE;
        if (pos == (RTP_HAEDER_SIZE + FRAME_SIZE * BUFFER_FRAMES)) {
            buff[2] = seq >> 8;
            buff[3] = seq;
            
            clock_gettime(CLOCK_MONOTONIC, &ts);
            t = ((uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000) * 9 / 100;
            buff[4] = t >> 24;
            buff[5] = t >> 16;
            buff[6] = t >> 8;
            buff[7] = t;
            
            if (sendto(sock, buff, RTP_HAEDER_SIZE + FRAME_SIZE * BUFFER_FRAMES, 0, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                perror("Error sending packet");
                return 1;
            }
            
            pos = RTP_HAEDER_SIZE;
            seq++;
        }
    }
    
    close(sock);
    return 0;
}
