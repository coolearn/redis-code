#include <stdio.h>
#include "../anet.c"

int main(int argc,char *argv[]){

    char err[256];
    char ch ='a';
    if(argc>1){
        ch = argv[1][0];
    }
    printf("向服务器写字符:%c\n",ch);
    int socketfd = anetTcpConnect(err,"192.168.50.190",8088);
    anetWrite(socketfd,&ch,1);
    anetRead(socketfd,&ch,1);
    
    printf("char from server = %c\n",ch);
    
    // 关闭fd
    close(socketfd);

    return 0;


}