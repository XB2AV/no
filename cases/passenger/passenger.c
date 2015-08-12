/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  passenger.c
 * 
 * Description :  passenger
 * 
 *     Version :  1.0.0
 * 
 *      Author :  HQ 
 *
 **/

#include "msd_core.h"

#define TOTAL_BACK_END_CNT        10
#define BACK_END_LIST_MAX_LENGTH  10

typedef enum pas_back_stat{
    B_OK,
    B_BAD
}back_stat_t;

typedef struct pas_addr{
    char    *back_ip;    /* back_end ip */
    int      back_port;   /* back_end port */
}pas_addr_t;

typedef struct pas_back_end{
    int               fd;           /* ��fd */
    int               idx;          /* ��ǰfd���������е����� */
    back_stat_t       status;
    time_t            access_time;
    msd_str_t         *recvbuf;
    msd_str_t         *sendbuf;
    msd_dlist_t       *address_list; /* pas_addr_t���� */
}back_end_t; 

/* worker˽������ */
typedef struct pas_worker_data{
    msd_dlist_t         *back_end_list;           /* ����ͽ�����ݵĵ�ַ */
    int                  back_end_alive_interval; /* ��˴��״̬̽��Ƶ�� */
    msd_thread_worker_t *worker;                  /* worker��� */
}pas_worker_data_t;

static back_end_t* deal_one_back_line(msd_conf_t *conf, const char *back_end_name, msd_thread_worker_t *worker);
static int chose_one_avail_fd(back_end_t *back_end, msd_thread_worker_t *worker);
static int check_fd_cron(msd_ae_event_loop *el, long long id, void *privdate);
static void send_to_back(msd_ae_event_loop *el, int fd, void *privdata, int mask);
static void read_from_back(msd_ae_event_loop *el, int fd, void *privdata, int mask);
/**
 * ����: ��ʼ���ص�����ʼ��Back_end
 * ����: @conf
 * ˵��: 
 *       1. ��ѡ����
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_handle_init(void *conf) 
{
    MSD_INFO_LOG("Msd_handle_init is called!");
    return MSD_OK;
}

/**
 * ����: �����̳߳�ʼ���ص�
 * ����: @worker
 * ˵��: 
 *       1. ��ѡ����
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_handle_worker_init(void *conf, void *arg)
{
    MSD_INFO_LOG("Msd_handle_worker_init is called!");
    msd_thread_worker_t *worker = (msd_thread_worker_t *)arg;
    
    int i;
    char back_end_name[20] = {};
    back_end_t *back_end;
    pas_worker_data_t *worker_data;
    msd_dlist_t *dlist;
      
    if(!(worker->priv_data = calloc(1, sizeof(pas_worker_data_t))))
    {
        MSD_ERROR_LOG("Calloc priv_data failed!");
        return MSD_ERR;
    }
    worker_data = worker->priv_data;
    worker_data->back_end_alive_interval = msd_conf_get_int_value(conf,"back_end_alive_interval",60);
    worker_data->worker = worker;

    if(!(dlist = msd_dlist_init()))
    {
        MSD_ERROR_LOG("Msd_dlist_init failed!");
        free(worker_data);
        return MSD_ERR;
    }
    worker_data->back_end_list = dlist;
    
    //ѭ����ʼ��back_end_list
    for (i=0; i<TOTAL_BACK_END_CNT; i++)
    {
        memset(back_end_name, 0, 20);
        strncpy(back_end_name, "backend_list_", strlen("backend_list_"));
        sprintf(back_end_name+strlen("backend_list_"), "%d", i+1);
        if((back_end = deal_one_back_line(conf, back_end_name, worker)))
        {
            msd_dlist_add_node_tail(dlist, back_end);
        }
    }

    /* ע��һ����ʱ���¼������back_end_list����Ч�� */
    if(msd_ae_create_time_event(worker->t_ael, worker_data->back_end_alive_interval, 
            check_fd_cron, worker, NULL) == MSD_ERR)
    {
        MSD_ERROR_LOG("Create time event failed");
        return MSD_ERR; 
    }
    //return MSD_ERR;
    return MSD_OK;
}

