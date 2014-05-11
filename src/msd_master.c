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
static int msd_conn_client_find_free_slot();
static int msd_conn_client_clear(msd_conn_client_t *client);
static int msd_thread_worker_dispatch(int client_idx);
static int msd_thread_list_find_next();
static void msd_close_client(int client_idx);


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
    master->client_limit   = msd_conf_get_int_value(g_ins->conf, "client_limit", 10000);
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

    MSD_LOCK_INIT(master->thread_woker_list_lock);
    
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
    MSD_INFO_LOG("Create Master Ae Success");
    msd_ae_main_loop(master->m_ael);
    /*
    if (qbh_ae_create_file_event(ael, notifier, QBH_AE_READABLE, 
                qbh_notifier_handler, NULL) == QBH_ERROR) 
    {
        qbh_boot_notify(-1, "Create notifier file event");
        kill(getppid(), SIGQUIT); 
        exit(0);
    }

    if (dll.handle_init) 
    {
        if (dll.handle_init(data, qbh_process) != 0) 
        {
            qbh_boot_notify(-1, "Invoke handle_init hook in conn process");
            kill(getppid(), SIGQUIT); 
            exit(0);
        }
    }

    qbh_redirect_std();
    qbh_ae_main(ael);

    if (dll.handle_fini) 
    {
        dll.handle_fini(data, qbh_process);
    }

    qbh_ae_free_event_loop(ael);
    qbh_dlist_destroy(clients);
    qbh_vector_free(conn_vec);
    exit(0);
    */
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
        msd_close_client(client_idx);
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
    
    idx = msd_conn_client_find_free_slot();
    if(MSD_ERR == idx)
    {
        MSD_ERROR_LOG("Max client num. Can not create more");
        return MSD_ERR;
    }

    pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, idx);
    client  = *pclient;
    if (!client)
    {
        client = (msd_conn_client_t *)calloc(1, sizeof(*client));
        if (!client) 
        {
            MSD_ERROR_LOG("Create client connection structure failed");
            return MSD_ERR;
        }
        msd_vector_set_at(master->client_vec, idx, (void *)&client);
        MSD_DEBUG_LOG("Create client struct.Idx:%d", idx);
    }

    msd_conn_client_clear(client);

    /* ��ʼ��cli�ṹ */
    client->magic = MSD_MAGIC_DEBUG;  /* ��ʼ��ħ���� */
    client->fd = cli_fd;
    client->close_conn = 0;
    client->recv_prot_len = 0;
    client->remote_ip = strdup(cli_ip);
    client->remote_port = cli_port;
    client->recvbuf = msd_str_new_empty(); //?
    client->sendbuf = msd_str_new_empty(); //?
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
static int msd_conn_client_find_free_slot()
{
    int i;
    int idx = -1;
    msd_master_t *master = g_ins->master;
    msd_conn_client_t **pclient;
    msd_conn_client_t *client;
    
    //�б�Ҫ��?
    //pthread_mutex_lock(&conn_list_lock);
    for (i = 0; i < master->client_limit; i++)
    {
        //printf("loop\n");
        idx = (i + master->cur_conn+ 1) % master->client_limit;
        pclient = (msd_conn_client_t **)msd_vector_get_at(master->client_vec, idx);
        client = *pclient;
        if (!client || 0 == client->access_time)
        {
            master->cur_conn = idx;
            break;
        }
    }
    //pthread_mutex_unlock(&conn_list_lock);

    if (i == master->client_limit)
        idx = -1;

    return idx;
}

/**
 * ����: ���conn_client�ṹ
 * ����:
 *      1. 
 * ����:�ɹ�:0��������; ʧ��:-x 
 **/
static int msd_conn_client_clear(msd_conn_client_t *client)
{
    if (client)
    {
        client->fd = 0;
        client->close_conn= 0;
        free(client->remote_ip);
        client->remote_ip = 0;
        client->remote_port = 0;
        client->recv_prot_len = 0;
        msd_str_free(client->recvbuf);
        client->recvbuf = NULL;
        msd_str_free(client->sendbuf);
        client->sendbuf= NULL;
        client->access_time = 0;

        client->worker_id = 0;

        /*
        thread_worker * worker = *(g_worker_list + client->worker_id);
        /// todo free every connection comes?
        if (client->read_buffer)
        {
            if(client->r_buf_arena_id >= 0)
            {
                worker->mem.free_block(client->r_buf_arena_id);
                client->r_buf_arena_id = INVILID_BLOCK;
                client->read_buffer = NULL;
            }
            else
            {
                free(client->read_buffer);
                client->read_buffer = NULL;
            }
        }
        client->rbuf_size = RBUF_SZ;
        client->have_read = 0;
        client->need_read = CMD_PREFIX_BYTE;

        /// todo free every connection comes?
        if (client->__write_buffer)
        {
            if(client->w_buf_arena_id >= 0)
            {
                worker->mem.free_block(client->w_buf_arena_id);
                client->w_buf_arena_id = INVILID_BLOCK;
                client->__write_buffer = NULL;
            }
            else
            {
                free(client->__write_buffer);
                client->__write_buffer = NULL;
            }
        }
        client->wbuf_size = WBUF_SZ - CMD_PREFIX_BYTE;
        client->have_write = 0;
        client->__need_write = CMD_PREFIX_BYTE;
        client->need_write = 0;

        /// Delete event
        event_del(&client->event);

        client->conn_time = 0;
        */
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
    
    if (worker_id < 0)
    {
        return MSD_ERR;
    }

    /* �������д��֪ͨ! */
    res = write((*(worker_pool->thread_worker_array+ worker_id))->notify_write_fd, 
                &client_idx, sizeof(int));
    if (res == -1)
    {
        MSD_ERROR_LOG("Pipe write error, errno:%d", errno);
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

    //�б�Ҫ����?
    MSD_LOCK_LOCK(master->thread_woker_list_lock);
    for (i = 0; i < pool->thread_worker_num; i++)
    {
        idx = (i + master->cur_thread + 1) % pool->thread_worker_num;
        if (*(pool->thread_worker_array + idx) && (*(pool->thread_worker_array + idx))->tid)
        {
            master->cur_thread = idx;
            break;
        }
    }
    MSD_LOCK_UNLOCK(master->thread_woker_list_lock);

    if (i == pool->thread_worker_num)
    {
        MSD_ERROR_LOG("Thread pool full, Can not find a free worker!");
        idx = -1;
    }
    return idx;
}


/**
 * ����: �ر�client
 **/
static void msd_close_client(int client_idx) 
{
    msd_conn_client_t **pclient;
    msd_conn_client_t  *client;
    msd_conn_client_t  *null = NULL;

    pclient = (msd_conn_client_t **)msd_vector_get_at(g_ins->master->client_vec,client_idx);
    client  = *pclient;
    /*
    //TODO
    if (dll.handle_close) 
    {
        dll.handle_close(cli->remote_ip, cli->remote_port);
    }
    msd_ae_delete_file_event(ael, cli->fd, QBH_AE_READABLE);
    msd_ae_delete_file_event(ael, cli->fd, QBH_AE_WRITABLE);
    */
    
    /* qbh_vector_set_at�ĵ�����������data��ָ�룬���ڴ���data�����client_conn��ָ�룬
     * ���Ե���������Ӧ���Ǹ�����ָ�� */
    msd_vector_set_at(g_ins->master->client_vec, client->idx, (void *)&null);
    close(client->fd);
    client->fd = -1;

}

