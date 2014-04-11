/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_ae.h
 * 
 * Description :  A simple event-driven programming library. It's from Redis.
 *                The default multiplexing layer is select. If the system 
 *                support epoll, you can define the epoll macro.
 *
 *                #define MSD_EPOLL_MODE
 *
 *     Created :  Apr 7, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/
#ifndef __MSD_AE_H_INCLUDED__
#define __MSD_AE_H_INCLUDED__

/*
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
       
#define MSD_OK 0
#define MSD_ERR -1

#define MSD_EPOLL_MODE
//#define MSD_SELECT_MODE
*/

#ifdef MSD_EPOLL_MODE
#define MSD_AE_SETSIZE     (1024 * 10)     /* Max number of fd supported */
#else
#define MSD_AE_SETSIZE     1024            /* select ������1024��fd */
#endif

#define MSD_AE_NONE         0
#define MSD_AE_READABLE     1
#define MSD_AE_WRITABLE     2

#define MSD_AE_FILE_EVENTS  1
#define MSD_AE_TIME_EVENTS  2
#define MSD_AE_ALL_EVENTS   (MSD_AE_FILE_EVENTS | MSD_AE_TIME_EVENTS)
#define MSD_AE_DONT_WAIT    4

#define MSD_AE_NOMORE       -1

/* Macros����ʹ�ã�ɶҲ���� */
#define MSD_AE_NOTUSED(V)   ((void)V)

struct msd_ae_event_loop;

/* Type and data structures */
typedef void msd_ae_file_proc(struct msd_ae_event_loop *el, int fd, 
        void *client_data, int mask);
typedef int msd_ae_time_proc(struct msd_ae_event_loop *el, long long id,
        void *client_data);
typedef void msd_ae_event_finalizer_proc(struct msd_ae_event_loop *el,
        void *client_data);
typedef void msd_ae_before_sleep_proc(struct msd_ae_event_loop *el);

/* File event structure */
typedef struct msd_ae_file_event {
    int                  mask; /* one of AE_(READABLE|WRITABLE) */
    msd_ae_file_proc    *r_file_proc;
    msd_ae_file_proc    *w_file_proc;
    void *client_data;
} msd_ae_file_event;

/* Time event structure */
typedef struct msd_ae_time_event {
    long long id;                                /* ʱ���¼�ע��id */
    long when_sec;                               /* �¼������ľ���ʱ������� */
    long when_ms;                                /* �¼������ľ���ʱ��ĺ����� */
    msd_ae_time_proc    *time_proc;              /* ��ʱ�ص����� */
    msd_ae_event_finalizer_proc *finalizer_proc; /* ʱ���¼��������ص� */
    void *client_data;                           /* ��ʱ�ص�����/���������Ĳ��� */
    struct msd_ae_time_event *next;
} msd_ae_time_event;

/* A fired event */
typedef struct msd_ae_fired_event {
    int fd;
    int mask;
} msd_ae_fired_event;

#ifdef MSD_EPOLL_MODE

typedef struct msd_ae_api_state 
{
    int epfd;
    struct epoll_event events[MSD_AE_SETSIZE];
} msd_ae_api_state;
#else

/* select���fdֻ֧��1024! */
typedef struct msd_ae_api_state 
{
    fd_set rfds, wfds;
    /* We need to have a copy of the fd sets as it's not safe to
       reuse FD sets after select(). */
    fd_set _rfds, _wfds;
} msd_ae_api_state;
#endif

/* State of an event base program */
typedef struct msd_ae_event_loop {
    int maxfd;                                  /* highest file descriptor currently registered */
    long long           time_event_next_id;     /* �����µ�ʱ���¼�ע��ʱ��Ӧ�ø�������ע��id */
    time_t              last_time;              /* Used to detect system clock skew */
    msd_ae_file_event   events[MSD_AE_SETSIZE]; /* �ļ��¼����飬fd��Ϊ����������api_data�Ƕ�Ӧ�� */
    msd_ae_fired_event  fired[MSD_AE_SETSIZE];  /* Fired events�����������fd��Ϊ�����ģ���0��ʼ */
    msd_ae_time_event   *time_event_head;
    int stop;
    msd_ae_api_state    *api_data;              /* fdΪ��������eventsһһ��Ӧ������selec/epoll������ѯ */
    msd_ae_before_sleep_proc    *before_sleep;
} msd_ae_event_loop;

/* Prototypes */
msd_ae_event_loop   *msd_ae_create_event_loop(void);
void msd_ae_free_event_loop(msd_ae_event_loop *el);
void msd_ae_stop(msd_ae_event_loop *el);
int msd_ae_create_file_event(msd_ae_event_loop *el, int fd, int mask,
        msd_ae_file_proc *proc, void *client_data);
void msd_ae_delete_file_event(msd_ae_event_loop *el, int fd, int mask);
int msd_ae_get_file_event(msd_ae_event_loop *el, int fd);
long long msd_ae_create_time_event(msd_ae_event_loop *el, long long milliseconds,
        msd_ae_time_proc *proc, void *client_data,
        msd_ae_event_finalizer_proc *finalizer_proc);
int msd_ae_delete_time_event(msd_ae_event_loop *el, long long id);
int msd_ae_process_events(msd_ae_event_loop *el, int flags);
int msd_ae_wait(int fd, int mask, long long milliseconds);
void msd_ae_main(msd_ae_event_loop *el);
char *msd_ae_get_api_name(void);
void msd_ae_set_before_sleep_proc(msd_ae_event_loop *el, 
        msd_ae_before_sleep_proc *before_sleep);

#endif /*__MSD_AE_H_INCLUDED__ */
