/*
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_signal.h 
 * 
 * Description :  Msd_signal, Mossad�źŴ�����ص�����.
 *                �źŴ����Ϊ��������:
 *                1.������Ϊ�����߼���Ҫ���������źţ���ר�ŵ��źŴ����߳�ͬ�����д���
 *                2.�Իᵼ�³���������ֹ���ź���SIGSEGV/SIGBUS�ȣ��ɸ����߳��Լ�����
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/
#ifndef __MSD_SIGNAL_H_INCLUDED__
#define __MSD_SIGNAL_H_INCLUDED__

/*
#include <execinfo.h>
*/

typedef struct 
{
    int         signo;                 /* �ź���ֵ */
    char        *signame;              /* �źź��� */
} msd_signal_t;

typedef struct thread_signal{
    pthread_t          tid;             /* �߳�id */
    int                notify_read_fd;  /* ��master�߳�ͨ�Źܵ���ȡ�� */
    int                notify_write_fd; /* ��master�߳�ͨ�Źܵ�д��� */
}msd_thread_signal_t;

int msd_init_public_signals();
int msd_init_private_signals();

#endif /* __MSD_SIGNAL_H_INCLUDED__ */

