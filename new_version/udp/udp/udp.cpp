// udp.cpp : �������̨Ӧ�ó������ڵ㡣
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

