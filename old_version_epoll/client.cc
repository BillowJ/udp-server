
#include "client.h"

#define SERVER_IP "127.0.0.1"

// char buf[BUFSIZ];

int echoClient::ClientFd_ = -1;
ikcpcb* echoClient::KcpClient_;

echoClient::echoClient(int maxCount) : MaxCount_(maxCount), Count_(0) {
    assert(MaxCount_ != 0);
	Init();
    KcpInit();
}

echoClient::~echoClient(){
    close(ClientFd_);
    ikcp_release(KcpClient_);
}

static inline void isleep(unsigned long millisecond)
{
	#ifdef __unix 	/* usleep( time * 1000 ); */
	struct timespec ts;
	ts.tv_sec = (time_t)(millisecond / 1000);
	ts.tv_nsec = (long)((millisecond % 1000) * 1000000);
	/*nanosleep(&ts, NULL);*/
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32)
	Sleep(millisecond);
	#endif
}


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

/* get clock in millisecond 64 */
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

void * handle_update(void *argp)
{
	while (1) {
		isleep(1);
		ikcp_update(echoClient::KcpClient_, iclock());
	}
}


void * udp_msg_sender(void *argp)
{
	int fd = (int)((long) argp);
	socklen_t len;
	int hr;
	char buffer[BUFSIZ];
	struct sockaddr_in clent_addr;
 
	while (1) {
		memset(buffer, 0, BUFSIZ);
		hr = recvfrom(fd, buffer, BUFSIZ, 0, (struct sockaddr*)&clent_addr, &len);
 
		if (hr == -1) {
			//printf("recieve data fail!\n");
			continue;
		}
 
		//printf("ikcp_input: hr: %d\n", hr);
		// 如果 p1收到udp，则作为下层协议输入到kcp1
		ikcp_input(echoClient::KcpClient_, buffer, hr);
	}
}

void echoClient::Init(){
    int ret;
    struct sockaddr_in addr;
    ClientFd_ = socket(AF_INET, SOCK_DGRAM, 0);

    int flag = 1;
    setsockopt(ClientFd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // client port
    addr.sin_port = htons(8889);

    if( bind(ClientFd_, (struct sockaddr*)&addr, sizeof(addr) ) != 0) {
		perror("socket bind fail!\n");
        return;
	}

    if(ClientFd_ == -1){
        perror("Socket Init Error\n");
        return;
    }

}

int echoClient::UDP_OUTPUT(const char *buff, int len, ikcpcb *kcp, void *user){
    struct sockaddr_in ser_addr;
	memset(&ser_addr, 0, sizeof(ser_addr));
	ser_addr.sin_family      = AF_INET;
	ser_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	ser_addr.sin_port        = htons(8888);
    sendto(ClientFd_, buff, len, 0, (struct sockaddr*)&ser_addr, sizeof(struct sockaddr_in));
    return 0;
}


void echoClient::KcpInit(){
    KcpClient_ = ikcp_create(0x11223344, (void*)0);
    // 输出的回调函数 发udp报文
    ikcp_setoutput(KcpClient_, UDP_OUTPUT);
    ikcp_wndsize(KcpClient_, 2048, 2048);
    ikcp_nodelay(KcpClient_, 1, 10, 2, 1);
    KcpClient_ -> rx_minrto = 10;
    KcpClient_ -> fastresend = 1;
}


void echoClient::start(){
    char buf[BUFSIZ];
    // 多次调用
    if(Count_ != 0) Count_;

    IUINT32 current = iclock();
    IUINT32 slap = current + 20;
    IUINT32 index = 0;
    IUINT32 next = 0;
	IINT64 sumrtt = 0;
    int maxrtt = 0;
    int hr;
    
    //thread
    pthread_t recvdata_id;
	// pthread_t update_id;
	pthread_create(&recvdata_id, NULL, udp_msg_sender, (void*)ClientFd_);
	// pthread_create(&update_id, NULL, handle_update, NULL);

    do{
        isleep(1);
        ikcp_update(KcpClient_, iclock());
        current = iclock();

        // 20ms左右发生一次
        for (; current >= slap && Count_ <= MaxCount_; slap += 20) {
			((IUINT32*)buf)[0] = index++;	// 自定义的报文讯号
			((IUINT32*)buf)[1] = current;	// 加入当前的时间

			// 发送上层协议包
			ikcp_send(KcpClient_, buf, 8);
			Count_++;
		}

        while(true){
            hr = ikcp_recv(KcpClient_, buf, 10);
            if(hr < 0) break;
            IUINT32 sn = *(IUINT32*)(buf + 0);
			IUINT32 ts = *(IUINT32*)(buf + 4);
            // RTT
            IUINT32 rtt = current - ts;

            if(sn != next){
                printf("ERROR sn %d<->%d\n", (int)Count_, (int)next);
				return;
            }
            next++;
			sumrtt += rtt;
			// Count_++;
            if (rtt > (IUINT32)maxrtt) maxrtt = rtt;

            printf("[RECV] sn=%d rtt=%d\n", (int)sn, (int)rtt);
        }


    } while(next < MaxCount_);

    printf("avgrtt=%d maxrtt=%d\n", (int)(sumrtt / Count_), (int)maxrtt);
    
	pthread_cancel(recvdata_id);
	pthread_join(recvdata_id, NULL);
	// pthread_join(update_id, NULL);
	return;
}