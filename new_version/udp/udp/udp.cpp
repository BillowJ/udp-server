// udp.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "echo_s.h"
#include <iostream>
int main()
{
	// 1. tcp 2.udp
	echoServer* server = new echoServer(8000, 2);
	server->start();
	delete server;
	system("pause");
    return 0;
}

