#ifndef ECHO_H_
#define ECHO_H_

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h> //memset
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h> //sendto
#include <sys/socket.h>
#include <functional>

#include "ikcp.h"

class echoServer{
public:
    enum METHOD{
        TCP = 1,
        UDP = 2
    };
    explicit echoServer(int Port, int method);
    ~echoServer();
    int start();
    void SetCallback(std::function<void(int)> func) { Callback_ = std::move(func);}

private:
    void HandleConnect();
    void HandleEcho(int);
    void Echo(int);
    void UdpEcho(int);
    void KcpInit();
    int Init();
    static int UDP_MSG_SENDER(const char *buf, int len, ikcpcb *kcp, void *user);
private:
    int Port_;
    static int ListenFd_;
    int EpollFd_;
    std::function<void(int)> Callback_;
    METHOD Method_;
    static struct epoll_event* events;
    static char buf[BUFSIZ];
    // KCP
    ikcpcb* KcpServer_;
};

#endif