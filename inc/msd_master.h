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

       
/*
    char is_server;
    int listen_fd;
    struct sockaddr_in listen_addr;
    in_addr_t srv_addr;
    int srv_port;
    unsigned int start_time;
    int nonblock;
    int listen_queue_length;
    int tcp_send_buffer_size;
    int tcp_recv_buffer_size;
    int send_timeout;
    int tcp_reuse;
    int tcp_nodelay;
    struct event ev_accept;
    //void *(* on_data_callback)(void *);
    ProcessHandler_t on_data_callback;
    
    
    
    
    qbh_ae_event_loop       *ael;
    static int              listen_fd;
    static char             sock_error[QBH_ANET_ERR_LEN];
    static qbh_dlist        *clients; 
    static int              client_limit;   // client�ڵ�������� 
    static int              client_timeout; // client��ʱʱ�� 
    static time_t           unix_clock;
    static pid_t            conn_pid;       // conn���̵�id 
    static qbh_vector_t     *conn_vec;    // vector�ṹ��Ԫ����qbh_client_connָ�룬������clientʧЧ�жϵ�
                                           //����������client���ӹ�����ʱ��conn���õĶ�Ӧ��fd 
*/                                                

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



    int     idx;           /* λ��client_vec�е�λ�� */
    int     worker_id;     /* ���ĸ�woker�̸߳���������ӵ��������� */

    /*
    int id;
    int worker_id;
    int client_fd;
    long task_id;
    long sub_task_id;
    in_addr_t client_addr;
    int client_port;
    time_t conn_time;
    func_t handle_client;
    struct event event;
    std::vector<struct event *> ev_dns_vec;
    enum conn_states state;
    enum error_no err_no;
    enum conn_type type;
    int ev_flags;
 
    void * userData;

    int r_buf_arena_id;
    char *read_buffer;
    unsigned int rbuf_size;
    unsigned int need_read;
    unsigned int have_read;

    int w_buf_arena_id;
    char *__write_buffer;
    char *write_buffer;
    unsigned int wbuf_size;
    unsigned int __need_write;
    unsigned int need_write;
    unsigned int have_write;
    char client_hostname[MAX_HOST_NAME + 1];
    */
}msd_conn_client_t;

int msd_master_cycle();
void msd_close_client(int client_idx, const char *info);
int msd_master_destroy(msd_master_t *master);

#endif

