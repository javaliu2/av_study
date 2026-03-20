#include <cstdint>
#include <cstdio>

bool startCode001(char* buf);

bool startCode0001(char* buf);
char* findNextStartCode(char* buf, int len);
int getFrameFromH264File(FILE* fp, char* frame, int size);
int rtpSendH264Frame(int serverRtpSockfd, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize);