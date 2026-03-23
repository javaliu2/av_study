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
    char localIp[100];

    sscanf(url, "rtsp://%[^:]:", localIp);

    sprintf(sdp, "v=0\r\n"
            "o=- 9%ld 1 IN IP4 %s\r\n"
            "t=0 0\r\n"
            "a=control:*\r\n"
            "m=video 0 RTP/AVP/TCP 96\r\n"
            "a=rtpmap:96 H264/90000\r\n"
            "a=control:track0\r\n"
            "m=audio 1 RTP/AVP/TCP 97\r\n"
            "a=rtpmap:97 mpeg4-generic/44100/2\r\n"
            "a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210;\r\n"
            "a=control:track1\r\n",
            time(NULL), localIp
    );
    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
        "Content-Base: %s\r\n"
        "Content-type: application/sdp\r\n"
        "Content-length: %zu\r\n\r\n"
        "%s",
        cseq,
        url,
        strlen(sdp),
        sdp
    );
    return 0;
}

int handleCmd_SETUP(char* result, int cseq, int clientRtpPort) {
    if (cseq == 3) {
        sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
        "Session: 66334873\r\n"
        "\r\n", 
        cseq);
    } else if (cseq == 4) {
        sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n"
        "Session: 66334873\r\n"
        "\r\n", 
        cseq);
    }
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