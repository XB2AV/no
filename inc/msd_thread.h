/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_thread.h 
 * 
 * Description :  Msd_thread, �����߳����ʵ��.
 * 
 *     Created :  May 20, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/
#ifndef __MSD_THREAD_H_INCLUDED__
#define __MSD_THREAD_H_INCLUDED__

/*
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
       
#define MSD_PTHREAD_LOCK_MODE
//#define MSD_SYSVSEM_LOCK_MODE
//#define MSD_FCNTL_LOCK_MODE

#define MSD_LOG_MODE_THREAD

#include "msd_lock.h"
#include "msd_log.h"

#define MSD_OK   0
#define MSD_ERR -1
*/

#define THREAD_STACK_SIZE   10*1024*1024     /* 10M */

typedef struct thread_pool   msd_thread_pool_t;
typedef struct thread_worker msd_thread_worker_t;

typedef enum msd_worker_stat{
    W_RUNNING,
    W_STOPING,
    W_STOP
}msd_worker_stat_t;

struct thread_worker{
    int                idx;             /* λ��woker�б��е�λ������ */
    pthread_t          tid;             /* �߳�id */
    int                notify_read_fd;  /* master��woker�߳�ͨ�Źܵ���ȡ�� */
    int                notify_write_fd; /* master��woker�߳�ͨ�Źܵ�д��� */  
    msd_worker_stat_t  status;          /* worker��״̬ */

    msd_ae_event_loop  *t_ael;          /* worker�߳�ae��������ڼ����ܵ����������client fd */
    msd_thread_pool_t  *pool;           /* �����ڵ��̳߳ؾ�� */
    msd_dlist_t        *client_list;    /* ���̸߳����client����ɵĶ��� */
    
    //struct event_base *ev_base;
    //struct event ev_notify;
    //struct event ev_cleantimeout;

    //std::set<int> conn_set;
    //ares_channel dns_channel;
    //gkoAlloc mem;
    //void * userData;
};

struct thread_pool{
    msd_lock_t            *thread_lock;           /* �����߳���������ƽ�ȵĹ����߳�֮�䣬��Ҫͬ����ʱ���ô��� */
    msd_thread_worker_t   **thread_worker_array;  /* worker�߳��б� */
    
    int                   thread_worker_num;      /* thread_worker_array���鳤�� */
    int                   thread_stack_size;      /* �߳�ջ��С */
    
    int                   client_timeout;         /* client��ʱʱ�� */
    int                   poll_interval;          /* worker�߳�CronƵ�� */
    //pthread_cond_t      cond;
    //pthread_cond_t      exit_cond;
    //dqueue_t            task_queue;
    //int                 exit;
    //int                 threads_idle;
    //
    //int                 threads_max;
};
 
msd_thread_pool_t *msd_thread_pool_create(int worker_num, int stack_size , void* (*worker_task)(void *arg));
int msd_thread_pool_destroy(msd_thread_pool_t *pool);
void* msd_thread_worker_cycle(void* arg);
void msd_thread_sleep(int s);
void msd_thread_usleep(int s);

#endif /* __MSD_THREAD_H_INCLUDED__ */

