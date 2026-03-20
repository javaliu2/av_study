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
    return 0;
}

int handleCmd_SETUP(char* result, int cseq, int clientRtpPort) {
    return 0;
}

int handleCmd_PLAY(char* result, int cseq) {
    
    return 0;
}