/**
 * ����: ��̬Լ��mossad��client֮���ͨ��Э�鳤�ȣ���mossadӦ�ö�ȡ�������ݣ�����һ������
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����
 * ����:�ɹ�:Э�鳤��; ʧ��:
 **/
int msd_handle_prot_len(msd_conn_client_t *client) 
{
    /* At here, try to find the end of the http request. */
    int head_len = 10;
    int content_len;
    char content_len_buf[15] = {0};
    char *err=NULL;
    
    if(client->recvbuf->len <= head_len)
    {
        MSD_INFO_LOG("Msd_handle_prot_len return 0. Client:%s:%d. Buf:%s", client->remote_ip, client->remote_port, client->recvbuf->buf);
        return 0;
    }
    
    memcpy(content_len_buf, client->recvbuf->buf, head_len);
    content_len = strtol((const char *)content_len_buf, &err, 10);
    if(*err)
    {
        MSD_ERROR_LOG("Wrong format:%s. content_len_buf:%s. recvbuf:%s", err, content_len_buf, client->recvbuf->buf);
        MSD_ERROR_LOG("Wrong format:%d, %d. %p. content_len:%d", err[0], err[1], err, content_len);
        //return MSD_ERR;
    }
    if(head_len + content_len > client->recvbuf->len)
    {
        MSD_INFO_LOG("Msd_handle_prot_len return 0. Client:%s:%d. head_len:%d, content_len:%d, recvbuf_len:%d. buf:%s", 
            client->remote_ip, client->remote_port, head_len, content_len, client->recvbuf->len, client->recvbuf->buf);
        return 0;
    }
    return head_len+content_len;
}

/**
 * ����: ��Ҫ���û��߼�
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����
 *       2. ÿ�δ�recvbuf��Ӧ��ȡ��recv_prot_len���ȵ����ݣ���Ϊһ����������
 * ����:�ɹ�:0; ʧ��:-x
 *       MSD_OK: �ɹ������������Ӽ���
 *       MSD_END:�ɹ������ڼ�����mossad��responseд��client���Զ��ر�����
 *       MSD_ERR:ʧ�ܣ�mossad�ر�����
 **/
int msd_handle_process(msd_conn_client_t *client) 
{
    msd_thread_worker_t *worker; 
    msd_dlist_iter_t dlist_iter;
    msd_dlist_node_t *node;
    back_end_t *back_end;
    msd_dlist_t *dlist;
    
    /* ������Ϣд��sendbuf */
    msd_str_cat_len(&(client->sendbuf), "ok", 2);
    /*
    // ע���д�¼� -- ����Mossad���ʵ��
    if (msd_ae_create_file_event(worker->t_ael, client->fd, MSD_AE_WRITABLE,
                msd_write_to_client, client) == MSD_ERR) 
    {
        msd_close_client(client->idx, "create file event failed");
        MSD_ERROR_LOG("Create write file event failed for connection:%s:%d", client->remote_ip, client->remote_port);
        return MSD_ERR;
    } 
    */
    
    worker = msd_get_worker(client->worker_id);
    /* ע���д�¼� */
    dlist = ((pas_worker_data_t *)worker->priv_data)->back_end_list;
	msd_dlist_rewind(dlist, &dlist_iter);
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        back_end = node->value;  
        /* ��Ϣд��sendbuf */
        msd_str_cat_len(&(back_end->sendbuf), client->recvbuf->buf, client->recv_prot_len);
        if (back_end->status!=B_BAD 
            && back_end->fd != -1 
            && msd_ae_create_file_event(worker->t_ael, back_end->fd, MSD_AE_WRITABLE,
                send_to_back, back_end) == MSD_ERR) 
        {
            MSD_ERROR_LOG("Create write file event failed for backend_end fd:%d", back_end->fd);
            return MSD_ERR;
        }
    }

    return MSD_OK;
}

/*
 *����: �ɹ���һ����˾��;ʧ��:NULL
 */
