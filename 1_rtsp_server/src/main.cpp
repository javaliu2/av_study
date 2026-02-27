/**
 * code refenence from https://gitee.com/Vanishi/BXC_RtspServer_study/blob/master/study1/main.cpp
 * 仅用于学习
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <string>

// 以下两行是MSVC(micro soft visual c++编译器)专属的预处理指令
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")  // 告诉编译器自动链接ws2_32.lib库
#pragma warning(disable : 4096)  // 关闭MSVC的4096号编译
#endif

#define SERVER_PORT 8554
#define SERVER_RTP_PORT 55532
#define SERVER_RTCP_PORT 55533

static int createTcpSocket() {
    int sockfd;
    int on = 1;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        return -1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    return sockfd;
}

static int bindSocketAddr(int sockfd, const char* ip, int port) {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0)
        return -1;
    return 0;
}

static int acceptClient(int sockfd, char* ip, int* port) {
    int clientfd;
    socklen_t len = 0;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);

    clientfd = accept(sockfd, (struct sockaddr*)&addr, &len);
    if (clientfd < 0)
        return -1;
    
    strcpy(ip, inet_ntoa(addr.sin_addr));
    *port = ntohs(addr.sin_port);

    return clientfd;
}

static int handleCmd_OPTIONS(char* result, int cseq) {
    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\nPublic: OPTIONS, DESCRIBE, SETUP, PLAY\r\n\r\n", cseq);
    return 0;
}

static int handleCmd_DESCRIBE(char* result, int cseq, const char* url) {
    char sdp[500];
    char localIp[64] = {0};
    // %63[^:/]: []中的是要匹配的字符，^表示不匹配这些字符，63指定匹配的字符的最大长度，防止溢出
    // rtsp:// 固定匹配; 从//之后连续匹配最多63个字符直至遇到:或者/
    // 即从url中截取主机名
    sscanf(url, "rtsp://%63[^:/]", localIp);
    printf("%s\n", localIp);  // output ok
    sprintf(sdp, "v=0\r\no=- 9%ld 1 IN IP4 %s\r\nt=0 0\r\na=control:*\r\nm=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\na=control:track0\r\n", 
        time(NULL), localIp);
    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
    "Content-Base: %s\r\n"
    "Content-type: application/sdp\r\n"
    "Content-length: %zu\r\n\r\n"
    "%s",
    cseq, url, strlen(sdp), sdp);  // %zu是用来处理size_t类型变量的格式符
    return 0;
}

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
    "CSeq: %d\r\n"
    "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
    "Session: 66334873\r\n\r\n",
    cseq, clientRtpPort, clientRtpPort + 1, SERVER_RTP_PORT, SERVER_RTCP_PORT);
    return 0;
}

static int handleCmd_PLAY(char* result, int cseq) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
    "CSeq: %d\r\n"
    "Range: npt=0.000-\r\n"
    "Session: 66334873; timeout=10\r\n\r\n",
    cseq);
    return 0;
}

static void doClient(int clientSockfd, const char* clientIP, int clientPort) {
    char method[40];
    char url[100];
    char version[40];
    int CSeq;

    int clientRtpPort, clientRtcpPort;
    char* rBuf = (char*)malloc(10000);
    char* sBuf = (char*)malloc(10000);

    while (true) {
        int recvLen;

        recvLen = recv(clientSockfd, rBuf, 2000, 0);
    }

}

int main() {
    const char* url = "rtsp://192.168.1.10:8554/live";
    handleCmd_DESCRIBE(0, 0, url);
    const char* url2 = "rtsp://example.com/live";
    handleCmd_DESCRIBE(0, 0, url2);
    return 0;
}