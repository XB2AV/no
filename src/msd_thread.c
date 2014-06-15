/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_thread.c
 * 
 * Description :  Msd_thread, �����߳����ʵ��.
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/
#include "msd_core.h"

#define MSD_IOBUF_SIZE    4096
#define MSD_MAX_PROT_LEN  10485760 /* ��������10M */


extern msd_instance_t *g_ins;

static void msd_thread_worker_process_notify(struct msd_ae_event_loop *el, int fd, 
        void *client_data, int mask);
static int msd_thread_worker_create(msd_thread_pool_t *pool, void* (*worker_task)(void *arg), 
        int idx);
static void msd_read_from_client(msd_ae_event_loop *el, int fd, void *privdata, int mask);
static int msd_thread_worker_cron(msd_ae_event_loop *el, long long id, void *privdate);
static void msd_thread_worker_shut_down(msd_thread_worker_t *worker);

/**
 * ����:�����̳߳�
 * ����:@num: �����̸߳��� 
 *      @statc_size: �����߳�ջ��С 
 * ����:
 *      1. 
 * ����:�ɹ�:�̳߳ص�ַ;ʧ��:NULL
 **/
msd_thread_pool_t *msd_thread_pool_create(int worker_num, int stack_size , void* (*worker_task)(void*)) 
{
    msd_thread_pool_t *pool;
    int i;
    assert(worker_num > 0 && stack_size >= 0);

    /* Allocate memory and zero all them. */
    pool = (msd_thread_pool_t *)calloc(1, sizeof(*pool));
    if (!pool) {
        return NULL;
    }

    /* ��ʼ���߳��� */
    if (MSD_LOCK_INIT(pool->thread_lock) != 0) 
    {
        free(pool);
        return NULL;
    }

    /* ��ʼ���߳�worker�б� */
    if(!(pool->thread_worker_array = 
        (msd_thread_worker_t **)calloc(worker_num, sizeof(msd_thread_worker_t))))
    {
        MSD_LOCK_DESTROY(pool->thread_lock);
        free(pool);
        return NULL;
    }
    
    pool->thread_worker_num = worker_num;
    pool->thread_stack_size = stack_size;    

    /* ��ʼ��client��ʱʱ�䣬Ĭ��5���� */
    pool->client_timeout =  msd_conf_get_int_value(g_ins->conf, "client_timeout", 300);

    /* ��ʼ��cronƵ�� */
    pool->poll_interval  =  msd_conf_get_int_value(g_ins->conf, "worker_poll_interval", 60);
    
    /* ��ʼ������woker�߳� */
    for (i = 0; i < worker_num; ++i) 
    {
        if(msd_thread_worker_create(pool, worker_task, i) != MSD_OK)
        {
            MSD_ERROR_LOG("Msd_thread_worker_create failed, idx:%d", i);
            return NULL;
        }
    }

    return pool;
}

/**
 * ����:���������߳�
 * ����:@pool: �̳߳ؾ�� 
 *      @worker: ��������
 *      @idx: �߳�λ�������� 
 * ����:
 *      1. ���ȴ���woker�̵߳���Դ������ʼ��
 *      2. ���߳̾���������̵߳������еĶ�Ӧλ��
 *      3. ����woker�̣߳���ʼ�ɻ��������
 * ����:�ɹ�:0; ʧ��:-x
 **/