static back_end_t* deal_one_back_line(msd_conf_t *conf, const char *back_end_name, msd_thread_worker_t *worker)
{
    int j, cnt;
    char back_end_buf[1024];
    char *back_end_n;
    unsigned char *back_arr[BACK_END_LIST_MAX_LENGTH];
    unsigned char *ip_port[BACK_END_LIST_MAX_LENGTH];
    pas_addr_t *tmp;
    back_end_t *back_end;
    
    if(!(back_end = malloc(sizeof(back_end_t))))
    {
        MSD_ERROR_LOG("Bad fomat back address list!");
        return NULL;
    }
    back_end->fd          = -1;
    back_end->idx         = -1;
    back_end->status      = B_BAD;
    back_end->recvbuf     = msd_str_new_empty(); 
    back_end->sendbuf     = msd_str_new_empty(); 
    back_end->access_time = time(NULL);
    if(!(back_end->address_list = msd_dlist_init()))
    {
        msd_str_free(back_end->recvbuf);
        msd_str_free(back_end->sendbuf);
        free(back_end);
        MSD_ERROR_LOG("msd_dlist_init failed!");
        return NULL;        
    }
    
    back_end_n = msd_conf_get_str_value(conf, back_end_name, NULL);
    if(back_end_n)
    {
        memset(back_end_buf,  0, 1024);
        strncpy(back_end_buf, back_end_n, 1024);
        cnt = msd_str_explode((unsigned char *)back_end_buf, back_arr, 
                        BACK_END_LIST_MAX_LENGTH, (const unsigned char *)";");
        if(MSD_ERR == cnt)
        {
            MSD_ERROR_LOG("Bad fomat back address list!");
            goto deal_one_err;
        }
          
        for(j=0; j<cnt; j++)
        {
            if(2 != msd_str_explode(back_arr[j], ip_port, 2, (const unsigned char *)":"))
            {
                MSD_ERROR_LOG("Error fomat back address!");
                goto deal_one_err;
            }

            if(!(tmp = malloc(sizeof(pas_addr_t))))
            {
                MSD_ERROR_LOG("Malloc failed!");
                goto deal_one_err;
            }
            
            if(!(tmp->back_ip   = strdup((const char *)ip_port[0])))
            {
                MSD_ERROR_LOG("Strdup failed!");
                goto deal_one_err;
            }
            tmp->back_port = atoi((const char *)ip_port[1]);
            if(!msd_dlist_add_node_tail(back_end->address_list, tmp))
            {
                MSD_ERROR_LOG("Msd_dlist_add_node_tail failed!");
                goto deal_one_err;
            }
            
        }
        
        if(MSD_OK != chose_one_avail_fd(back_end, worker))
        {
            MSD_ERROR_LOG("Chose_one_avail_fd failed!");
            //goto deal_one_err;
        }
    }
    else
    {
        goto deal_one_err;
    }

    return back_end;
    
deal_one_err:
    msd_str_free(back_end->recvbuf);
    msd_str_free(back_end->sendbuf);
    msd_dlist_destroy(back_end->address_list);
    free(back_end);
    return NULL;
}

/*
 * �жϷ�����Connect�ɹ���һ�㲽�裺
 *
 * 1.���򿪵�socket��Ϊ��������,������fcntl(socket, F_SETFL, O_NDELAY)���(�е�ϵͳ��FNEDLAYҲ��).
 * 2.��connect����,��ʱ����-1,����errno����ΪEINPROGRESS,�⼴connect�Ծ��ڽ��л�û�����.
 * 3.���򿪵�socket��������ӵĿ�д(ע�ⲻ�ǿɶ�)�ļ�������select���м���,�����д,
 *   ��getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, sizeof(int));���õ�error��ֵ,���Ϊ��,��connect�ɹ�.
 *
 * added by huaqi 2015-8-12 ������һ�����ͣ�����MSG_PEEK��εı�Ҫ�ԣ�
 *   ��Ϊע����read_from_back���¼������Ըо�cose_one_avial_fd�е�MSG_PEEK��һ��ûʲô̫���Ҫ��
 *         
 */
