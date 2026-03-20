int createTcpSocket();
int createUdpSocket();
int bindSocketAddr(int sockfd, const char* ip, int port);
int acceptClient(int sockfd, char* ip, int* port);