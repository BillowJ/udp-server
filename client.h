#ifndef CLIENT_H_
#define CLIENT_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include "ikcp.h"

class echoClient{
public:
    echoClient(int maxCount);
    ~echoClient();
    void start();
    static ikcpcb* KcpClient_;
private:
    void Init();
    void KcpInit();
    static int UDP_OUTPUT(const char *buf, int len, ikcpcb *kcp, void *user);
private:
    int Count_;
    int MaxCount_;
    static int ClientFd_;
    // static char buf[BUFSIZ];
};

#endif