static int chose_one_avail_fd(back_end_t *back_end, msd_thread_worker_t *worker)
{
    assert(back_end);
    
    char err_buf[1024];
    msd_dlist_iter_t   dlist_iter;
    msd_dlist_node_t  *node;
    pas_addr_t        *addr;
    int address_len = back_end->address_list->len;
    int fd_arr[address_len];
    int i,n;
    fd_set wset;  
    struct timeval tval;  
    int error,code;
    socklen_t len;
    int find_flag = 0;
    int ret;
    char buf[2];
    
    for(i=0; i<address_len; i++)
    {
        fd_arr[i] = -1;
    }
    
    if(back_end->fd == -1 || back_end->status == B_BAD)
    {
again:    
        msd_dlist_rewind(back_end->address_list, &dlist_iter);
        i = 0;
        FD_ZERO(&wset);  
        tval.tv_sec  = 0;  
        tval.tv_usec = 50000; //50����
        while ((node = msd_dlist_next(&dlist_iter))) 
        {
            addr = node->value;
            memset(err_buf, 0, 1024);
            fd_arr[i] = msd_anet_tcp_nonblock_connect(err_buf, addr->back_ip, addr->back_port);
            FD_SET(fd_arr[i], &wset);           
            //printf("%s    %d      fd:%d\n", addr->back_ip, addr->back_port, fd_arr[i]);
            i++;
        }
        
        if ((n = select(fd_arr[i-1]+1, NULL, &wset, NULL, &tval)) == 0) 
        {  
            for(i=0; i<address_len; i++)
            {
                close(fd_arr[i]);
            }
            MSD_ERROR_LOG("Select time out, No fd ok!");
            return MSD_ERR; 
        }

        for(i=0; i<address_len; i++)
        {
            error = 0;
            if (FD_ISSET(fd_arr[i], &wset)) 
            {  
                len = sizeof(error);  
                code = getsockopt(fd_arr[i], SOL_SOCKET, SO_ERROR, &error, &len);  
                /* �����������Solarisʵ�ֵ�getsockopt����-1����pending error���ø�errno. Berkeleyʵ�ֵ� 
                 * getsockopt����0, pending error���ظ�error. ��Ҫ������������� */  
                if (code < 0 || error) 
                {  
                    MSD_WARNING_LOG("Fd:%d not ok!", fd_arr[i]);
                    if (error)   
                        errno = error; 
                    continue;
                }
                else
                {
                    /* error==0, ���ֿ������� */
                    find_flag        = 1;
                    back_end->idx    = i;
                    back_end->fd     = fd_arr[i];
                    back_end->status = B_OK;
                    node = msd_dlist_index(back_end->address_list, i);
                    addr = node->value; 
                    MSD_INFO_LOG("Find the ok fd:%d, IP:%s, Port:%d",fd_arr[i], addr->back_ip, addr->back_port);
                    //printf("Find the ok fd:%d\n",fd_arr[i]);
                    break;
                }
            }       
        }
        
        if(!find_flag)
        {
            MSD_ERROR_LOG("No fd ok!");
            for(i=0; i<address_len; i++)
            {
                back_end->idx = -1;
                close(fd_arr[i]);
            }
            return MSD_ERR;             
        }
        else
        {
            //�ر�û��ѡ�е�ȫ��fd
            for(i=0; i<address_len; i++)
            {
                if(back_end->fd != fd_arr[i]) 
                {
                    MSD_DEBUG_LOG("Close the not chosen fd:%d", fd_arr[i]);
                    close(fd_arr[i]);
                }
            }    
            
            //ע���ȡ����
            if (msd_ae_create_file_event(worker->t_ael, back_end->fd, MSD_AE_READABLE,
                            read_from_back, worker) == MSD_ERR) 
            {
                MSD_ERROR_LOG("Create read file event failed for connection:%s:%d",
                        addr->back_ip, addr->back_port);
                return MSD_ERR;
            }
        }
    }
    else
    {
        /*
         * ��UNIX/LINUX�£�������ģʽSOCKET���Բ���recv+MSG_PEEK�ķ�ʽ�����жϣ�����MSG_PEEK��֤�˽�������״̬�жϣ�����Ӱ�����ݽ���
         * ���������رյ�SOCKET, recv����-1������errno����Ϊ9��#define EBADF   9  // Bad file number ��
         * ��104 ��#define ECONNRESET 104 // Connection reset by peer ��
         * ���ڱ����رյ�SOCKET,recv����0������errno����Ϊ11��#define EWOULDBLOCK EAGAIN // Operation would block ��
         * ��������SOCKET, ����н������ݣ��򷵻�>0, ���򷵻�-1������errno����Ϊ11��#define EWOULDBLOCK EAGAIN // Operation would block ��
         * ��˶��ڼ򵥵�״̬�жϣ������࿼���쳣�������
         * recv����>0��   ����
         * ����-1������errno����Ϊ11  ����
         * �������    �ر�
         */
        //̽������fd������
        memset(buf, 0, 2);
        ret = recv(back_end->fd, buf, 1, MSG_PEEK);
        if(ret == 0)
        {
            //�Է��ر����ӣ�������ѡ����
            node = msd_dlist_index(back_end->address_list, back_end->idx);
            addr = node->value; 
            MSD_WARNING_LOG("Peer close. Current fd:%d not ok, IP:%s, Port:%d, chose again!", back_end->fd, addr->back_ip, addr->back_port);
            msd_ae_delete_file_event(worker->t_ael, back_end->fd, MSD_AE_WRITABLE | MSD_AE_READABLE);
            close(back_end->fd);
            back_end->idx    = -1;
            back_end->fd     = -1;
            back_end->status = B_BAD;
            goto again;
        }
        else if(ret < 0)
        {
            if (errno == EAGAIN || errno==EINTR)
            {
                MSD_DEBUG_LOG("Normal:%s. Current fd:%d ok!",strerror(errno), back_end->fd);
            }
            else
            {
                //δ֪����������ѡ����
                node = msd_dlist_index(back_end->address_list, back_end->idx);
                addr = node->value; 
                MSD_WARNING_LOG("Unkonw error:%s. Current fd:%d not ok, Ip%s, Port:%d, chose again!",strerror(errno), back_end->fd, addr->back_ip, addr->back_port);
                msd_ae_delete_file_event(worker->t_ael, back_end->fd, MSD_AE_WRITABLE | MSD_AE_READABLE);
                close(back_end->fd);
                back_end->idx    = -1;
                back_end->fd     = -1;
                back_end->status = B_BAD;
                goto again;
            }
        }
        
        //OK! do nothing.
    }
    return MSD_OK;
}

