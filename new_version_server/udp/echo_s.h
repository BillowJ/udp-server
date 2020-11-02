#ifndef __ECHO_H__
#define __ECHO_H__

#include <iostream>
#include <vector>
#include <queue>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>

#include <string.h> //memset

#include <sys/types.h> //sendto
#if defined(__linux__)
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>
#define SOCKET int
#endif // !__linux__

#ifdef WIN32
typedef int socklen_t;
typedef int ssize_t;
#endif

#include <time.h>
#include <functional>
#include <winsock2.h>
#include "ikcp.h"
#include "stdafx.h"
#pragma comment (lib, "ws2_32.lib")

#define FD_SETSIZE 1024
#define BUFSIZ 1024

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
	friend int UDP_MSG_SENDER(const char *buf, int len, ikcpcb *kcp, void *user);
    void HandleConnect();
    void HandleEcho(int);
    void Echo(int);
    void UdpEcho(int);
    void KcpInit();
    int Init();
    static int UDP_MSG_SENDER(const char *buf, int len, ikcpcb *kcp, void *user);
private:
    int Port_;
    int MaxClis_;
    int NReady_;
	// SOCKET MaxFd_;

    std::function<void(int)> Callback_;
    METHOD Method_;
    
    fd_set ReadSet_;
    fd_set AllSet_;
    std::vector<SOCKET> Clients_;
	static uint16_t ConnectPort_;
	static char* ConnectIP_;
	static SOCKET ListenFd_;
    static char buf[BUFSIZ];
    
    // KCP
    ikcpcb* KcpServer_;
};

#endif