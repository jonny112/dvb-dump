
#ifdef WINDOWS
#include <fcntl.h>
int _CRT_fmode = _O_BINARY;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef WINDOWS
// -lws2_32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#define BUFFER_SIZE 1316

int main(int argc, char **argv) {
    int sock;
    char buff[BUFFER_SIZE];
    struct sockaddr_in addr_local, addr_remote;
#ifdef WINDOWS
    int addr_remote_len = sizeof(addr_remote);
    WSADATA wsaData = {0};
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    socklen_t addr_remote_len = sizeof(addr_remote);
#endif
    
    if (argc < 3) {
        fprintf(stderr, "Usage: udpdump <address> <port>\n");
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
    
    while (recvfrom(sock, buff, BUFFER_SIZE, 0, (struct sockaddr*)&addr_remote, &addr_remote_len) == BUFFER_SIZE) {
        write(STDOUT_FILENO, buff, BUFFER_SIZE);
    }
    
    close(sock);
    return 0;
}
