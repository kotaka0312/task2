#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 2048

typedef struct {
    uint16_t seq_no;  // 序列号
    uint8_t ver;      // 版本号
    uint8_t type;     // 包类型
    char data[200];   // 数据
} Packet;

void get_current_time(char *buffer, size_t len);

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    Packet recv_packet, send_packet;
    socklen_t addr_len = sizeof(client_addr);
    struct timeval start_time, end_time;

    // 创建套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 绑定套接字到服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server started...\n");

    while (1) {
        // 接收数据包
        if (recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &addr_len) < 0) {
            perror("recvfrom");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // 记录开始时间
        gettimeofday(&start_time, NULL);

        // 检查版本号
        if (recv_packet.ver != 2) {
            printf("Unexpected version number: %d\n", recv_packet.ver);
            continue;
        }

        // 根据包的类型处理不同的情况
        switch (recv_packet.type) {
            case 1: // SYN
                printf("Received SYN.\n");
                send_packet.seq_no = recv_packet.seq_no;
                send_packet.ver = 2;
                send_packet.type = 2; // SYN-ACK
                memset(send_packet.data, 'B', sizeof(send_packet.data));
                if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
                    perror("sendto");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                break;

            case 3: // ACK
                printf("Received ACK.\n");
                break;

            case 4: // Data packet
                printf("Received data packet %d.\n", recv_packet.seq_no);
                send_packet.seq_no = recv_packet.seq_no;
                send_packet.ver = 2;
                send_packet.type = 3; // ACK
                get_current_time(send_packet.data, sizeof(send_packet.data));
                if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
                    perror("sendto");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                break;

            case 5: // FIN
                printf("Received FIN.\n");
                send_packet.seq_no = recv_packet.seq_no;
                send_packet.ver = 2;
                send_packet.type = 3; // ACK for FIN
                if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
                    perror("sendto");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                printf("Connection closed.\n");
                break;

            default:
                printf("Unexpected packet type: %d\n", recv_packet.type);
                break;
        }

        // 记录结束时间并计算总响应时间
        //gettimeofday(&end_time, NULL);
        //double response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        //printf("整体响应时间：%.2f ms\n", response_time);
    }

    close(sockfd);
    return 0;
}

void get_current_time(char *buffer, size_t len) {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    snprintf(buffer, len, "%02d-%02d-%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}