static int msd_thread_worker_create(msd_thread_pool_t *pool, void* (*worker_task)(void *), int idx)
{
    msd_thread_worker_t *worker  = NULL;
    char error_buf[256];
    if(!(worker = calloc(1, sizeof(msd_thread_worker_t))))
    {
        MSD_ERROR_LOG("Thread_worker_creat failed, idx:%d", idx);
        return MSD_ERR;
    }
    
    worker->pool                 = pool;
    worker->idx                  = idx;
    worker->status               = W_STOP;
    worker->priv_data            = NULL;
    pool->thread_worker_array[idx] = worker; /* �����Ӧ��λ�� */

    /* 
     * ����ͨ�Źܵ�
     * master�߳̽���Ҫ�����fdд��ܵ���woker�̴߳ӹܵ�ȡ��
     **/
    int fds[2];
    if (pipe(fds) != 0)
    {
        free(worker);
        MSD_ERROR_LOG("Thread_worker_creat pipe failed, idx:%d", idx);
        return MSD_ERR;
    }
    worker->notify_read_fd  = fds[0];
    worker->notify_write_fd = fds[1];

    /* �ܵ�������������! */
    if((MSD_OK != msd_anet_nonblock(error_buf,  worker->notify_read_fd))
        || (MSD_OK != msd_anet_nonblock(error_buf,  worker->notify_write_fd)))
    {
        close(worker->notify_read_fd);
        close(worker->notify_write_fd);
        free(worker);
        MSD_ERROR_LOG("Set pipe nonblock failed, err:%s", error_buf);
        return MSD_ERR;        
    }

    /* ����ae��� */
    if(!(worker->t_ael = msd_ae_create_event_loop()))
    {
        close(worker->notify_read_fd);
        close(worker->notify_write_fd);
        free(worker);
        MSD_ERROR_LOG("Create ae failed");
        return MSD_ERR;  
    }

    /* ��ʼ��client���� */
    if(!(worker->client_list = msd_dlist_init()))
    {
        close(worker->notify_read_fd);
        close(worker->notify_write_fd);
        msd_ae_free_event_loop(worker->t_ael);
        free(worker);
        MSD_ERROR_LOG("Create ae failed");
        return MSD_ERR;         
    }

    /* �����߳� */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, pool->thread_stack_size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if(pthread_create(&worker->tid, &attr, worker_task, worker) != 0)
    {
        close(worker->notify_read_fd);
        close(worker->notify_write_fd);
        free(worker);
        MSD_ERROR_LOG("Thread_worker_creat failed, idx:%d", idx);
        return MSD_ERR;        
    }
    pthread_attr_destroy(&attr);
    return MSD_OK;    
}

/**
 * ����: �����̳߳�
 * ����: @pool: �̳߳ؾ�� 
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_thread_pool_destroy(msd_thread_pool_t *pool) 
{
    assert(pool);
    int i;
    msd_thread_worker_t *worker;
    MSD_LOCK_LOCK(g_ins->thread_woker_list_lock);
    for(i=0; i<pool->thread_worker_num; i++)
    {
        worker = pool->thread_worker_array[i];
        if(worker)
        {
            free(worker);
        }
    }
    MSD_LOCK_UNLOCK(g_ins->thread_woker_list_lock);
    MSD_LOCK_DESTROY(pool->thread_lock);
    free(pool->thread_worker_array);
    free(pool);
    MSD_INFO_LOG("Destory the thread pool!");
    return MSD_OK;
}

/**
 * ����: �߳�˯�ߺ���
 * ����: @sec, ˯��ʱ��������
 * ˵��: 
 *       1.����select����ʵ��˯��Ч��
 **/
void msd_thread_sleep(int s) 
{
    struct timeval timeout;
    timeout.tv_sec  = s;
    timeout.tv_usec = 0;
    select( 0, NULL, NULL, NULL, &timeout);
}

/**
 * ����: �߳�˯�ߺ���
 * ����: @sec, ˯��ʱ��������
 * ˵��: 
 *       1.����select����ʵ��˯��Ч��
 **/
void msd_thread_usleep(int s) 
{
    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = s;
    select( 0, NULL, NULL, NULL, &timeout);
}


/**
 * ����: worker�̹߳�������
 * ����: @arg: �̺߳�����ͨ����˵�������߳���������ָ��
 * ˵��: 
 *       1.msd_ae_free_event_loop��Ӧ����ae�Ļص�������ã��ڻص�����Ӧ�õ���
 *         msd_ae_stop������msd_ae_main_loop�˳���Ȼ��ae����������main_loop��
 * ����:�ɹ�:0; ʧ��:-x
 **/
void* msd_thread_worker_cycle(void* arg) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)arg;
    MSD_INFO_LOG("Worker[%d] begin to work", worker->idx);

    worker->status = W_RUNNING;

    /* worker�̳߳�ʼ������ */
    if (g_ins->so_func->handle_worker_init) 
    {
        if (g_ins->so_func->handle_worker_init(g_ins->conf, worker) != MSD_OK) 
        {
            MSD_ERROR_LOG("Invoke hook handle_worker_init failed");
            MSD_BOOT_FAILED("Invoke hook handle_worker_init failed");
        }
    }

    
    if (msd_ae_create_file_event(worker->t_ael, worker->notify_read_fd, 
                MSD_AE_READABLE, msd_thread_worker_process_notify, arg) == MSD_ERR) 
    {
        MSD_ERROR_LOG("Create file event failed");
        msd_ae_free_event_loop(worker->t_ael);
        return (void *)NULL; 

    }

    /* ������ʱ��������ʱ��client */
    msd_ae_create_time_event(worker->t_ael, 1000*worker->pool->poll_interval,
            msd_thread_worker_cron, worker, NULL);

    /* ����ѭ�� */
    msd_ae_main_loop(worker->t_ael);

    /* �˳���ae_main_loop���̵߳������ߵ���ͷ */
    worker->tid = 0;
    worker->idx = -1;
    msd_dlist_destroy(worker->client_list);
    msd_ae_free_event_loop(worker->t_ael);
    pthread_exit(0);
    return (void*)NULL;
}

