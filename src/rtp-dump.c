
#ifdef WINDOWS
#include <fcntl.h>
int _CRT_fmode = _O_BINARY;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef WINDOWS
// link ws2_32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#define RTP_HAEDER_SIZE 12
#define BUFFER_SIZE 1316

int main(int argc, char **argv) {
    int sock, incnt;
    unsigned short cont = 0, seq = 0;
    unsigned char sync = 0;
    unsigned char buff[RTP_HAEDER_SIZE + BUFFER_SIZE];
    struct sockaddr_in addr_local, addr_remote;
#ifdef WINDOWS
    int addr_remote_len = sizeof(addr_remote);
    WSADATA wsaData = {0};
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    socklen_t addr_remote_len = sizeof(addr_remote);
#endif
    
    if (argc < 3) {
        fprintf(stderr, "Usage: rtpdump <address> <port>\n");
        return 1;
    }
    
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Could not open socket");
        return 1;
    }
    
    memset(&addr_local, 0, sizeof(addr_local));
    addr_local.sin_family = AF_INET;
    addr_local.sin_port = htons(atoi(argv[2]));
    
#ifdef WINDOWS    
    if ((addr_local.sin_addr.s_addr = inet_addr(argv[1])) == 0) {
#else
    if (inet_aton(argv[1], &addr_local.sin_addr) == 0) {
#endif
        fprintf(stderr, "Failed to set address\n");
        return 1;
    }
    
    if (bind(sock, (struct sockaddr *)&addr_local, sizeof(addr_local)) == -1) {
        perror("Failed to bind socket");
        return 1;
    }
    
    while ((incnt = recvfrom(sock, (char *)buff, RTP_HAEDER_SIZE + BUFFER_SIZE, 0, (struct sockaddr*)&addr_remote, &addr_remote_len)) == (RTP_HAEDER_SIZE + BUFFER_SIZE)) {
        seq = ((buff[2] << 8) | buff[3]);
        
        if (buff[0] != 0x80) {
            fprintf(stderr, "[0x%04X:0x%04X] RTP: Bad header (0x%02X)\n", cont, seq, buff[0]);
        } else if ((buff[1] & 0xEF) != 33) {
            fprintf(stderr, "[0x%04X:0x%04X] RTP: Unsupported payload type (%d)\n", cont, seq, buff[1] & 0xEF);
        } else {
            if (sync && (buff[1] & 0x80) == 0 && seq != (unsigned short)(cont + 1)) {
                fprintf(stderr, "[0x%04X:0x%04X] RTP: Sync lost\n", cont, seq);
                sync = 0;
            } else if (! sync && seq == (unsigned short)(cont + 1)) {
                fprintf(stderr, "[0x%04X:0x%04X] RTP: Synchronized\n", cont, seq);
                sync = 1;
            }
            
            write(STDOUT_FILENO, buff + RTP_HAEDER_SIZE, BUFFER_SIZE);
            cont = seq;
        }
    }
    fprintf(stderr, "[0x%04X:0x%04X] RTP: Window missmatch, expected %d, got %d\n", cont, seq, RTP_HAEDER_SIZE + BUFFER_SIZE, incnt);
    
    close(sock);
    return 2;
}
