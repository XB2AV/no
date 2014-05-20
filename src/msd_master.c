/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_master.c
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
#include "msd_core.h"

extern msd_instance_t *g_ins;

static void msd_master_accept(msd_ae_event_loop *el, int fd, 
        void *client_data, int mask);
static int msd_create_client(int cli_fd, const char *cli_ip, 
        int cli_port);
static int msd_client_find_free_slot();
static int msd_client_clear(msd_conn_client_t *client);
static int msd_thread_worker_dispatch(int client_idx);
static int msd_thread_list_find_next();
static int msd_master_cron(msd_ae_event_loop *el, long long id, void *privdate);
static void msd_master_recv_signal(msd_ae_event_loop *el, int fd, 
        void *client_data, int mask);
static void msd_shut_down();

/**
 * ����: ���̹߳���
 * ����: 
 * ����:
 *      1. client_vec��Ԫ��ʦconn_client��ָ��!������conn_client����
 * ����:�ɹ�:���һֱ������ae_main���棬������; ʧ��:-x
 **/
int msd_master_cycle() 
{
    int  listen_fd;
    char error_buf[MSD_ANET_ERR_LEN];
    
    memset(error_buf, 0, MSD_ANET_ERR_LEN);
    msd_master_t *master = calloc(1, sizeof(msd_master_t));
    if(!master)
    {
        MSD_ERROR_LOG("Create master failed");
        return MSD_ERR;
    }
    g_ins->master          = master;
    master->start_time     = time(NULL);
    master->cur_conn       = -1;
    master->cur_thread     = -1;
    master->total_clients  = 0;
    master->client_limit   = msd_conf_get_int_value(g_ins->conf, "client_limit", 100000);
    master->poll_interval  = msd_conf_get_int_value(g_ins->conf, "master_poll_interval", 600);
    master->m_ael          = msd_ae_create_event_loop();
    if(!master->m_ael)
    {
        MSD_ERROR_LOG("Create AE Pool failed");
        free(master);
        return MSD_ERR;
    }
    
    master->client_vec     = msd_vector_new(master->client_limit, sizeof(msd_conn_client_t *));
    if(!master->client_vec)
    {
        MSD_ERROR_LOG("Create AE Pool failed");
        msd_ae_free_event_loop(master->m_ael);
        free(master);
        return MSD_ERR;    
    }
    
    /* ���������� */
    listen_fd = msd_anet_tcp_server(error_buf, 
        msd_conf_get_str_value(g_ins->conf, "host", NULL), /* NULL��ʾbind�������� */
        msd_conf_get_int_value(g_ins->conf, "port", 9527)
    );
    if (listen_fd == MSD_ERR) 
    {
        MSD_ERROR_LOG("Create Server failed");
        msd_ae_free_event_loop(master->m_ael);
        free(master);
        return MSD_ERR;
    }

    MSD_INFO_LOG("Create Server Success, listen_fd:%d", listen_fd);
    
    /* ���÷�������nodelay */
    if((MSD_OK != msd_anet_nonblock(error_buf, listen_fd)) 
        || (MSD_OK != msd_anet_tcp_nodelay(error_buf, listen_fd)))
    {
        MSD_ERROR_LOG("Set noblock or nodelay failed");
        msd_ae_free_event_loop(master->m_ael);
        close(listen_fd);
        free(master);
        return MSD_ERR;        
    }
    MSD_DEBUG_LOG("Set Nonblock and Nodelay Success");
    master->listen_fd = listen_fd;

    /* ע��listen_fd�Ķ�ȡ�¼� */
    if (msd_ae_create_file_event(master->m_ael, listen_fd, 
                MSD_AE_READABLE, msd_master_accept, NULL) == MSD_ERR) 
    {
        MSD_ERROR_LOG("Create file event failed");
        msd_ae_free_event_loop(master->m_ael);
        close(listen_fd);
        free(master);
        return MSD_ERR; 
    }

    /* ע��sig_worker->notify_read_fd�Ķ�ȡ�¼� */
    if (msd_ae_create_file_event(master->m_ael, g_ins->sig_worker->notify_read_fd, 
                MSD_AE_READABLE, msd_master_recv_signal, NULL) == MSD_ERR) 
    {
        MSD_ERROR_LOG("Create file event failed");
        msd_ae_free_event_loop(master->m_ael);
        close(listen_fd);
        free(master);
        return MSD_ERR; 
    }

    /* ע��masterʱ���¼� */
    if(msd_ae_create_time_event(master->m_ael, master->poll_interval*1000, 
            msd_master_cron, master, NULL) == MSD_ERR)
    {
        MSD_ERROR_LOG("Create time event failed");
        msd_ae_free_event_loop(master->m_ael);
        close(listen_fd);
        free(master);
        return MSD_ERR; 
    }

    MSD_INFO_LOG("Create Master Ae Success");
    msd_ae_main_loop(master->m_ael);
    return MSD_OK;
}