static int check_fd_cron(msd_ae_event_loop *el, long long id, void *privdate) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)privdate;
    msd_dlist_iter_t dlist_iter;
    msd_dlist_node_t *node;
    back_end_t *back_end;
    msd_dlist_t *dlist = ((pas_worker_data_t *)worker->priv_data)->back_end_list;
    
    MSD_DEBUG_LOG("Worker[%d] check fd cron begin!", worker->idx);
   
    msd_dlist_rewind(dlist, &dlist_iter);
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        back_end = node->value;  
        msd_dlist_node_t *t_node = msd_dlist_index(back_end->address_list, back_end->idx);
        pas_addr_t       *addr = t_node->value; 
        
        MSD_DEBUG_LOG("Current fd:%d. IP:%s, Port:%d", back_end->fd, addr->back_ip, addr->back_port);
        if(MSD_OK != chose_one_avail_fd(back_end, worker))
        {
            MSD_DEBUG_LOG("Chose_one_avail_fd failed!");
        }
    }

    return 60*1000;
}

static void send_to_back(msd_ae_event_loop *el, int fd, void *privdata, int mask) 
{
    back_end_t *back_end = (back_end_t *)privdata;
    int nwrite;
    MSD_AE_NOTUSED(mask);

    msd_dlist_node_t *node = msd_dlist_index(back_end->address_list, back_end->idx);
    pas_addr_t       *addr = node->value; 
                
    nwrite = write(fd, back_end->sendbuf->buf, back_end->sendbuf->len);
    back_end->access_time = time(NULL);
    if (nwrite < 0) 
    {
        if (errno == EAGAIN) /* ������fdд�룬�����ݲ����� */
        {
            MSD_WARNING_LOG("Write to back temporarily unavailable! fd:%d. IP:%s, Port:%d", back_end->fd, addr->back_ip, addr->back_port);
            nwrite = 0;
        } 
        else if(errno==EINTR)/* �����ж� */
        {  
            MSD_WARNING_LOG("Write to back was interupted! fd:%d. IP:%s, Port:%d", back_end->fd, addr->back_ip, addr->back_port);
            nwrite = 0; 
        } 
        else 
        {
            MSD_ERROR_LOG("Write to back failed! fd:%d. IP:%s, Port:%d. Error:%s", back_end->fd, addr->back_ip, addr->back_port, strerror(errno));
            return;
        }
    }
    MSD_INFO_LOG("Write to back! fd:%d. IP:%s, Port:%d. content:%s", back_end->fd, addr->back_ip, addr->back_port, back_end->sendbuf->buf);

    /* ���Ѿ�д������ݣ���sendbuf�вü��� */
    if(MSD_OK != msd_str_range(back_end->sendbuf, nwrite, back_end->sendbuf->len-1))
    {
        /* ����Ѿ�д����,��write_len == back_end->sendbuf->len����msd_str_range����NONEED */
        msd_str_clear(back_end->sendbuf);
    }

     /* ˮƽ���������sendbuf�Ѿ�Ϊ�գ���ɾ��д�¼�������᲻�ϴ���
       * ע��д�¼���so�е�handle_processȥ��� */
    if(back_end->sendbuf->len == 0)
    {
        msd_ae_delete_file_event(el, back_end->fd, MSD_AE_WRITABLE);
    }
    return;
}