/**
 * ����: worker�̹߳�������
 * ����: @arg: �̺߳�����ͨ����˵�������߳���������ָ��
 * ˵��: 
 *       1. ���������client_idx��-1�����ʾ��Ҫ���򼴽��ر�
 *       2. ���������client_idx��-1�������idxȡ��client���
 *          ��client->fd�����ȡ�¼����ϣ����й���
 * ����:�ɹ�:0; ʧ��:-x
 **/
static void msd_thread_worker_process_notify(struct msd_ae_event_loop *el, int notify_fd, 
        void *client_data, int mask)
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)client_data;
    msd_master_t        *master = g_ins->master;
    int                  client_idx;
    msd_conn_client_t   **pclient = NULL;
    msd_conn_client_t    *client = NULL;
    int                  read_len;

    /* ��ȡclient idx */
    if((read_len = read(notify_fd, &client_idx, sizeof(int)))!= sizeof(int))
    {
        MSD_ERROR_LOG("Worker[%d] read the notify_fd error! read_len:%d", worker->idx, read_len);
        return;
    }
    MSD_INFO_LOG("Worker[%d] Get the task. Client_idx[%d]", worker->idx, client_idx);

    /* ���������client_idx��-1�����ʾ��Ҫ���򼴽��ر� */
    if(-1 == client_idx)
    {
        msd_thread_worker_shut_down(worker);
        return;
    }
    
    /* ����client idx��ȡ��client��ַ */
    MSD_LOCK_LOCK(g_ins->client_conn_vec_lock);
    pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, client_idx);
    MSD_LOCK_UNLOCK(g_ins->client_conn_vec_lock);

    if(pclient && *pclient)
    {
        client = *pclient;
    }
    else
    {
        MSD_ERROR_LOG("Worker[%d] get the client error! client_idx:%d", worker->idx, client_idx);
        return;
    }

    /* ��worker_idд��client�ṹ */
    client->worker_id = worker->idx;
    client->status    = C_WAITING;
    
    /* ��worker->client_list׷��client */
    msd_dlist_add_node_tail(worker->client_list, client);
    
    /* ��ӭ��Ϣ */
    if(g_ins->so_func->handle_open)
    {
        if(MSD_OK != g_ins->so_func->handle_open(client))
        {
            msd_close_client(client_idx, NULL);
            MSD_ERROR_LOG("Handle open error! Close connection. IP:%s, Port:%d.", 
                            client->remote_ip, client->remote_port);
            return;
        }
    }
        
    /* ע���ȡ�¼� */
    if (msd_ae_create_file_event(worker->t_ael, client->fd, MSD_AE_READABLE,
                msd_read_from_client, client) == MSD_ERR) 
    {
        msd_close_client(client_idx, "create file event failed");
        MSD_ERROR_LOG("Create read file event failed for connection:%s:%d",
                client->remote_ip, client->remote_port);
        return;
    }  
}

/**
 * ����: client�ɶ�ʱ�Ļص�������
 * ����: @privdata���ص����������
 * ˵��: 
 *       1.  
 * ����:�ɹ�:0; ʧ��:-x
 **/