/**
 * ����: ����master��Դ
 * ����: 
 * ����:
 *      1. 
 **/
int msd_master_destroy()
{
    //TODO
    return 0;
}
 
/**
 * ����: ���߳�accept����
 * ����: @el, ae��� 
 *       @fd, ��Ҫaccept��listen_fd 
 *       @client_data, �������
 *       @mask, ��Ҫ������¼� 
 * ����:
 *      1. �˺����ǻص����������еĲ�����������ae_main����
 *         process_event������ʱ�����
 **/
static void msd_master_accept(msd_ae_event_loop *el, int fd, 
        void *client_data, int mask)
{
    int cli_fd, cli_port;
    char cli_ip[16];
    char error_buf[MSD_ANET_ERR_LEN];
    int worker_id;
    int client_idx;
    
    MSD_AE_NOTUSED(el);
    MSD_AE_NOTUSED(mask);
    MSD_AE_NOTUSED(client_data);

    MSD_DEBUG_LOG("Master Begin Accept");
    /* ���client��fd������ip��port */   
    cli_fd = msd_anet_tcp_accept(error_buf, fd, cli_ip, &cli_port);
    if (cli_fd == MSD_ERR) 
    {
        MSD_ERROR_LOG("Accept failed:%s", error_buf);
        return;
    }

    MSD_INFO_LOG("Receive connection from %s:%d.The client_fd is %d.", cli_ip, cli_port, cli_fd);

    /* ���÷�������nodelay */
    if((MSD_OK != msd_anet_nonblock(error_buf, cli_fd)) 
        || (MSD_OK != msd_anet_tcp_nodelay(error_buf, cli_fd)))
    {
        close(cli_fd);
        MSD_ERROR_LOG("Set client_fd noblock or nodelay failed");
        return;        
    }    

    /* ����client�ṹ */
    if(MSD_ERR == (client_idx = msd_create_client(cli_fd, cli_ip, cli_port)))
    {
        close(cli_fd);
        MSD_ERROR_LOG("Create client failed, client_idx:%d", client_idx);
        return;         
    }

    /* �����·� */
    if(MSD_ERR == (worker_id = msd_thread_worker_dispatch(client_idx)))
    {
        msd_close_client(client_idx, "Dispatch failed!");
        MSD_ERROR_LOG("Worker dispatch failed, worker_id:%d", worker_id);
    }

    MSD_INFO_LOG("Worker[%d] Get the client[%d] task. Ip:%s, Port:%d", worker_id, client_idx, cli_ip, cli_port);
    return;
}

/**
 * ����: ���̴߳���client����
 * ����: @cli_fd��client��Ӧ��fd
 *       @cli_ip��client IP
 *       @cli_port,client Port
 * ����:
 *      1. cliet->magic����ʼ��ָΪħ�������û���������Ƿ�Ƿ�����
 *      2. msd_vector_set_at�ĵ�����������Ԫ�ص�ָ�룬��conn_vec��Ԫ����ָ������,
 *         ����msd_vector_set_at�� ����������Ӧ���Ǹ�����ָ�� 
 * ����:�ɹ�:������client��idx��������; ʧ��:-x 
 **/
