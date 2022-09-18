/* A simple event-driven programming library. Originally I wrote this code
 * for the Jim's event-loop (Jim is a Tcl interpreter) but later translated
 * it in form of a library for easy reuse.
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "ae.h"
#include "zmalloc.h"
#include "config.h"

/* Include the best multiplexing layer supported by this system.
 * The following should be ordered by performances, descending. */
#ifdef HAVE_EPOLL
#include "ae_epoll.c"
#else
    #ifdef HAVE_KQUEUE
    #include "ae_kqueue.c"
    #else
    #include "ae_select.c"
    #endif
#endif

// 初始化
aeEventLoop *aeCreateEventLoop(void) {
    aeEventLoop *eventLoop;
    int i;

    eventLoop = zmalloc(sizeof(*eventLoop));
    if (!eventLoop) return NULL;
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 0;
    eventLoop->stop = 0;
    eventLoop->maxfd = -1;
    eventLoop->beforesleep = NULL;
    if (aeApiCreate(eventLoop) == -1) {
        zfree(eventLoop); //释放空间
        return NULL;
    }
    /* Events with mask == AE_NONE are not set. So let's initialize the
     * vector with it. */
    for (i = 0; i < AE_SETSIZE; i++) // AE_SETSIZE (1024*10) 10240
        eventLoop->events[i].mask = AE_NONE; // 
    return eventLoop;
}

void aeDeleteEventLoop(aeEventLoop *eventLoop) {
    aeApiFree(eventLoop); // 释放 eventLoop -> apidata
    zfree(eventLoop); // 
}

void aeStop(aeEventLoop *eventLoop) {
    eventLoop->stop = 1;
}

// mask 读事件 ｜ 写事件 （可以同时是读写） 、*proc 读写事件
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData)
{
    // eventLoop -> events[fd]  fileEvent 读写可以有读写俩个事件
    // eventLoop->maxfd
    if (fd >= AE_SETSIZE) return AE_ERR;
    aeFileEvent *fe = &eventLoop->events[fd];

    // aeApiAddEvent  -> apidata
    if (aeApiAddEvent(eventLoop, fd, mask) == -1)
        return AE_ERR;
    fe->mask |= mask;
    if (mask & AE_READABLE) fe->rfileProc = proc;
    if (mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData; // 相关的处理事件数据
    if (fd > eventLoop->maxfd) // 检查是否比maxfd更大，设置最大的fd
        eventLoop->maxfd = fd;
    return AE_OK;
}

void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask)
{
    if (fd >= AE_SETSIZE) return;
    aeFileEvent *fe = &eventLoop->events[fd];

    if (fe->mask == AE_NONE) return;
    // mask = 11 读写(fd-mask)
    // mask = 01 0写（参数-mask）
    // mask = 10 取反
    // mask = 10 和(fd-mask)&
    // 得到最终的10 保留读事件
    fe->mask = fe->mask & (~mask); // 将对应的文件描述符的指定事件删除
    // 如果删除的fd是最大的 （全部清除的情况下 fe->mask == AE_NONE）
    if (fd == eventLoop->maxfd && fe->mask == AE_NONE) {
        /* Update the max fd */
        int j;

        // 从后往前找fd，找到最后一个不等于AE_NONE的fd，更新maxfd
        for (j = eventLoop->maxfd-1; j >= 0; j--) 
            if (eventLoop->events[j].mask != AE_NONE) break;
        eventLoop->maxfd = j;
    }
    aeApiDelEvent(eventLoop, fd, mask);
}

static void aeGetTime(long *seconds, long *milliseconds)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;  // 时间戳
    *milliseconds = tv.tv_usec/1000; // 计算额外的微秒数
}

static void aeAddMillisecondsToNow(long long milliseconds, long *sec, long *ms) {
    long cur_sec, cur_ms, when_sec, when_ms;

    aeGetTime(&cur_sec, &cur_ms);
    when_sec = cur_sec + milliseconds/1000;  // 1003ms 
    when_ms = cur_ms + milliseconds%1000;   // 3ms 
    if (when_ms >= 1000) { // 当前毫秒为980ms + 3ms > 1000了，需要在秒上处理
        when_sec ++;
        when_ms -= 1000;
    }
    *sec = when_sec;
    *ms = when_ms;
}

