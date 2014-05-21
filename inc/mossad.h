/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *     Filename:  Msd_core.h 
 * 
 *  Description:  The massod's public header file. 
 * 
 *      Created:  Mar 18, 2012 
 *      Version:  0.0.1 
 * 
 *       Author:  HQ 
 *      Company:  Qihoo 
 *
 **/

#ifndef __MSD_CORE_H_INCLUDED__ 
#define __MSD_CORE_H_INCLUDED__

/* --------SYSTEM HEADER FILE---------- */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <strings.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <getopt.h>
#include <dlfcn.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/resource.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/cdefs.h>
#include <execinfo.h>

 
/* -------------------CONFIG------------------- */
#define MSD_PTHREAD_LOCK_MODE       /* Lock mode */
//#define MSD_SYSVSEM_LOCK_MODE
//#define MSD_FCNTL_LOCK_MODE

#define MSD_LOG_MODE_THREAD         /* Log mode */
//#define MSD_LOG_MODE_PROCESS

#define MSD_EPOLL_MODE              /* Multiplexing mode */
//#define MSD_SELECT_MODE
 

/* ---------------PROJECT HEADER FILE----------- */
#include "msd_lock.h"
#include "msd_log.h"
#include "msd_string.h"
#include "msd_hash.h"
#include "msd_conf.h"
#include "msd_vector.h"
#include "msd_dlist.h"
#include "msd_ae.h"
#include "msd_so.h"
#include "msd_daemon.h"
#include "msd_anet.h"
#include "msd_thread.h"
#include "msd_master.h"
#include "msd_plugin.h"
#include "msd_signal.h"
 
/* -----------------PUBLIC MACRO---------------- */
#define MSD_OK       0
#define MSD_ERR     -1
#define MSD_FAILED  -2
#define MSD_NONEED  -3
#define MSD_END     -4


#define MSD_PROG_TITLE   "Mossad"
#define MSD_PROG_NAME    "mossad"
#define MOSSAD_VERSION   "0.0.1"

/* --------Structure--------- */

/* ������ȫ��so�ĺ�����������so�������û���д�� */
typedef struct msd_so_func_struct 
{
    int (*handle_init)(void *);                                       /* �����̳�ʼ����ʱ�������ã�������ȫ�ֱ���g_conf���˺�����ѡ */
    int (*handle_fini)(void *);                                      /* �������˳���ʱ�������ã�������ȫ�ֱ���g_conf���˺�����ѡ */
    /*int (*handle_open)(char **, int *, char *, int);*/              /* �������������ˣ�Accept֮����ÿ������һЩ��ӭ��Ϣ֮�࣬�˺����ǿ�ѡ�ģ�һ�㲻��д��
                                                                       * ��һ��������������������ڶ���������������ã�������clientip, ���ĸ�client port 
                                                                       */   
    int (*handle_open)(msd_conn_client_t *);
    int (*handle_close)(msd_conn_client_t *, const char *);          /* ���ر���ĳ��client�����ӵ�ʱ����ã���һ������client ip, �ڶ���client port���˺�����ѡ */                                             
    int (*handle_prot_len)(msd_conn_client_t *);                      /* ������ȡclient����������Ϣ�ľ��峤�ȣ����õ�Э�鳤��
                                                                       * mossad��ȡ���˳���֮��ӽ��ջ������ж�ȡ��Ӧ���ȵ��������ݣ�����handle_process������
                                                                       * ��һ�������ǽ��ջ��������ڶ������ջ��������ȣ�������client ip, ���ĸ�client port 
                                                                       */
    int (*handle_process)(msd_conn_client_t *);                        /* Worker����ר�ã���������client�����룬������������³�����
                                                                       * ����:��һ���ǽ��ջ��������ڶ����ǽ������ݳ���
                                                                       *      �������Ƿ��ͻ����������ĸ��Ƿ������ݵĳ�������
                                                                       *      ���������client ip, ������client port
                                                                       */
} msd_so_func_t;

/* mossadʵ���ṹ */
typedef struct _msd_instance_t{
    msd_str_t           *pid_file;
    msd_str_t           *conf_path;
    msd_conf_t          *conf;
    //msd_log_t           *log;

    msd_master_t        *master;                 /* master�߳̾�� */ 
    msd_thread_pool_t   *pool;                   /* worker�̳߳ؾ�� */ 
    msd_thread_signal_t *sig_worker;             /* signal worker�߳̾�� */ 

    msd_str_t           *so_file;                /* so�ļ�·�� */
    msd_so_func_t       *so_func;                /* װ����ȫ��so�ĺ��������� */
    void                *so_handle;              /* so���ָ�� */

    msd_lock_t          *thread_woker_list_lock; /* woker list �� */
    msd_lock_t          *client_conn_vec_lock;   /* client vector �� */
}msd_instance_t;


#endif