static int msd_create_client(int cli_fd, const char *cli_ip, int cli_port)
{
    int idx;
    msd_master_t *master = g_ins->master;
    msd_conn_client_t **pclient;
    msd_conn_client_t  *client;
    
    idx = msd_client_find_free_slot();
    if(MSD_ERR == idx)
    {
        MSD_ERROR_LOG("Max client num. Can not create more");

        /* ����д��ʧ��ԭ��cli_fd������ */
        write(cli_fd, "Max client num.\n", strlen("Max client num.\n"));
        return MSD_ERR;
    }

    MSD_LOCK_LOCK(g_ins->client_conn_vec_lock);
    pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, idx);
    client  = *pclient;
    if (!client)
    {
        /* client�п���Ϊ�գ��������û�з��ù�client���ͻ��ǿ�
         * ���������client��close�ˣ����ǿ�
         */
        client = (msd_conn_client_t *)calloc(1, sizeof(*client));
        if (!client) 
        {
            MSD_ERROR_LOG("Create client connection structure failed");
            MSD_LOCK_UNLOCK(g_ins->client_conn_vec_lock);
            return MSD_ERR;
        }
        msd_vector_set_at(master->client_vec, idx, (void *)&client);
        MSD_DEBUG_LOG("Create client struct.Idx:%d", idx);
    }

    /* client�������� */
    master->total_clients++;
    MSD_LOCK_UNLOCK(g_ins->client_conn_vec_lock);
    
    msd_client_clear(client);
    /* ��ʼ��cli�ṹ */
    client->magic = MSD_MAGIC_DEBUG;  /* ��ʼ��ħ���� */
    client->fd = cli_fd;
    client->close_conn = 0;
    client->recv_prot_len = 0;
    client->remote_ip = strdup(cli_ip);
    client->remote_port = cli_port;
    client->recvbuf = msd_str_new_empty(); 
    client->sendbuf = msd_str_new_empty(); 
    client->access_time = time(NULL);    
    client->idx = idx;

    return idx;

}


/**
 * ����: ��master�̵߳�conn_vec���ҵ�һ�����е�λ��
 * ����:
 *      1. �����Ǵ�last_conn��ʼ�ģ��������Ա���ÿ�ζ���ͷ��ʼ�����Ч��
 *         ����Ҳ������м���ֿ׶�(ĳЩclient�ر�������)������mod������
 *         ʵ��ѭ������׶�
 *      2. �ҵ�client�����������clientΪNULL��˵������û�г�ʼ�������
 *         client->access_timeΪ�գ�˵���ǿ׶���������client�ѹر�
 * ����:�ɹ�:0��������; ʧ��:-x 
 **/
static int msd_client_find_free_slot()
{
    int i;
    int idx = -1;
    msd_master_t *master = g_ins->master;
    msd_conn_client_t **pclient;
    msd_conn_client_t *client;
    
    MSD_LOCK_LOCK(g_ins->client_conn_vec_lock);
    for (i = 0; i < master->client_limit; i++)
    {
        idx = (i + master->cur_conn+ 1) % master->client_limit;
        pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, idx);
        client = *pclient;
        if (!client || 0 == client->access_time)
        {
            master->cur_conn = idx;
            break;
        }
    }
    MSD_LOCK_UNLOCK(g_ins->client_conn_vec_lock);

    if (i == master->client_limit)
        idx = -1;

    return idx;
}

/**
 * ����: ���conn_client�ṹ
 * ����:
 *      1. ֻ�����client�ṹ����ĳ�Ա��client�����ͷ�
 *      2. ������ָ��ĳ�Ա����Ҫ���ö���free
 * ����:�ɹ�:0��������; ʧ��:-x 
 **/
static int msd_client_clear(msd_conn_client_t *client)
{
    assert(client);
    if (client)
    {
        client->fd = -1;
        client->close_conn= 0;
        if(client->remote_ip)
        {
            free(client->remote_ip);
            client->remote_ip = NULL;
        }    
        client->remote_ip = 0;
        client->remote_port = 0;
        client->recv_prot_len = 0;
        if(client->recvbuf)
        {
            msd_str_free(client->recvbuf);
            client->recvbuf = NULL;
        }
        if(client->sendbuf)
        {
            msd_str_free(client->sendbuf);
            client->sendbuf= NULL;
        }
        client->access_time = 0;

        client->worker_id = -1;
        return 0;
    }
    return MSD_OK;
}

