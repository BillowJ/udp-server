#include "echo.h"


int main(){
    // 1: TCP MODE 
    // 2: UDP MODE
    echoServer* server = new echoServer(8888, 2);
    server ->start();
    delete server;
    return 0;
}