static void msd_read_from_client(msd_ae_event_loop *el, int fd, void *privdata, int mask) 
{
    msd_conn_client_t *client = (msd_conn_client_t *)privdata;
    int nread;
    char buf[MSD_IOBUF_SIZE];
    //MSD_AE_NOTUSED(el);
    MSD_AE_NOTUSED(mask);
    int ret;
    
    MSD_DEBUG_LOG("Read from client %s:%d", client->remote_ip, client->remote_port);
    
    nread = read(fd, buf, MSD_IOBUF_SIZE - 1);
    client->access_time = time(NULL);
    if (nread == -1) 
    {
        /* ��������fd����ȡ������������������ݿɶ�����errno==EAGAIN */
        if (errno == EAGAIN) 
        {
            MSD_WARNING_LOG("Read connection %s:%d eagain: %s",
                     client->remote_ip, client->remote_port, strerror(errno));
            nread = 0;
            return;
        }
        else if (errno == EINTR) 
        {
            MSD_WARNING_LOG("Read connection %s:%d interrupt: %s",
                     client->remote_ip, client->remote_port, strerror(errno));
            nread = 0;
            return;
        } 
        else 
        {
            MSD_ERROR_LOG("Read connection %s:%d failed: %s",
                     client->remote_ip, client->remote_port, strerror(errno));

            msd_close_client(client->idx, "Read data failed!");
            return;
        }
    } 
    else if (nread == 0) 
    {
        MSD_INFO_LOG("Client close connection %s:%d",client->remote_ip, client->remote_port);
        msd_close_client(client->idx, NULL);
        return;
    }
    buf[nread] = '\0';

    //printf("%d %d  %d  %d\n", buf[nread-3], buf[nread-2], buf[nread-1], buf[nread]);
    MSD_INFO_LOG("Read from client %s:%d. Length:%d, Content:%s", 
                    client->remote_ip, client->remote_port, nread, buf);

    /* ������������ƴ�ӵ�recvbuf���棬������ƴ�ӣ���Ϊ��������İ��ܴ�
     * ��Ҫ���read���ܽ���ȡ����ƴװ��һ������������ */
    msd_str_cat(&(client->recvbuf), buf);

    /* һ�ν��ܵ����ݿ����Ƕ��������Э��� */
    do{
        if (client->recv_prot_len == 0) 
        {
            /* ÿ�ζ�ȡ�ĳ��Ȳ���������recv_prot_len��һ�����������ϵ��� */ 
            client->recv_prot_len = g_ins->so_func->handle_prot_len(client);
            MSD_DEBUG_LOG("Get the recv_prot_len %d", client->recv_prot_len);
        } 

        if (client->recv_prot_len < 0 || client->recv_prot_len > MSD_MAX_PROT_LEN) 
        {
            MSD_ERROR_LOG("Invalid protocol length:%d for connection %s:%d", 
                     client->recv_prot_len, client->remote_ip, client->remote_port);
            msd_close_client(client->idx, "Wrong recv_port_len!");
            return;
        } 

        if (client->recv_prot_len == 0) 
        {
            /* ������һ���Ƚϴ��͵��������ʱ��(���߿ͻ�����telnet��ÿһ�λس����ᴥ��һ��TCP������)
             * һ��������ܻ��ɶ��TCP�����͹���andle_input��ʱ���޷��жϳ���������ĳ��ȣ��򷵻�0��
             * ��ʾ��Ҫ������ȡ���ݣ�ֱ��handle_input�ܹ��жϳ����󳤶�Ϊֹ
             */
            MSD_INFO_LOG("Unkonw the accurate protocal lenght, do noting!. connection %s:%d", 
                            client->remote_ip, client->remote_port);
            client->status = C_RECEIVING;
            return;
        } 

        if ( client->recvbuf->len >= client->recv_prot_len) 
        {
            client->status = C_PROCESSING;
            /* Ŀǰ��ȡ�������ݣ��Ѿ��㹻ƴ��һ������������������handle_process */
            ret = g_ins->so_func->handle_process(client);
            
            if(MSD_OK == ret)
            {
                /* ����O����ʾ�ɹ������� */
                MSD_INFO_LOG("The handle_process success. Continue. Connection:%s:%d", 
                            client->remote_ip, client->remote_port);
            }
            else if(MSD_END == ret)
            {
                /* ����MSD_END����ʾ�ɹ��������� */
                MSD_INFO_LOG("The handle_process success. End. Connection:%s:%d", 
                            client->remote_ip, client->remote_port);
                //msd_close_client(client->idx, NULL);
                //return;
                client->close_conn = 1; /* response�ɹ�֮���Զ��ر����� */
                msd_ae_delete_file_event(el, fd, MSD_AE_READABLE);
            }
            else
            {
                /* ����ʧ�ܣ�ֱ�ӹر�client */
                MSD_ERROR_LOG("The handle_process failed. End. Connection:%s:%d", 
                            client->remote_ip, client->remote_port);
                msd_close_client(client->idx, NULL);
                return;
            }

            /* ÿ��ֻ��ȡrecv_prot_len���ݳ��ȣ����recvbuf���滹��ʣ�����ݣ���Ӧ�ýس������� */
            if(MSD_OK != msd_str_range(client->recvbuf, client->recv_prot_len,  client->recvbuf->len - 1))
            {
                /* ��recv_prot_len==recvbuf->len��ʱ��range()���ش���ֱ����ջ����� */
                msd_str_clear(client->recvbuf);
            }
                
            /* ��Э�鳤����0����Ϊÿ�������Ƕ����ģ������Э�鳤���ǿ��ܷ����仯������http������ 
               * ��Ҫ��handle_input����ȥʵʱ���� */
            client->recv_prot_len = 0; 
            client->status        = C_WAITING;

            /* �ž���ת */
            if(client->recvbuf->len <= 0)
            {
                return;
            }                     
        }
        else
        {
            /* Ŀǰ��ȡ�������ݻ�������װ����Ҫ������������ȴ���ȡ */
            MSD_DEBUG_LOG("The data lenght not enough, do noting!. connection %s:%d", 
                            client->remote_ip, client->remote_port);
            return;
        }
    }while(1);
    return;
}