/**
 * ����: �������
 * ����: @client_idx: client��λ��
 * ����:
 *      1. 
 * ����:�ɹ�:������woker��id��������; ʧ��:-x 
 **/
static int msd_thread_worker_dispatch(int client_idx)
{
    int res;
    int worker_id = msd_thread_list_find_next();
    msd_thread_pool_t *worker_pool = g_ins->pool;
    msd_thread_worker_t *worker;
    
    if (worker_id < 0)
    {
        return MSD_ERR;
    }

    worker = *(worker_pool->thread_worker_array+ worker_id);
    /* �������д��֪ͨ! */
    res = write(worker->notify_write_fd, &client_idx, sizeof(int));
    if (res == -1)
    {
        MSD_ERROR_LOG("Pipe write error, errno:%d", errno);
        return MSD_ERR;
    }
    MSD_DEBUG_LOG("Notify the worker[%d] process the client[%d]", worker_id, client_idx);
    return worker_id;
}

/**
 * ����: ��woker�������ҵ�һ�����õ�
 * ����:
 *      1. ֱ����woker����������ң�����modʵ��ѭ��
 *      2. //TODO: Ӧ�ð��շ�æ�̶ȣ��ҵ����е�woker����������ѵ
 * ����:�ɹ�:woker������. ʧ��:-x 
 **/
static int msd_thread_list_find_next()
{
    int i;
    int idx = -1;
    msd_master_t *master = g_ins->master;
    msd_thread_pool_t *pool = g_ins->pool;

    MSD_LOCK_LOCK(g_ins->thread_woker_list_lock);
    for (i = 0; i < pool->thread_worker_num; i++)
    {
        idx = (i + master->cur_thread + 1) % pool->thread_worker_num;
        if (*(pool->thread_worker_array + idx) && (*(pool->thread_worker_array + idx))->tid)
        {
            master->cur_thread = idx;
            break;
        }
    }
    MSD_LOCK_UNLOCK(g_ins->thread_woker_list_lock);

    if (i == pool->thread_worker_num)
    {
        MSD_ERROR_LOG("Thread pool full, Can not find a free worker!");
        idx = -1;
    }
    return idx;
}


/**
 * ����: �ر�client
 * ����: @client_idx
 *       @info���رյ���ʾ��Ϣ
 * ˵��: 
 *    1. 
 **/
void msd_close_client(int client_idx, const char *info) 
{
    msd_conn_client_t **pclient;
    msd_conn_client_t  *client;
    msd_conn_client_t  *null = NULL;
    msd_thread_worker_t *worker = NULL;
    msd_dlist_node_t   *node;

    MSD_LOCK_LOCK(g_ins->client_conn_vec_lock);
    pclient = (msd_conn_client_t **)msd_vector_get_at(g_ins->master->client_vec,client_idx);
    client  = *pclient;

    /* ����handle_close */
    if (g_ins->so_func->handle_close) 
    {
        g_ins->so_func->handle_close(client, info);
    }

    /* ɾ��client��Ӧfd��ae�¼���woker->client_list�г�Ա */
    worker = g_ins->pool->thread_worker_array[client->worker_id];
    if(worker)
    {
        msd_ae_delete_file_event(worker->t_ael, client->fd, MSD_AE_READABLE | MSD_AE_WRITABLE);
        if((node = msd_dlist_search_key(worker->client_list, client)))
        {
            MSD_INFO_LOG("Delete the client node[%d]. Addr:%s:%d", client->idx, client->remote_ip, client->remote_port);
            msd_dlist_delete_node(worker->client_list, node);
        }
        else
        {
            MSD_ERROR_LOG("Lost the client node[%d]. Addr:%s:%d", client->idx, client->remote_ip, client->remote_port);
        }
    }
    else
    {
        /* �����·�ʧ�ܣ�Ҳ�����msd_close_client����ʱworker_id�ȵ�
         * �����Ϣ��û��client���й��������Կ��ܳ���wokerΪ�� */
        MSD_ERROR_LOG("The worker not found!!");
    }
    
    /* ɾ��conn_vec��Ӧ�ڵ㣬msd_vctor_set_at�ĵ�����������data��ָ�룬
     * ���ڴ���data�����client_conn��ָ�룬���Ե���������Ӧ���Ǹ�����ָ�� 
     **/
    msd_vector_set_at(g_ins->master->client_vec, client->idx, (void *)&null);
    g_ins->master->total_clients--;
    MSD_LOCK_UNLOCK(g_ins->client_conn_vec_lock);
    
    /* close��client��Ӧfd */
    close(client->fd);

    /* ����msd_client_clear()���client�ṹ */
    msd_client_clear(client);

    /* free client���� */
    free(client);

    MSD_INFO_LOG("Close client[%d], info:%s", client->idx, info);

}

