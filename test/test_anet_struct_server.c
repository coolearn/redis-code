#include <stdio.h>
#include "socket_struct.h"
#include "../anet.c"


int main(){

    char err[256];
    int socketfd;
    int clientfd;
    socketfd = anetTcpServer(err,8088,"192.168.50.190");

    while(1){

        printf("server wating \n");
        char *clientAddr;
        int prot;
        char recv_buf[1024];
        sendInfo clt; //定义结构体变量
        clientfd = anetAccept(err,socketfd,clientAddr,&prot);

        printf("clientfd=%d,sizeof=%lu\n",clientfd,sizeof(clt));

        memset(recv_buf,'z',1024);//清空缓存

        anetRead(clientfd,recv_buf,1024);
        // recv(socketfd,recv_buf,1024,0 );//读取数据
        memset(&clt,0,sizeof(clt));//清空结构体

        memcpy(&clt,recv_buf,sizeof(clt));//把接收到的信息转换成结构体
        
        clt.info_content[clt.info_length] = "";
        //消息内容结束，没有这句的话，可能导致消息乱码或输出异常
        //有网友建议说传递的结构体中尽量不要有string类型的字段，估计就是串尾符定位的问题
        
        // if(clt.info_content) //判断接收内容并输出
        printf("nclt.info_from is %snclt.info_to is %snclt.info_content is%snclt.info_length is %dn",clt.info_from,clt.info_to,clt.info_content,clt.info_length);
        //至此，结构体的发送与接收已经顺利结束了
        
        // printf("server wating \n");
        // char ch;
        // char *clientAddr;
        // int prot;
        // int clientfd = anetAccept(err,socketfd,clientAddr,&prot);
        // ch++;
        // anetRead(clientfd,&ch,1);
        close(clientfd);
    }

    return 0;
}