
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define FRAME_SIZE 188
#define BUFFER_FRAMES 7

int main(int argc, char **argv) {
    int sock, pos;
    unsigned char buff[FRAME_SIZE * BUFFER_FRAMES];
    struct sockaddr_in addr;
    
    if (argc < 3) {
        fprintf(stderr, "Usage: udpcast <address> <port>\n");
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
    
    pos = 0;
    while (read(STDIN_FILENO, buff + pos, FRAME_SIZE) == FRAME_SIZE) {
        pos += FRAME_SIZE;
        if (pos == (FRAME_SIZE * BUFFER_FRAMES)) {
            if (sendto(sock, buff, FRAME_SIZE * BUFFER_FRAMES, 0, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                perror("Error sending packet");
                return 1;
            }
            pos = 0;
        }
    }
    
    close(sock);
    return 0;
}
