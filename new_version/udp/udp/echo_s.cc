

#include "echo_s.h"

#define IP_ADDR "10.0.18.166"
#define IP_PORT 8000
#define LISTENS 5

/* static member */
SOCKET echoServer::ListenFd_ = -1;
char echoServer::buf[BUFSIZ];
char* echoServer::ConnectIP_;
uint16_t echoServer::ConnectPort_;

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


echoServer::echoServer(int port, int Method) 
        : Port_(port), Method_((METHOD)Method), Clients_(std::vector<SOCKET>(FD_SETSIZE, -1))
        , NReady_(0)		// , MaxFd_(-1)
{
    if(Init() != 0){
        perror("Init Socket Error\n");
    }
	// if(Method == UDP)
    KcpInit();
}

echoServer::~echoServer(){
	WSACleanup();
    closesocket(ListenFd_);
    ikcp_release(KcpServer_);
}

int echoServer::Init(){
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
    
	int ret;
    struct sockaddr_in server;

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

    BOOL flag = 1;
    ret = setsockopt(ListenFd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(flag));
    if(ret != 0){
        perror("setsockopt Error\n");
        return -1;
    }
	// int flag_ = 1;
    // ioctlsocket(ListenFd_, FIONBIO, (unsigned long*)&flag_);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(Port_);
    
    ret = bind(ListenFd_, (sockaddr*)&server, sizeof server);
    if(ret != 0){
        perror("Socket Bind Error\n");
        return -1;
    }
    // tcp listen
    if(Method_ == TCP){
        ret = listen(ListenFd_, LISTENS);
    }

    // MaxFd_ = ListenFd_;
    FD_ZERO(&AllSet_);
    FD_ZERO(&ReadSet_);
    FD_SET(ListenFd_, &AllSet_);
    return 0;
}

int echoServer::start(){

    // int count;
    // int client[FD_SETSIZE];

    while(true){
        
        // select
        ReadSet_ = AllSet_;
		// count = select(MaxFd_ + 1, &ReadSet_, NULL, NULL, NULL);
		int count = select(0, &ReadSet_, NULL, NULL, NULL);
        NReady_ = count;

        // 更新状态
		if(Method_ == UDP)
        ikcp_update(KcpServer_, iclock());

        if(count == SOCKET_ERROR) {
			printf("select task finish,called failed:%d!\n", WSAGetLastError());
            return -1;
        }
        
        for(int i = 0; i < count; i++)
        {
            if(FD_ISSET(ListenFd_, &ReadSet_)){
                if(Method_ == TCP){
                    HandleConnect();
                }
                else{// UDP
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

// 仅处理TCP协议
void echoServer::HandleConnect() {
    size_t i = 0;
    struct sockaddr_in client;
	int client_len = sizeof(client);
    SOCKET clientfd = accept(ListenFd_, (sockaddr*)&client, &client_len);
    if(clientfd == INVALID_SOCKET){ 
		printf("Accept,called failed:%d!\n", WSAGetLastError());
    }
	
	std::cout << inet_ntoa(client.sin_addr) << std::endl;
	std::cout << ntohs(client.sin_port) << std::endl;
	
	FD_SET(clientfd, &AllSet_);
    for(i = 0; i < FD_SETSIZE; i++){
        if(Clients_[i] == -1){
            Clients_[i] = clientfd;
            break;
        }
    }
    // FD_SET(clientfd, &AllSet_);

    //Update Status
	/*
	if(clientfd > MaxFd_){
		MaxFd_ = clientfd;
	}
	*/
    

    if(MaxClis_ < i){
        MaxClis_ = i;
    }
    --NReady_;
    return;
}

void echoServer::Echo(int fd){
    int ret;
    for(int i = 0; i <= MaxClis_; i++){
        SOCKET clientfd = Clients_[i];
        if(clientfd < 0) continue;
        if(FD_ISSET(clientfd, &ReadSet_))
        {
            int res = recv(clientfd, buf, BUFSIZ	, 0);
            if(res <= 0){
                closesocket(clientfd);
                Clients_[i] = -1;
                FD_CLR(clientfd, &AllSet_);
                printf("close: %d", clientfd);
            }
            else{
                ret = send(clientfd, buf, (size_t)res, 0);
                if(ret == -1){
                    perror("tcp Echo Error\n");
                }
            }
            // last event
            if(--NReady_ == 0) break;
        }
        
    }

    return;
}

// 触发事件，接受UDP报文传入KCP中.
void echoServer::UdpEcho(int i){
    SOCKET clientfd = ListenFd_;
    struct sockaddr_in addr;
	int len = sizeof(addr);
    
    memset(buf, '\0', BUFSIZ);	
    int res = recvfrom(clientfd, buf, BUFSIZ, 0,(struct sockaddr*)&addr, &len);
    int rput = ikcp_input(KcpServer_, buf, res);
	ConnectIP_ = inet_ntoa(addr.sin_addr);
	ConnectPort_ = htons(addr.sin_port);
	std::cout << inet_ntoa(addr.sin_addr) << std::endl; 
	std::cout << htons(addr.sin_port) << std::endl;
	std::cout << "rput: " << rput << std::endl;
	std::cout << "res: " << res << std::endl;

    if(res < 0){
        std::cout << "res < 0\n";
        return;
    }

    int ret = ikcp_recv(KcpServer_, buf, BUFSIZ);
    std::cout << "ret: " << ret << std::endl;
    int r = ikcp_send(KcpServer_, buf, ret);

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
    // Destination IP
    addr.sin_addr.s_addr = inet_addr(echoServer::ConnectIP_);
    // Destination PORT
    addr.sin_port = htons(echoServer::ConnectPort_);

    sendto(ListenFd_, buf, len, 0,(struct sockaddr*)&addr, sizeof(addr));
    return 0;
}

void echoServer::KcpInit(){
    KcpServer_ = ikcp_create(0x11223344, (void*)1);
    ikcp_setoutput(KcpServer_, echoServer::UDP_MSG_SENDER);
    ikcp_wndsize(KcpServer_, 2048, 2048);
	ikcp_nodelay(KcpServer_, 1, 10, 2, 1);		// Fast Mode
	// ikcp_nodelay(KcpServer_, 0, 40, 0, 0);			// Normal Mode
}

