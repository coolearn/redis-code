#include <stdio.h>
#include "../anet.h"
#include "../anet.c"

int main(int argc,char *argv[]){

    char *host="shaodenangdeMBP";
    char err[256];
    if(argc > 1 ){
        host = argv[1];
    }
    // struct sockaddr_in sa;
    // sa.sin_family = AF_INET; // ipv4
    // if (inet_aton(host, &sa.sin_addr) == 0) {
    //     struct hostent *he;
    //     he = gethostbyname(host); // 获取IP
    //     if (he == NULL) {
    //         anetSetError(err, "can't resolve: %s\n", host);
    //         return ANET_ERR;
    //     }
    //     printf("得到地址:%ld\n",he->h_addr); // he->h_addr_list[0]
    //     // 赋值到sin_addr 
    //     memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));

    //     printf("得到地址:%s\n",inet_ntoa(sa.sin_addr));

    // }else {
    //     printf("ip地址:%s\n",host);
    // }

    // 编译
    // gcc -o  test_anet_host_resolve test_anet_host_resolve.c
    // 执行 - 无参数
    // ./test_anet_host_resolve 
    // 参数
    // ./test_anet_host_resolve  127.0.0.1
    // ./test_anet_host_resolve  center


    // 调用 anet.c中的方法 , 实现相同功能
    int res;
    // char err[256];
    char ipbuf[256];
    res = anetResolve(err,host,ipbuf);
    printf("转换后的ip地址为：%s\n",ipbuf);

    return 0;
}