/**
 * ����: ��client��дresponse
 * ����: @privdata���ص����������
 * ˵��: 
 *       1.  
 * ����:�ɹ�:0; ʧ��:-x
 **/
void msd_write_to_client(msd_ae_event_loop *el, int fd, void *privdata, int mask) 
{
    msd_conn_client_t *client = (msd_conn_client_t *)privdata;
    int nwrite;
    MSD_AE_NOTUSED(mask);
    
    MSD_DEBUG_LOG("Write to client %s:%d", client->remote_ip, client->remote_port);

    nwrite = write(fd, client->sendbuf->buf, client->sendbuf->len);
    client->access_time = time(NULL);
    if (nwrite < 0) 
    {
        if (errno == EAGAIN) /* ������fdд�룬�����ݲ����� */
        {
            MSD_WARNING_LOG("Write to client temporarily unavailable! IP:%s, Port:%d.", client->remote_ip, client->remote_port);
            nwrite = 0;
        } 
        else if(errno==EINTR)/* �����ж� */
        {  
            MSD_WARNING_LOG("Handle process was interupted! IP:%s, Port:%d. Error:%s.", client->remote_ip, client->remote_port, strerror(errno));
            nwrite = 0; 
        } 
        else 
        {
            MSD_ERROR_LOG("Write to client %s:%d failed:%s", client->remote_ip, client->remote_port, strerror(errno));
            msd_close_client(client->idx, NULL);
            return;
        }
    }
    MSD_INFO_LOG("Write to client! IP:%s, Port:%d. Write_len:%d", client->remote_ip, client->remote_port, nwrite);

    /* ���Ѿ�д������ݣ���sendbuf�вü��� */
    if(MSD_OK != msd_str_range(client->sendbuf, nwrite, client->sendbuf->len-1))
    {
        /* ����Ѿ�д����,��write_len == client->sendbuf->len����msd_str_range����NONEED */
        msd_str_clear(client->sendbuf);
    }

     /* ˮƽ���������sendbuf�Ѿ�Ϊ�գ���ɾ��д�¼�������᲻�ϴ���
       * ע��д�¼���so�е�handle_processȥ��� */
    if(client->sendbuf->len == 0)
    {
        msd_ae_delete_file_event(el, fd, MSD_AE_WRITABLE);

        if (client->close_conn) 
        {
            msd_close_client(client->idx, NULL);
        }
    }
    return;
}

/**
 * ����: worker�̵߳�ʱ��ص�������
 * ����: @el
 *       @id��ʱ���¼�id
 *       @privdata��workerָ��
 * ˵��: 
 *       ����clients���飬�ҳ����㳬ʱ������client�ڵ�
 *       �ر�֮,ÿ��������һ�� 
 * ����:�ɹ�:0; ʧ��:-x
 **/
