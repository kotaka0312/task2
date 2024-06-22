#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <math.h>

#define SERVER_PORT 12345
#define BUFFER_SIZE 2048
#define LOSS_RATE 0.5  // 模拟丢包率
#define TIMEOUT 100    // 超时时间，单位：毫秒

typedef struct {
    uint16_t seq_no;  // 序列号
    uint8_t ver;      // 版本号
    uint8_t type;     // 包类型
    char data[200];   // 数据
} Packet;

void send_data_packets(int sockfd, struct sockaddr_in server_addr, double *pkt_loss_rate, double *server_response_time);
double calculate_rtt(struct timeval start, struct timeval end);

int main(int argc, char *argv[]) {
    if (argc != 3) { // 检查参数个数
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];   // 服务器IP地址
    int server_port = atoi(argv[2]);   // 服务器端口号

    int sockfd;
    struct sockaddr_in server_addr;
    Packet send_packet, recv_packet;
    socklen_t addr_len = sizeof(server_addr);
    struct timeval start_time, end_time;  // 定义时间结构体变量
    double pkt_loss_rate = 0.0;
    double server_response_time = 0.0;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // 创建UDP套接字
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT * 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) { // 设置接收超时时间
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) { // 设置服务器地址信息
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 记录开始时间
    gettimeofday(&start_time, NULL);

    // 发送SYN包
    send_packet.seq_no = 1;
    send_packet.ver = 2;
    send_packet.type = 1; // SYN
    memset(send_packet.data, 0, sizeof(send_packet.data));

    if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
        perror("sendto");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("发送SYN...\n");

    // 接收SYN-ACK包
    if (recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        perror("recvfrom");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if (recv_packet.type != 2) {
        fprintf(stderr, "连接建立失败。收到意外的包类型：%d\n", recv_packet.type);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("收到SYN-ACK。\n");

    // 发送ACK包
    send_packet.seq_no += 1;
    send_packet.type = 3; // ACK
    memset(send_packet.data, 0, sizeof(send_packet.data));

    if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
        perror("sendto");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("发送ACK...\n");
    printf("连接建立成功。\n");

    // 发送数据包
    send_data_packets(sockfd, server_addr, &pkt_loss_rate, &server_response_time);

    // 发送FIN包
    send_packet.seq_no += 1;
    send_packet.ver = 2;
    send_packet.type = 5; // FIN
    memset(send_packet.data, 0, sizeof(send_packet.data));

    if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
        perror("sendto");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("发送FIN...\n");

    // 接收FIN-ACK包
    if (recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        perror("recvfrom");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if (recv_packet.type != 3) {
        printf("终止连接失败。收到意外的包类型：%d\n", recv_packet.type);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("收到FIN的ACK。\n");

    // 发送最终ACK
    send_packet.seq_no += 1;
    send_packet.type = 3; // ACK
    memset(send_packet.data, 0, sizeof(send_packet.data));

    if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
        perror("sendto");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("发送最终ACK。连接终止。\n");

    // 记录结束时间并计算总响应时间
    gettimeofday(&end_time, NULL);
    server_response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + (end_time.tv_usec - start_time.tv_usec) / 1000.0;
    printf("整体响应时间：%.2f ms\n", server_response_time);

    // 输出丢包率
    printf("实际丢包率：%.2f%%\n", pkt_loss_rate * 100.0);

    close(sockfd);
    return 0;
}

void send_data_packets(int sockfd, struct sockaddr_in server_addr, double *pkt_loss_rate, double *server_response_time) {
    Packet send_packet, recv_packet;
    socklen_t addr_len = sizeof(server_addr);
    struct timeval start, end;
    double rtts[12];
    int received_packets = 0;
    int total_packets = 12;
    int retransmissions = 0;
    int i, j;

    for (i = 0; i < total_packets; i++) {
        int retries = 2;  // 允许最多2次重传
        while (retries > 0) {
            send_packet.seq_no = i + 1;
            send_packet.ver = 2;
            send_packet.type = 4; // 数据包
            memset(send_packet.data, 'A' + i, sizeof(send_packet.data)); // 使用特定字符填充数据

            // 发送数据包
            gettimeofday(&start, NULL);
            if (rand() / (double)RAND_MAX >= LOSS_RATE) { // 模拟丢包
                if (sendto(sockfd, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
                    perror("sendto");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                printf("发送数据包 %d...\n", send_packet.seq_no);
            } else {
                printf("模拟丢包，未发送数据包 %d\n", send_packet.seq_no);
                retries--;
                retransmissions++;
                continue;
            }

            // 接收ACK包
            if (recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&server_addr, &addr_len) < 0) {
                perror("recvfrom");
                retries--;
                retransmissions++;
                printf("重试数据包 %d，剩余重试次数：%d...\n", send_packet.seq_no, retries);
                continue;
            }

            // 检查接收到的包是否是预期的ACK
            if (recv_packet.type == 3 && recv_packet.seq_no == send_packet.seq_no) {
                gettimeofday(&end, NULL);
                double rtt = calculate_rtt(start, end);
                rtts[received_packets] = rtt;
                received_packets++;
                printf("收到数据包 %d 的ACK。RTT：%.2f ms，服务器时间：%s\n", recv_packet.seq_no, rtt, recv_packet.data);
                break;
            } else {
                retries--;
                retransmissions++;
                printf("重试数据包 %d，剩余重试次数：%d...\n", send_packet.seq_no, retries);
            }
        }

        if (retries == 0) {
            printf("数据包 %d 请求超时。\n", send_packet.seq_no);
        }
    }

    // 计算丢包率
    *pkt_loss_rate = (double)retransmissions / total_packets;

    // 计算并打印统计信息
    double total_rtt = 0, max_rtt = rtts[0], min_rtt = rtts[0], mean_rtt, std_dev = 0;
    for (i = 0; i < received_packets; i++) {
        total_rtt += rtts[i];
        if (rtts[i] > max_rtt) max_rtt = rtts[i];
        if (rtts[i] < min_rtt) min_rtt = rtts[i];
    }
    mean_rtt = total_rtt / received_packets;
    for (i = 0; i < received_packets; i++) {
        std_dev += pow(rtts[i] - mean_rtt, 2);
    }
    std_dev = sqrt(std_dev / received_packets);

    printf("RTT统计：\n");
    printf("最大RTT：%.2f ms\n", max_rtt);
    printf("最小RTT：%.2f ms\n", min_rtt);
    printf("平均RTT：%.2f ms\n", mean_rtt);
    printf("RTT标准差：%.2f ms\n", std_dev);
    printf("重传次数：%d\n", retransmissions);
}

double calculate_rtt(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
}