static void read_from_back(msd_ae_event_loop *el, int fd, void *privdata, int mask) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)privdata;
    back_end_t *back_end;
    pas_addr_t     *addr;
    int nread;
    char buf[1024];
    MSD_AE_NOTUSED(el);
    MSD_AE_NOTUSED(mask);

    msd_dlist_t *dlist = ((pas_worker_data_t *)worker->priv_data)->back_end_list;
    msd_dlist_node_t *node;
    msd_dlist_iter_t dlist_iter;
    msd_dlist_rewind(dlist, &dlist_iter);
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        back_end = node->value;  
        if(back_end->fd == fd)
            break;
    }

    node = msd_dlist_index(back_end->address_list, back_end->idx);
    addr = node->value;     
    
    nread = read(fd, buf, 1024 - 1);
    back_end->access_time = time(NULL);
    if (nread == -1) 
    {
        /* ��������fd����ȡ������������������ݿɶ�����errno==EAGAIN */
        if (errno == EAGAIN) 
        {
            MSD_WARNING_LOG("Read back [%s:%d] eagain: %s",
                     addr->back_ip, addr->back_port, strerror(errno));
            nread = 0;
            return;
        }
        else if (errno == EINTR) 
        {
            MSD_WARNING_LOG("Read back [%s:%d] interrupt: %s",
                     addr->back_ip, addr->back_port, strerror(errno));
            nread = 0;
            return;
        } 
        else 
        {
            MSD_ERROR_LOG("Read back [%s:%d] failed: %s",
                     addr->back_ip, addr->back_port, strerror(errno));

            if(MSD_OK != chose_one_avail_fd(back_end, worker))
            {
                close(back_end->fd);
                MSD_DEBUG_LOG("Chose_one_avail_fd failed!");
            }
            return;
        }
    } 
    else if (nread == 0) 
    {
        MSD_WARNING_LOG("Back_end close.FD:%d, IP:%s, Port:%d",back_end->fd, addr->back_ip, addr->back_port);
        msd_ae_delete_file_event(worker->t_ael, back_end->fd, MSD_AE_WRITABLE | MSD_AE_READABLE);
        close(back_end->fd);
        back_end->idx    = -1;
        back_end->fd     = -1;
        back_end->status = B_BAD;
        if(MSD_OK != chose_one_avail_fd(back_end, worker))
        {
            close(back_end->fd);
            MSD_DEBUG_LOG("Chose_one_avail_fd failed!");
        }        
        return;
    }
    buf[nread] = '\0';

    //printf("%d %d  %d  %d\n", buf[nread-3], buf[nread-2], buf[nread-1], buf[nread]);
    MSD_INFO_LOG("Read from back.Fd:%d, IP:%s, Port:%d. Length:%d, Content:%s", 
                    back_end->fd, addr->back_ip, addr->back_port, nread, buf);
    return;
}
