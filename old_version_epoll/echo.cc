

#include "echo.h"

#define MAX_EVENT 1024

int echoServer::ListenFd_ = -1;

/* get system time */
static inline void itimeofday(long *sec, long *usec)
{
	#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

static inline IINT64 iclock64(void)
{
	long s, u;
	IINT64 value;
	itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

static inline IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

struct epoll_event* echoServer::events =  new epoll_event[MAX_EVENT];
char echoServer::buf[BUFSIZ];

echoServer::echoServer(int port, int Method) : Port_(port), Method_((METHOD)Method)
{
    // echoServer::EpollFd_ =  epoll_create(1024);
    if(Init() != 0){
        perror("Init Socket Error\n");
    }
    KcpInit();
}

echoServer::~echoServer(){
    close(EpollFd_);
    close(ListenFd_);
    ikcp_release(KcpServer_);
}

int echoServer::Init(){
    int ret;
    struct sockaddr_in server;
    // char buf[BUFSIZ];

    if(Method_ == TCP){
        ListenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    }
    else if(Method_ == UDP){
        ListenFd_ = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if(ListenFd_ == -1){
        perror("Socket Init Error\n");
        return -1;
    }

    int flag = 1;
    ret = setsockopt(ListenFd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    if(ret != 0){
        perror("setsockopt Error\n");
        return -1;
    }
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(Port_);
    
    ret = bind(ListenFd_, (sockaddr*)&server, sizeof server);
    if(ret != 0){
        perror("Socket Bind Error\n");
        return -1;
    }
    if(Method_ == TCP){
        ret = listen(ListenFd_, 5);
    }
    
    struct epoll_event event;
    event.data.fd = ListenFd_;
    // LT模式
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
    // ET模式
    // event.events = EPOLLIN | EPOLLET;
    
    echoServer::EpollFd_ =  epoll_create(1024);
    if(epoll_ctl(EpollFd_, EPOLL_CTL_ADD, ListenFd_, &event) < 0){
        perror("Epoll Add Error\n");
        return -1;
    }
    return 0;
}

int echoServer::start(){

    while(true){
        int count;
        count = epoll_wait(EpollFd_, events, MAX_EVENT, -1);
        // FIXME : unsign int iclock()
        // 更新状态
        ikcp_update(KcpServer_, iclock());

        if(count == -1) {
            perror("epoll_wait failed.\n");
            return -1;
        }
        
        for(int i = 0; i < count; i++)
        {
            if(events[i].data.fd == ListenFd_){
                if(Method_ == TCP){
                    HandleConnect();
                }
                else{
                    HandleEcho(i);
                }
            }
            else{
                if(Method_ == TCP){
                    HandleEcho(i);
                }
                else{
                    perror("something wrong?\n");
                }
            }
        }
    }
    // free(events);
}

// 仅适用于TCP协议
void echoServer::HandleConnect(){
    struct sockaddr_in client;
    epoll_event event;
    socklen_t client_len;
    int clientfd = accept(ListenFd_, (sockaddr*)&client, &client_len);
    if(clientfd < 0){ 
        perror("Accept failed \n");
    }
    event.data.fd = clientfd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    if(epoll_ctl(EpollFd_, EPOLL_CTL_ADD, clientfd, &event) < 0){
        perror("Epoll Add Error\n");
    }
}

void echoServer::Echo(int i){
    int ret;
    struct epoll_event event;
    int res = recv(events[i].data.fd, buf, BUFSIZ, 0);
    printf("%d", res);
    if(res <= 0){
        close(events[i].data.fd);
        epoll_ctl(EpollFd_, EPOLL_CTL_DEL,
                    events[i].data.fd, &event);
        return;
    }
    else{
        ret = send(events[i].data.fd, buf, (size_t)res, 0);
        if(ret == -1){
            perror("ret failed\n");
        }
    }
}

// Epoll触发事件，接受UDP报文传入KCP中.
void echoServer::UdpEcho(int i){
    int ret; 
    int res = 0;
    int clientfd = events[i].data.fd;
    // client addr
    struct sockaddr_in addr;
    struct epoll_event event;
    socklen_t len;
    
    // 接受客户端所有的数据包
    /*
    while (true)
    {
        memset(buf, 0, BUFSIZ);
        res = recvfrom(clientfd, buf, BUFSIZ, 0,(struct sockaddr*)&addr, &len);
        if(res == -1){
            break;
        }
        ikcp_input(KcpServer_, buf, res);
        ikcp_update(KcpServer_, iclock());
    }
    */

    memset(buf, 0, BUFSIZ);
    res = recvfrom(clientfd, buf, BUFSIZ, 0,(struct sockaddr*)&addr, &len);
    ikcp_input(KcpServer_, buf, res);
    
    
    /*
    while (true) {
        ret = ikcp_recv(KcpServer_, buf, 10);
        // 没有收到包就退出
        if (ret < 0){
             break;
        }
        // 如果收到包就回射
        else{
            // 更新sockaddr_in client
            ikcp_send(KcpServer_, buf, ret);
        }
    }
    
    */

    // 当前传输结束
    if(res < 0){
        event.data.fd = ListenFd_;
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
        epoll_ctl(EpollFd_, EPOLL_CTL_DEL, clientfd, &event);
        epoll_ctl(EpollFd_, EPOLL_CTL_ADD, clientfd, &event);
        return;
    }
    //执行回射
    ret = ikcp_recv(KcpServer_, buf, 10);
    if(ret < 0){
        event.data.fd = ListenFd_;
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
        epoll_ctl(EpollFd_, EPOLL_CTL_DEL, clientfd, &event);
        epoll_ctl(EpollFd_, EPOLL_CTL_ADD, clientfd, &event);
        return;
    }
    ikcp_send(KcpServer_, buf, ret);
    
    //处理完毕
}

void echoServer::HandleEcho(int i){
    if (Method_ == TCP)
    {
        Callback_ = std::bind(&echoServer::Echo, this,std::placeholders::_1);
    }
    else if(Method_ == UDP){
        // Callback_ = std::bind(...);
        Callback_ = std::bind(&echoServer::UdpEcho, this,std::placeholders::_1);
    }
    Callback_(i);
}

// OUTPUT
int echoServer::UDP_MSG_SENDER(const char *buf, int len, ikcpcb *kcp, void *user){
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    // localhost
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // client port
    addr.sin_port = htons(8889);

    sendto(ListenFd_, buf, len, 0,(struct sockaddr*)&addr, sizeof(addr));
    return 0;
}

void echoServer::KcpInit(){
    KcpServer_ = ikcp_create(0x11223344, (void*)1);
    // 输出的回调函数 发udp报文
    ikcp_setoutput(KcpServer_, echoServer::UDP_MSG_SENDER);
    ikcp_wndsize(KcpServer_, 2048, 2048);
    ikcp_nodelay(KcpServer_, 1, 10, 2, 1);
}

