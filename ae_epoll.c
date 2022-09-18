/* Linux epoll(2) based ae.c module
 * Copyright (C) 2009-2010 Salvatore Sanfilippo - antirez@gmail.com
 * Released under the BSD license. See the COPYING file for more info. */

#include <sys/epoll.h>

typedef struct aeApiState {
    int epfd; // 保存创建的epoll实例
    struct epoll_event events[AE_SETSIZE]; //存储epoll事件信息
} aeApiState;

// 创建epoll
static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = zmalloc(sizeof(aeApiState));

    if (!state) return -1;
    state->epfd = epoll_create(1024); /* 1024 is just an hint for the kernel */
    if (state->epfd == -1) return -1;
    eventLoop->apidata = state;
    return 0;
}

// 释放epoll
static void aeApiFree(aeEventLoop *eventLoop) {
    aeApiState *state = eventLoop->apidata;

    close(state->epfd);
    zfree(state);
}

// fd 事件描述符
// mask 事件
static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee; // epoll读写事件带入结构体
    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. */
    int op = eventLoop->events[fd].mask == AE_NONE ?
            EPOLL_CTL_ADD : EPOLL_CTL_MOD; // 添加还是修改

    ee.events = 0;
    mask |= eventLoop->events[fd].mask; /* Merge old events */
    if (mask & AE_READABLE) ee.events |= EPOLLIN; //有没有注册读事件 ， 与写入读事件
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT; // 有没有注册写事件 ， 与|写入写事件
    ee.data.u64 = 0; /* avoid valgrind warning */ // 避免一个告警 ？？
    ee.data.fd = fd; // 保存fd
    // 0：epoll实例、1:操作类型（添加、修改）、2、fd、3、事件（读事件、写事件）
    if (epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1;  // 设置epoll_ctl读写事件
    return 0;
}

static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int delmask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee;
    // 场景A
    // 10110 mask       当前前的事件
    // 11000 delmask    要删除的
    // 00111 ～delmask 取反
    // 00110 & mask
    // 最后更新事件为：00110

    // 场景B
    // 11000 mask       当前前的事件
    // 11000 delmask    要删除的
    // 00111 ～delmask 取反
    // 00000 & mask
    // 最后更新事件为：00000 -> AE_NONE
    // 删除事件

    int mask = eventLoop->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;
    if (mask != AE_NONE) {
        // 有事件修改
        epoll_ctl(state->epfd,EPOLL_CTL_MOD,fd,&ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        // 没有事件删除
        epoll_ctl(state->epfd,EPOLL_CTL_DEL,fd,&ee);
    }
}

static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    retval = epoll_wait(state->epfd,state->events,AE_SETSIZE,
            tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if (retval > 0) {
        int j;

        numevents = retval;
        for (j = 0; j < numevents; j++) { // 遍历事件
            int mask = 0;
            struct epoll_event *e = state->events+j; // 获取对应出发的事件

            if (e->events & EPOLLIN) mask |= AE_READABLE; // 添加读标记
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE; // 添加写标记
            eventLoop->fired[j].fd = e->data.fd;           // 设置触发事件的描述福
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}

static char *aeApiName(void) {
    return "epoll";
}