static int msd_thread_worker_cron(msd_ae_event_loop *el, long long id, void *privdate) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)privdate;
    msd_dlist_iter_t    iter;
    msd_dlist_node_t    *node;
    msd_conn_client_t   *cli;
    time_t              unix_clock = time(NULL);

    MSD_INFO_LOG("Worker[%d] cron begin! Client count:%d", worker->idx, worker->client_list->len);
     
    msd_dlist_rewind(worker->client_list, &iter);

    while ((node = msd_dlist_next(&iter))) 
    {
        cli = node->value;
        /* client��ʱ�жϣ�Ĭ����60�볬ʱ */
        if ( unix_clock - cli->access_time > worker->pool->client_timeout) 
        {
            MSD_DEBUG_LOG("Connection %s:%d timeout closed", 
                    cli->remote_ip, cli->remote_port);
            msd_close_client(cli->idx, "Time out!");
        }
    }
    MSD_INFO_LOG("Worker[%d] cron end! Client count:%d", worker->idx, worker->client_list->len);
    return (1000 * worker->pool->poll_interval);
}

/**
 * ����: �ر��Լ������ȫ��client
 * ����: @el
 *       @id��ʱ���¼�id
 *       @privdata��workerָ��
 * ˵��: 
 *       1. ����clients���飬�ر�ȫ��client 
 *       2. �������г�Ա����Դ
 *       3. �߳��˳�
 * ����:�ɹ�:0; ʧ��:-x
 **/
static void msd_thread_worker_shut_down(msd_thread_worker_t *worker)
{
    msd_dlist_iter_t    iter;
    msd_dlist_node_t    *node;
    msd_conn_client_t   *cli;

    MSD_INFO_LOG("Worker[%d] begin to shutdown! Client count:%d", worker->idx, worker->client_list->len);
    worker->status = W_STOPING;
    /* ���� */
    msd_dlist_rewind(worker->client_list, &iter);
    while ((node = msd_dlist_next(&iter))) 
    {
        cli = node->value;
        msd_close_client(cli->idx, "Mossad is shutting down!");
    }
    MSD_INFO_LOG("Worker[%d] end shutdown! Client count:%d", worker->idx, worker->client_list->len);    

    /* ȥ���¼�������AEֹͣ */
    msd_ae_delete_file_event(worker->t_ael, worker->notify_read_fd, MSD_AE_READABLE | MSD_AE_WRITABLE);
    msd_ae_stop(worker->t_ael);
    close(worker->notify_read_fd);
    close(worker->notify_write_fd);    
}

/**
 * ����: ����worker��id�����worker���
 * ����: �ɹ�:worker���; ʧ��:-x
 **/
msd_thread_worker_t * msd_get_worker(int worker_id)
{
    msd_thread_worker_t *worker;

    if(worker_id < 0 || worker_id > g_ins->pool->thread_worker_num-1)
    {
        return (msd_thread_worker_t *)MSD_ERR;
    }
    
    worker = *(g_ins->pool->thread_worker_array+ worker_id);  
    return worker;
}


#ifdef __MSD_THREAD_TEST_MAIN__

int g_int = 0;
int fd;

//�����߳���
//����������������յ��ļ�������������������˳�򲻶�
//��������������յ��ļ�������������˳����ȫ��ȷ
void* task(void* arg) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)arg;
    
    MSD_LOCK_LOCK(worker->pool->thread_lock);
    msd_thread_sleep(worker->idx); // ˯��
    g_int++;
    printf("I am thread %d, I get the g_int:%d. My tid:%lu, pool:%lu\n", worker->idx, g_int, (long unsigned int)worker->tid, (long unsigned int)worker->pool);
    write(fd, "hello\n", 6);
    write(fd, "abcde\n", 6);
    MSD_LOCK_UNLOCK(worker->pool->thread_lock);
    
    return NULL;
}

int main(int argc, char *argv[]) 
{
    int i=0;
    fd = open("test.txt", O_WRONLY|O_CREAT, 0777);
    msd_log_init("./","thread.log", 4, 10*1024*1024, 1, 0); 
    //msd_thread_pool_create(1000, 1024*1024, task);
    msd_thread_pool_t *pool = msd_thread_pool_create(10, 1024*1024, task);
    msd_thread_sleep(1);//˯��һ�룬���̳߳������������
    
    /* ע�⣬����߳���detache״̬�������join��ʧЧ
     * ���ǣ�detached���̣߳�������Ϊ���߳��˳����˳�������
     */
    for(i=0; i<10; i++)
    {
        pthread_join(pool->thread_worker_array[i]->tid, NULL);
        printf("Thread %d(%lu) exit\n", i, pool->thread_worker_array[i]->tid);
    }
    printf("The final val:%d\n", g_int);
    msd_thread_pool_destroy(pool);
    return 0;
}

#endif /* __MSD_THREAD_TEST_MAIN__ */

