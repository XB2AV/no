/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_thread.c
 * 
 * Description :  Msd_thread, �����߳����ʵ��.
 * 
 *     Created :  Apr 20, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
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

static int msd_thread_worker_destroy(msd_thread_worker_t *worker);

static void msd_read_from_client(msd_ae_event_loop *el, int fd, void *privdata, int mask);

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
    
    /* ��ʼ������woker�߳� */
    for (i = 0; i < worker_num; ++i) 
    {
        if(msd_thread_worker_create(pool, worker_task, i) != MSD_OK)
        {
            MSD_ERROR_LOG("Msd_thread_worker_create failed, idx:%d", i);
            return NULL;
        }
    }
    
    /* ��ʼ�������źŴ����߳� */
    //TODO
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
    MSD_LOCK_LOCK(pool->thread_lock);
    for(i=0; i<pool->thread_worker_num; i++)
    {
        msd_thread_worker_destroy(pool->thread_worker_array[i]);
    }
    MSD_LOCK_UNLOCK(pool->thread_lock);
    MSD_LOCK_DESTROY(pool->thread_lock);
    free(pool->thread_worker_array);
    free(pool);
    return MSD_OK;
}

/**
 * ����: ����worker�̳߳�
 * ����: @pool: �̳߳ؾ�� 
 * ����:�ɹ�:0; ʧ��:-x
 **/
static int msd_thread_worker_destroy(msd_thread_worker_t *worker) 
{
    assert(worker);
    //TODO
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
 * ����: worker�̹߳�������
 * ����: @arg: �̺߳�����ͨ����˵�������߳���������ָ��
 * ˵��: 
 *       1.  
 * ����:�ɹ�:0; ʧ��:-x
 **/
void* msd_thread_worker_cycle(void* arg) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)arg;
    MSD_INFO_LOG("Thread no.%d begin to work", worker->idx);

    
    if (msd_ae_create_file_event(worker->t_ael, worker->notify_read_fd, 
                MSD_AE_READABLE, msd_thread_worker_process_notify, arg) == MSD_ERR) 
    {
        MSD_ERROR_LOG("Create file event failed");
        msd_ae_free_event_loop(worker->t_ael);
        return (void *)NULL; 

    }

    //TODO
    //������ʱ��������ʱ��client

    
    msd_ae_main_loop(worker->t_ael);
 
    return (void*)NULL;
}

/**
 * ����: worker�̹߳�������
 * ����: @arg: �̺߳�����ͨ����˵�������߳���������ָ��
 * ˵��: 
 *       1.  
 * ����:�ɹ�:0; ʧ��:-x
 **/
static void msd_thread_worker_process_notify(struct msd_ae_event_loop *el, int notify_fd, 
        void *client_data, int mask)
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)client_data;
    msd_master_t        *master = g_ins->master;
    int                  client_idx;
    msd_conn_client_t   **pclient = NULL;
    int                  read_len;

    /* ��ȡclient idx */
    if((read_len = read(notify_fd, &client_idx, sizeof(int)))!= sizeof(int))
    {
        MSD_ERROR_LOG("Worker[%d] read the notify_fd error! read_len:%d", worker->idx, read_len);
        return;
    }
    MSD_INFO_LOG("Worker[%d] Get the task. Client_idx[%d]", worker->idx, client_idx);

    /* ����client idx��ȡ��client��ַ */
    pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, client_idx);

    /* ��worker_idд��client�ṹ */
    (*pclient)->worker_id = worker->idx;
    //TODO Ӧ�ü�һ����woker->conn����֮��Ĳ���

    
    /* ��ӭ��Ϣ */
    if(g_ins->so_func->handle_open)
    {
        if(MSD_OK != g_ins->so_func->handle_open(*pclient))
        {
            msd_close_client(client_idx, NULL);
            MSD_ERROR_LOG("Handle open error! Close connection. IP:%s, Port:%d.", 
                            (*pclient)->remote_ip, (*pclient)->remote_port);
            return;
        }
    }
        
    /* ע���ȡ�¼� */
    if (msd_ae_create_file_event(worker->t_ael, (*pclient)->fd, MSD_AE_READABLE,
                msd_read_from_client, *pclient) == MSD_ERR) 
    {
        msd_close_client(client_idx, "create file event failed");
        MSD_ERROR_LOG("Create read file event failed for connection:%s:%d",
                (*pclient)->remote_ip, (*pclient)->remote_port);
        return;
    }  
}

/**
 * ����: client�ɶ�ʱ�Ļص�������
 * ����: @arg: �̺߳�����ͨ����˵�������߳���������ָ��
 * ˵��: 
 *       1.  
 * ����:�ɹ�:0; ʧ��:-x
 **/
static void msd_read_from_client(msd_ae_event_loop *el, int fd, void *privdata, int mask) 
{
    msd_conn_client_t *client = (msd_conn_client_t *)privdata;
    int nread;
    char buf[MSD_IOBUF_SIZE];
    MSD_AE_NOTUSED(el);  /* ��ʹ�ã�ɶҲ���� */
    MSD_AE_NOTUSED(mask);
    
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

    if (client->recv_prot_len == 0) 
    {
        /* ÿ�ζ�ȡ�ĳ��Ȳ���������recv_prot_len��һ�����������ϵ��� */ 
        client->recv_prot_len = g_ins->so_func->handle_input(client);
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
        return;
    } 

    if ( client->recvbuf->len >= client->recv_prot_len) 
    {
        //TODO ����ط�����Ҫ��һЩ���Ӳ�⣬����http��transfer�ȣ�
        //������֧����һ�飬����һ��echo����
        msd_close_client(client->idx, "fuck!");return;
        /* Ŀǰ��ȡ�������ݣ��Ѿ��㹻ƴ��һ������������������handle_process */
        if(MSD_OK != g_ins->so_func->handle_process(client))
        {
            MSD_ERROR_LOG("The handle_process failed. connection %s:%d", 
                        client->remote_ip, client->remote_port);
        }
        else
        {
            MSD_DEBUG_LOG("The handle_process success. connection %s:%d", 
                        client->remote_ip, client->remote_port);
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
                              
    }
    else
    {
        /* Ŀǰ��ȡ�������ݻ�������װ����Ҫ������������ȴ���ȡ */
        MSD_DEBUG_LOG("The data lenght not enough, do noting!. connection %s:%d", 
                        client->remote_ip, client->remote_port);
    }
    return;
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
