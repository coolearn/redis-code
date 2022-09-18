#include <stdio.h>
#include "socket_struct.h"
#include "../anet.c"

int main(int argc,char *argv[]){

    char err[256];
    // char ch = 'a';
    char snd_buf[2048];
    sendInfo info; //定义结构体变量
    char *infoFrom = "hello";
    char *infoTo = "world";

    // infoFrom = strcpy(infoFrom,"test");

    // if(argc>1){
    //     ch = argv[1][0];
    // }
    // printf("向服务器写字符:%c\n",ch);
    printf("This is client,please input message:\n");

    int socketfd = anetTcpConnect(err,"192.168.50.190",8088);
    //从键盘读取用户输入的数据，并写入info.info_content
    memset(info.info_content,0,sizeof(info.info_content));//清空缓存
    info.info_length=read(STDIN_FILENO,info.info_content,1024) - 1;//读取用户输入的数据

    strcpy(info.info_from,infoFrom);
    strcpy(info.info_to,infoTo);

    
    
    //清空发送缓存，不清空的话可能导致接收时产生乱码，
    //或者如果本次发送的内容少于上次的话，snd_buf中会包含有上次的内容
    memset(snd_buf,0,2048);
    
    printf("info sizeof = %lu \n",sizeof(info));
    memcpy(snd_buf,&info,sizeof(info)); //结构体转换成字符串

    anetWrite(socketfd,snd_buf,sizeof(snd_buf));//发送信息

    // anetWrite(socketfd,&ch,1);
    // anetRead(socketfd,&ch,1);

     for (const char* p = snd_buf; *p; ++p)
    {
        printf("%02x", *p);
    }
    printf("\n");
    
    printf("char from server = %s\n",snd_buf);
    
    // 关闭fd
    close(socketfd);

    return 0;


}