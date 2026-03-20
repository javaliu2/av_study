#include "rtsp_base.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <windows.h>
#include <string>
#include "logger.h"
#include "rtp.h"



int handleCmd_OPTIONS(char* result, int cseq) {
    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\nPublic: OPTIONS, DESCRIBE, SETUP, PLAY\r\n\r\n", cseq);
    return 0;
}

int handleCmd_DESCRIBE(char* result, int cseq, const char* url) {
    char sdp[500];
    char localIp[64] = {0};
    // %63[^:/]: []中的是要匹配的字符，^表示不匹配这些字符，63指定匹配的字符的最大长度，防止溢出
    // rtsp:// 固定匹配; 从//之后连续匹配最多63个字符直至遇到:或者/
    // 即从url中截取主机名
    sscanf(url, "rtsp://%63[^:/]", localIp);
    printf("%s\n", localIp);  // output ok
    sprintf(sdp, "v=0\r\no=- 9%ld 1 IN IP4 %s\r\nt=0 0\r\na=control:*\r\nm=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/1200000\r\na=control:track0\r\n", 
        time(NULL), localIp);
    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
    "Content-Base: %s\r\n"
    "Content-type: application/sdp\r\n"
    "Content-length: %zu\r\n\r\n"
    "%s",
    cseq, url, strlen(sdp), sdp);  // %zu是用来处理size_t类型变量的格式符
    return 0;
}

int handleCmd_SETUP(char* result, int cseq, int clientRtpPort) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
    "CSeq: %d\r\n"
    "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
    "Session: 66334873\r\n\r\n",
    cseq, clientRtpPort, clientRtpPort + 1, SERVER_RTP_PORT, SERVER_RTCP_PORT);
    return 0;
}

int handleCmd_PLAY(char* result, int cseq) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
    "CSeq: %d\r\n"
    "Range: npt=0.000-\r\n"
    "Session: 66334873; timeout=10\r\n\r\n",
    cseq);
    return 0;
}