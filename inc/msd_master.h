/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_master.h
 * 
 * Description :  Msd_master. Mossad master thread work.
 * 
 *     Created :  Apr 22, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/

#ifndef __MSD_MASTER_H_INCLUDED__ 
#define __MSD_MASTER_H_INCLUDED__


#define MSD_MSG_MAGIC      0x567890EF
#define MSD_MAGIC_DEBUG    0x1234ABCD

typedef enum msd_master_stat{
    M_RUNNING,
    M_STOPING,
    M_STOP
}msd_master_stat_t;

typedef enum msd_client_stat{
    C_COMMING,                  /* ���Ӹ����� */
    C_DISPATCHING,              /* �������ڷ����� */
    C_WAITING,                  /* �����Ѿ����䵽�˾���woker�У��ȴ�request���� */
    C_RECEIVING,                /* request����������δ����һ��������Э����������ȴ� */
    C_PROCESSING,               /* request�����㹻��������ʼ���� */
    C_CLOSING                   /* ���ӹر��� */
}msd_client_stat_t;


typedef struct msd_master
{
    int                 listen_fd;
    int                 client_limit;   /* client�ڵ�������� */
    int                 poll_interval;  /* Master�߳�CronƵ�� */
    msd_master_stat_t   status;         /* Master�߳�״̬ */
    
    int                 cur_conn;       /* ��һ������client��client_vec��λ�� */
    int                 cur_thread;     /* ��һ������clientʱ������̺߳� */
    int                 total_clients;  /* ��ǰclient���ܸ��� */
    unsigned int        start_time;
    msd_vector_t        *client_vec;    /* msd_conn_client���͵�vector���µ����ӣ����ҵ��Լ��ĺ���λ�� */
    msd_ae_event_loop   *m_ael;         /* ae���������listen_fd��signal_notify�ļ��� */
}msd_master_t;

typedef struct msd_conn_client
{
    int     magic;         /* һ����ʼ����ʱ�����ϵ�ħ������������ʶcliָ���Ƿ����쳣 */
    int     fd;            /* client���ӹ���֮��conn������ɵ�fd (��������!) */    
    int     close_conn;    /* whether close connection after send response */
    char    *remote_ip;    /* client ip */
    int     remote_port;   /* client port */
    int     recv_prot_len; /* ����˺Ϳͻ���ͨ��Э��涨�õĶ�ȡ���� */
    msd_str_t  *sendbuf;      /* ���ͻ��� */
    msd_str_t  *recvbuf;      /* ���ջ��� */
    time_t  access_time;   /* client���һ������ʱ�䣬����:�������ӡ���ȡ��д�� */
    msd_client_stat_t status; /* client��״̬ */


    int     idx;           /* λ��client_vec�е�λ�� */
    int     worker_id;     /* ���ĸ�woker�̸߳���������ӵ��������� */
}msd_conn_client_t;

int msd_master_cycle();
void msd_close_client(int client_idx, const char *info);
int msd_master_destroy(msd_master_t *master);

#endif

