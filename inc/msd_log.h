/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_log.h
 * 
 * Description :  Msd_log, a generic log implementation.
 *                �����汾����־�����̰汾���̰߳汾��
 *                ��mossad���ö����ʱ���ý��̰汾�����ö��߳�ʱ�����̰߳汾
 *                ����ӿڶ���ͳһ�ģ�ֻ��Ҫ��msd_core.h�ж�����Ҫ���͵ĺ�
 *                #define MSD_LOG_MODE_THREAD(Ĭ��)
 *                #define MSD_LOG_MODE_PROCESS
 *
 *     Created :  Apr 6, 2014 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/

#ifndef _MSD_H_LOG_INCLUDED_
#define _MSD_H_LOG_INCLUDED_
 
/*
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
       
#define MSD_PTHREAD_LOCK_MODE
#define MSD_LOG_MODE_THREAD
#include "msd_lock.h"
*/

#define MSD_OK       0
#define MSD_ERR     -1
#define MSD_FAILED  -2
#define MSD_NONEED  -3

/* log level */
#define MSD_LOG_LEVEL_FATAL     0    
#define MSD_LOG_LEVEL_ERROR     1    
#define MSD_LOG_LEVEL_WARNING   2    
#define MSD_LOG_LEVEL_INFO      3    
#define MSD_LOG_LEVEL_DEBUG     4    
#define MSD_LOG_LEVEL_ALL       MSD_LOG_LEVEL_DEBUG

/* log size and num */
#define MSD_LOG_FILE_SIZE       (1<<30) /* 2G */
#define MSD_LOG_MULTIMODE_NO    0
#define MSD_LOG_MULTIMODE_YES   1
#define MSD_LOG_FILE_NUM        20

/* screen */
#define MSD_SCREEN_COLS         80
#define MSD_CONTENT_COLS        65

/* length */
#define MSD_LOG_BUFFER_SIZE     4096
#define MSD_LOG_MAX_NUMBER      100
#define MSD_LOG_PATH_MAX        1024

typedef struct s_msd_log
{
    int  fd;
    char path[MSD_LOG_PATH_MAX];
}s_msd_log_t;

typedef struct msd_log
{
    //char      *msd_log_buffer = MAP_FAILED; /* -1 */
    int          msd_log_has_init;           /* �Ƿ��Ѿ���ʼ�� */
    int          msd_log_level;              /* ��־��߼�¼�ĵȼ� */
    int          msd_log_size;               /* ÿ����־�ļ��Ĵ�С */
    int          msd_log_num;                /* ��־�ĸ��� */
    int          msd_log_multi;              /* �Ƿ񽫲�ͬ�ȼ�����־��д�벻ͬ���ļ� */
    msd_lock_t  *msd_log_rotate_lock;        /* ��־roate�� */
    s_msd_log_t  g_msd_log_files[MSD_LOG_LEVEL_DEBUG +1]; /* ������־�ļ���� */
}msd_log_t;

/*
 * isatty - test whether a file descriptor refers to a terminal, returns 1 if 
 * fd is an open file descriptor referring to a terminal; otherwise  0 is returned,
 * and errno is set to indicate the error.
 * ���STDOUT_FILENO�������նˣ�������ɫ��ʽɾ��ok/failed����������ͨ��ʽ���
 */
/* print [OK] in green */
#define MSD_OK_STATUS \
    (isatty(STDOUT_FILENO)? \
     ("\033[1m[\033[32m OK \033[37m]\033[m") : ("[ OK ]"))
     
/* print [FAILED] in red */
#define MSD_FAILED_STATUS \
    (isatty(STDOUT_FILENO)? \
     ("\033[1m[\033[31m FAILED \033[37m]\033[m") :("[ FAILED ]"))

/*
 * C�ĺ���:
 * #�Ĺ����ǽ������ĺ���������ַ�����������Stringfication����
 * ��˵�����ڶ��������õĺ� ����ͨ���滻���������Ҹ�����һ��˫���š�
 *
 * ##����Ϊ���ӷ���concatenator��������������Token����Ϊһ��Token�h��
 * ���߳䵱�����þ��ǵ����Ϊ�յ�ʱ������ǰ����Ǹ�����,��Variadic Macro��Ҳ���Ǳ�κ�
 * ���磺
 * #define myprintf(templt,...) fprintf(stderr,templt,__VA_ARGS__) ���� 
 * #define myprintf(templt,args...) fprintf(stderr,templt,args)
 * ��һ����������û�жԱ��������������Ĭ�ϵĺ�__VA_ARGS__����������ڶ������У�
 * ������ʽ���������Ϊargs����ô�����ں궨���оͿ�����args����ָ����ˡ� 
 * ͬC���Ե�stdcallһ������α�����Ϊ�����������һ����֡�
 *
 * �����Ϊ�գ��ᵼ�±�����󣬴�ʱ
 * #define myprintf(templt, ...) fprintf(stderr,templt, ##__VAR_ARGS__)
 * ��ʱ��##������ӷ��ų䵱�����þ��ǵ�__VAR_ARGS__Ϊ�յ�ʱ������ǰ����Ǹ����š���ô��ʱ�ķ���������£�
 * myprintf(templt); ==> myprintf(stderr,templt);
 */
#define MSD_BOOT_SUCCESS(fmt, args...) msd_boot_notify(0, fmt, ##args)
#define MSD_BOOT_FAILED(fmt, args...) do {\
    msd_boot_notify(-1, fmt, ##args); \
    exit(1);\
}while(0)

#define MSD_DETAIL(level, fmt, args...) \
    msd_log_write(level, "[%s:%d:%s]" fmt, __FILE__, __LINE__, __FUNCTION__, ##args) /* whether add #?? */

#define MSD_FATAL_LOG(fmt, args...)   MSD_DETAIL(MSD_LOG_LEVEL_FATAL, fmt, ##args)
#define MSD_ERROR_LOG(fmt, args...)   MSD_DETAIL(MSD_LOG_LEVEL_ERROR, fmt, ##args)
#define MSD_WARNING_LOG(fmt, args...) MSD_DETAIL(MSD_LOG_LEVEL_WARNING, fmt, ##args)
#define MSD_INFO_LOG(fmt, args...)    MSD_DETAIL(MSD_LOG_LEVEL_INFO, fmt, ##args)
#define MSD_DEBUG_LOG(fmt, args...)   MSD_DETAIL(MSD_LOG_LEVEL_DEBUG, fmt, ##args)

void msd_boot_notify(int ok, const char *fmt, ...);
int msd_log_init(const char *dir, const char *filename, int level, int size, int lognum, int multi);
int msd_log_write(int level, const char *fmt, ...);
void msd_log_close();

/* static int msd_get_map_mem(); */
/* static int msd_log_rotate(int fd, const char *path); */
#endif /*_MSD_H_LOG_INCLUDED_*/

