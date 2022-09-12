#include <stdio.h>
#include "../anet.c"

int main(){

    char err[256];
    int socketfd;
    socketfd = anetTcpServer(err,8088,"192.168.50.190");
    while(1){
        printf("server wating \n");
        char ch;
        char *clientAddr;
        int prot;
        int clientfd = anetAccept(err,socketfd,clientAddr,&prot);
        ch++;
        anetRead(clientfd,&ch,1);
        close(clientfd);
    }
    return 0;
}