long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds, // 20s后超时
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc)
{
    // xx++  ，先将值符给id，然后再自增
    long long id = eventLoop->timeEventNextId++; // timeEventNextId 表示的是下一个id的值，id是当前的
    aeTimeEvent *te;  

    te = zmalloc(sizeof(*te)); // 创建节点，申请内存
    if (te == NULL) return AE_ERR;
    te->id = id; // 0 , 1 ,2 ,3 
    aeAddMillisecondsToNow(milliseconds,&te->when_sec,&te->when_ms); // 计算超时后的时间
    te->timeProc = proc; // 超时回调函数
    te->finalizerProc = finalizerProc; // 结束时回调的函数
    te->clientData = clientData;
    // head -> node1 -> node3 添加链表的方式1，在最后追加，缺点时间复杂度为O(N)
    // 在节点头添加 O(1)
    te->next = eventLoop->timeEventHead;  // head -> node1 -> node2 内存中的链表，新增节点node3. head和node1可以理解为是一个节点
    eventLoop->timeEventHead = te;        // node3->next = head , node3 -> node1 -> node2
    return id;                            // 头节点指向node3 head = node3 , head -> node3 -> node1 -> node2
}                                         // head -> node3 -> NULL  最开始，没有任何事件

int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id) // O(N)
{
    aeTimeEvent *te, *prev = NULL;

    te = eventLoop->timeEventHead; // 头
    while(te) {
        if (te->id == id) {
            if (prev == NULL)  // 为空说明是头
                eventLoop->timeEventHead = te->next;
            else
                prev->next = te->next;  // node1 -> node2 -> node3
                                        // node1 -> node3
                                        // node1 就是prev上一个遍历的指针
            if (te->finalizerProc)  // 删除前调用回调函数
                te->finalizerProc(eventLoop, te->clientData);
            zfree(te); // 释放内存空间
            return AE_OK;
        }
        prev = te; // prev 指向便利的前一个
        te = te->next; // 下一个
    }
    return AE_ERR; /* NO event with the specified ID found */
}

/* Search the first timer to fire.
 * This operation is useful to know how many time the select can be
 * put in sleep without to delay any event.
 * If there are no timers NULL is returned.
 *
 * Note that's O(N) since time events are unsorted.
 * Possible optimizations (not needed by Redis so far, but...):
 * 1) Insert the event in order, so that the nearest is just the head.
 *    Much better but still insertion or deletion of timers is O(N).
 * 2) Use a skiplist to have this operation as O(1) and insertion as O(log(N)).
 */
static aeTimeEvent *aeSearchNearestTimer(aeEventLoop *eventLoop)
{
    aeTimeEvent *te = eventLoop->timeEventHead; // 得到链表头
    aeTimeEvent *nearest = NULL;

    // head -> node1 -> node2 -> node3
    // nearest 找最近的节点
    // 比较时间 ： 先比较秒 ，秒数相同再比较毫秒
    while(te) {
        if (!nearest || te->when_sec < nearest->when_sec ||
                (te->when_sec == nearest->when_sec &&
                 te->when_ms < nearest->when_ms))
            nearest = te;
        te = te->next;
    }
    return nearest;
}

/* Process time events */
// 处理时间事件
static int processTimeEvents(aeEventLoop *eventLoop) { 
    int processed = 0;
    aeTimeEvent *te;
    long long maxId;

    te = eventLoop->timeEventHead; // 头
    maxId = eventLoop->timeEventNextId-1; // 最大的
    while(te) {
        long now_sec, now_ms;
        long long id;

        if (te->id > maxId) { // 跳过id大于timeEventNextId
            te = te->next;
            continue;
        }
        // 获取当前时间
        aeGetTime(&now_sec, &now_ms);
        if (now_sec > te->when_sec || // 超时
            (now_sec == te->when_sec && now_ms >= te->when_ms)) // 超时
        {
            int retval;

            id = te->id;
            retval = te->timeProc(eventLoop, id, te->clientData);  // 超时处理回调
            processed++;
            /* After an event is processed our time event list may
             * no longer be the same, so we restart from head.
             * Still we make sure to don't process events registered
             * by event handlers itself in order to don't loop forever.
             * To do so we saved the max ID we want to handle.
             *
             * FUTURE OPTIMIZATIONS:
             * Note that this is NOT great algorithmically. Redis uses
             * a single time event so it's not a problem but the right
             * way to do this is to add the new elements on head, and
             * to flag deleted elements in a special way for later
             * deletion (putting references to the nodes to delete into
             * another linked list). */
            if (retval != AE_NOMORE) { 
                // 更新该事件的下一次超时时间  1000ms 12:00:00 12:00:01
                aeAddMillisecondsToNow(retval,&te->when_sec,&te->when_ms);
            } else {
                aeDeleteTimeEvent(eventLoop, id); // -1 删除
            }
            te = eventLoop->timeEventHead; // 会重头开始扫描。。。。
        } else {
            te = te->next;
        }
    }
    return processed;
}

