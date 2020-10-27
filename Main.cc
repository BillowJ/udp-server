#include "client.h"



int main(int argc, char** argv)
{
    // int num = 0;
    int num = atoi(argv[1]);
    echoClient* client = new echoClient(num);
    client -> start();
    delete client;
}