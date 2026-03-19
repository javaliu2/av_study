#pragma once

#define SERVER_PORT 8554
#define SERVER_RTP_PORT 55532
#define SERVER_RTCP_PORT 55533
#define BUF_MAX_SIZE (1024*1024)

int createTcpSocket();

int createUdpSocket();

int bindSocketAddr(int sockfd, const char* ip, int port);
int acceptClient(int sockfd, char* ip, int* port);

int handleCmd_OPTIONS(char* result, int cseq);

int handleCmd_DESCRIBE(char* result, int cseq, const char* url);

int handleCmd_SETUP(char* result, int cseq, int clientRtpPort);

int handleCmd_PLAY(char* result, int cseq);