/* Process every pending time event, then every pending file event
 * (that may be registered by time event callbacks just processed).
 * Without special flags the function sleeps until some file event
 * fires, or when the next time event occurrs (if any).
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has AE_ALL_EVENTS set, all the kind of events are processed.
 * if flags has AE_FILE_EVENTS set, file events are processed.
 * if flags has AE_TIME_EVENTS set, time events are processed.
 * if flags has AE_DONT_WAIT set the function returns ASAP until all
 * the events that's possible to process without to wait are processed.
 *
 * The function returns the number of events processed. */
int aeProcessEvents(aeEventLoop *eventLoop, int flags)
{
    int processed = 0, numevents;

    /* Nothing to do? return ASAP */
    if (!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS)) return 0;

    /* Note that we want call select() even if there are no
     * file events to process as long as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */
    if (eventLoop->maxfd != -1 ||
        ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
        int j;
        aeTimeEvent *shortest = NULL;
        struct timeval tv, *tvp;

        if (flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT))
            shortest = aeSearchNearestTimer(eventLoop);
        if (shortest) {
            long now_sec, now_ms;

            /* Calculate the time missing for the nearest
             * timer to fire. */
            aeGetTime(&now_sec, &now_ms);
            tvp = &tv;
            tvp->tv_sec = shortest->when_sec - now_sec;
            if (shortest->when_ms < now_ms) {
                tvp->tv_usec = ((shortest->when_ms+1000) - now_ms)*1000;
                tvp->tv_sec --;
            } else {
                tvp->tv_usec = (shortest->when_ms - now_ms)*1000;
            }
            if (tvp->tv_sec < 0) tvp->tv_sec = 0;
            if (tvp->tv_usec < 0) tvp->tv_usec = 0;
        } else {
            /* If we have to check for events but need to return
             * ASAP because of AE_DONT_WAIT we need to se the timeout
             * to zero */
            if (flags & AE_DONT_WAIT) {
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;
            } else {
                /* Otherwise we can block */
                tvp = NULL; /* wait forever */
            }
        }

        numevents = aeApiPoll(eventLoop, tvp);
        for (j = 0; j < numevents; j++) {
            aeFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];
            int mask = eventLoop->fired[j].mask;
            int fd = eventLoop->fired[j].fd;
            int rfired = 0;

	    /* note the fe->mask & mask & ... code: maybe an already processed
             * event removed an element that fired and we still didn't
             * processed, so we check if the event is still valid. */
            if (fe->mask & mask & AE_READABLE) {
                rfired = 1;
                fe->rfileProc(eventLoop,fd,fe->clientData,mask);
            }
            if (fe->mask & mask & AE_WRITABLE) {
                if (!rfired || fe->wfileProc != fe->rfileProc)
                    fe->wfileProc(eventLoop,fd,fe->clientData,mask);
            }
            processed++;
        }
    }
    /* Check time events */
    if (flags & AE_TIME_EVENTS)
        processed += processTimeEvents(eventLoop);

    return processed; /* return the number of processed file/time events */
}

/* Wait for millseconds until the given file descriptor becomes
 * writable/readable/exception */
int aeWait(int fd, int mask, long long milliseconds) {
    struct timeval tv;
    fd_set rfds, wfds, efds;
    int retmask = 0, retval;

    tv.tv_sec = milliseconds/1000;
    tv.tv_usec = (milliseconds%1000)*1000;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    if (mask & AE_READABLE) FD_SET(fd,&rfds);
    if (mask & AE_WRITABLE) FD_SET(fd,&wfds);
    if ((retval = select(fd+1, &rfds, &wfds, &efds, &tv)) > 0) {
        if (FD_ISSET(fd,&rfds)) retmask |= AE_READABLE;
        if (FD_ISSET(fd,&wfds)) retmask |= AE_WRITABLE;
        return retmask;
    } else {
        return retval;
    }
}

void aeMain(aeEventLoop *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        if (eventLoop->beforesleep != NULL)
            eventLoop->beforesleep(eventLoop);
        aeProcessEvents(eventLoop, AE_ALL_EVENTS);
    }
}

char *aeGetApiName(void) {
    return aeApiName();
}

void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep) {
    eventLoop->beforesleep = beforesleep;
}