/**
 * ����: master�̵߳�ʱ��ص�������
 * ����: @el
 *       @id��ʱ���¼�id
 *       @privdata��masterָ��
 * ˵��: 
 *       ����ͳ��ȫ��client����Ϣ���鿴�Ƿ�����쳣
 *       5��������һ��
 * ����:�ɹ�:0; ʧ��:-x
 **/
static int msd_master_cron(msd_ae_event_loop *el, long long id, void *privdate) 
{
    msd_master_t *master = (msd_master_t *)privdate;     
    //msd_vector_iter_t iter = msd_vector_iter_new(master->client_vec);
    msd_conn_client_t **pclient;
    msd_conn_client_t *client;
    msd_thread_pool_t *pool = g_ins->pool;
    msd_thread_worker_t *worker;
    msd_dlist_iter    dlist_iter;
    msd_dlist_node_t  *node;
    int i;
    int master_client_cnt = 0;
    int worker_client_cnt = 0;
    
    MSD_INFO_LOG("Master cron begin!");
    /* ����client_vec */
    MSD_LOCK_LOCK(g_ins->client_conn_vec_lock);
    
    for( i=0; i < master->client_limit; i++)
    {
        pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, i);
        //printf("%p,             %p\n", pclient, *pclient);
        client  = *pclient;
        if( client && 0 != client->access_time )
        {
            master_client_cnt++;
        }
    }
    /*
    //�δ���֮���Զδ�������Ϊ����msd_vector_iter_next�������������vec->count��������
    do {
        pclient = (msd_conn_client_t **)(iter->data);
        client  = *pclient;
        printf("a\n");
        if(!(!client || 0 == client->access_time))
        {
            master_client_cnt++;
        }
    } while (msd_vector_iter_next(iter) == 0);
    */
    MSD_LOCK_UNLOCK(g_ins->client_conn_vec_lock);
    
    /* �������е�worker�е�client_list */
    MSD_LOCK_LOCK(g_ins->thread_woker_list_lock);
    for (i = 0; i < pool->thread_worker_num; i++)
    {
        if (*(pool->thread_worker_array + i) && (*(pool->thread_worker_array + i))->tid)
        {
            worker = *(pool->thread_worker_array+i);
            msd_dlist_rewind(worker->client_list, &dlist_iter);
            while ((node = msd_dlist_next(&dlist_iter))) 
            {
                client = node->value;
                if(client && 0 != client->access_time)
                {
                    worker_client_cnt++;
                }
            }
            
        }
    }
    MSD_LOCK_UNLOCK(g_ins->thread_woker_list_lock);    
    
    if(master->total_clients == master_client_cnt && master_client_cnt == worker_client_cnt )
    {
        MSD_INFO_LOG("Master cron end! Total_clients:%d, Master_client_cnt:%d, Worker_client_cnt:%d", 
            master->total_clients, master_client_cnt, worker_client_cnt);
    }
    else
    {
        //TODO ������ϸ��������Ϣ!
        MSD_FATAL_LOG("Master cron end! Fatal Error!. Total_clients:%d, Master_client_cnt:%d, Worker_client_cnt:%d", 
            master->total_clients, master_client_cnt, worker_client_cnt);        
    }
    
    return (1000 * master->poll_interval);
}

/**
 * ����: ���߳̽���singal�̴߳��������ź���Ϣ
 * ����: @el, ae��� 
 *       @fd, signal�߳�֪ͨfd 
 *       @client_data, �������
 *       @mask, ��Ҫ������¼� 
 * ����:
 *      1. �˺����ǻص����������еĲ�����������ae_main����
 *         process_event������ʱ�����
 **/
static void msd_master_recv_signal(msd_ae_event_loop *el, int fd, 
        void *client_data, int mask)
{
    char msg[128];
    int read_len;
    
    MSD_AE_NOTUSED(el);
    MSD_AE_NOTUSED(mask);
    MSD_AE_NOTUSED(client_data);

    memset(msg, 0, 128);

    read_len = read(fd, msg, 4);
    if(read_len != 4)
    {
        MSD_ERROR_LOG("Master recv signal worker's notify error: %d", read_len);
        return;
    }
    
    MSD_DEBUG_LOG("Master recv signal worker's notify: %s", msg);
    if(strncmp(msg, "stop", 4) == 0)
    {
        //exit(0);
        msd_shut_down();
    }
    
    return;
}

/**
 * ����: master����mossad�ر�ָ��
 * ����:
 *      0. �ر�listen_fd��ֹͣ�µ���������
 *      1. ������worker�̷߳����ر�ָ�Լ��fd:-1
 *      2. ѭ���ȴ�ȫ��worker�ر�����(�ⲽ������Ҫ�ģ�
 *         ���еĹ�������Ϊ��������client���Ѻ��˳�)
 *      3. �����˳�
 **/
static void msd_shut_down()
{
    msd_master_t *master    = g_ins->master;
    msd_thread_pool_t *pool = g_ins->pool;
    msd_thread_worker_t *worker;
    msd_conn_client_t  **pclient;
    msd_conn_client_t   *client;
    int master_client_cnt;
    int i, res, info;

    /* �ر�listen_fd��ֹͣ�µ��������� */
    msd_ae_delete_file_event(master->m_ael, master->listen_fd, MSD_AE_READABLE);
    close(master->listen_fd); 
    /* ���˴���mossad���ڰ�رգ�����޷������������ˣ�����ԭ��������Ȼ���� */

    /* ������worker�̷߳����ر�ָ�Լ��fd:-1 */
    MSD_LOCK_LOCK(g_ins->thread_woker_list_lock);
    for (i = 0; i < pool->thread_worker_num; i++)
    {
        if (*(pool->thread_worker_array + i) && (*(pool->thread_worker_array + i))->tid)
        {
            worker = *(pool->thread_worker_array+i);
            info = -1;
            res = write(worker->notify_write_fd, &info, sizeof(int));
            if (res == -1)
            {
                MSD_ERROR_LOG("Pipe write error, errno:%d", errno);
                MSD_LOCK_UNLOCK(g_ins->thread_woker_list_lock);  
                return;
            }
        }
    }
    MSD_LOCK_UNLOCK(g_ins->thread_woker_list_lock);  

    /* ѭ���ȴ����е�worker�����ر� */
    do{
        /* ����client_vec���鿴���Ӹ��� */
        printf("bbbbbbbbbbbbbbbbbbbbbb\n");
        master_client_cnt = 0;
        MSD_LOCK_LOCK(g_ins->client_conn_vec_lock);
        for( i=0; i < master->client_limit; i++)
        {
            pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, i);
            client  = *pclient;
            if( client && 0 != client->access_time )
            {
                master_client_cnt++;
            }
        }
        MSD_LOCK_UNLOCK(g_ins->client_conn_vec_lock);

        if(master_client_cnt <= 0)
        {
            break;
        }
        else
        {
            printf("aaaaaaaaaaaaaaaaaa\n");
            msd_thread_usleep(10000);/* 10���� */
        }
    }while(1);
    
    /* �����̳߳� */
    msd_thread_pool_destroy(pool);

    return;
}



