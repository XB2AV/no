/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  redis_saver.c
 * 
 * Description :  ��������洢��Redis
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/

#include "redis_saver.h"

static int redis_connect(void *worker_data, redisContext **c);
static int redis_save(redisContext *c, const char* hostname, const char* item_id, const char* value);
static int redis_destroy(redisContext *c);


/**
 * ����: ����redis
 * ����: @data:woker˽������
 * ����:�ɹ�:0, ʧ��:-x
 **/
int redis_connect(void *data, redisContext **c)
{
    int port;
    int db;
    redisReply* r=NULL;
    char cmd[100] = {0};
    
    saver_worker_data_t *worker_data = (saver_worker_data_t *)data;
    port = atoi(worker_data->redis_port->buf);
    db   = atoi(worker_data->redis_db->buf);
    
    //����Redis��������ͬʱ��ȡ��Redis���ӵ������Ķ���    
    //�ö����������������Redis�����ĺ�����    
    *c = redisConnect(worker_data->redis_ip->buf, port);    
    if (c->err) {
        MSD_ERROR_LOG("Connect error: %s", c->errstr);         
        //redisFree(c);        
        return MSD_FAILED;    
    }

    //ѡ���
    snprintf(cmd, 100, "select %d", db);    
    r = (redisReply*)redisCommand(c, cmd);    
    if (r == NULL) {        
        MSD_ERROR_LOG("Select error: %s", c->errstr); 
        //redisFree(c);        
        return;    
    }    

    //���ں����ظ�ʹ�øñ�����������Ҫ��ǰ�ͷţ������ڴ�й©��    
    freeReplyObject(r);

    return MSD_OK;
}

/**
 * ����: redis�Ĵ洢����
 * ����: @conf
 * ˵��: 
 *       1. ��ѡ����
 * ����:�ɹ�:0; ʧ��:-x
 **/
int redis_save(redisContext *c, const char* hostname, const char* item_id, const char* value)
{
    redisReply* r=NULL;
    char cmd[4096] = {0};

    snprint(cmd, 4096, "hset %s %s %s", hostname, item_id, value);    
    r = (redisReply*)redisCommand(c,cmd);        
    if (NULL == r) {        
        MSD_ERROR_LOG("Hset Error: %s\n", c->errstr);        
        //redisFree(c);        
        return MSD_FAILED;    
    }    

    if (!(strcasecmp(r->str,"OK") == 0)) {        
        MSD_ERROR_LOG("Error: %s\n", c->errstr);        
        MSD_ERROR_LOG("Failed to execute command[%s]",cmd);        
        freeReplyObject(r);        
        //redisFree(c);        
        return MSD_FAILED;    
    }


    return MSD_OK;
}

int redis_destroy(redisContext *c)
{
    redisFree(c); 
    return MSD_OK;
}


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

    saver_worker_data_t *worker_data;
    
    if(!(worker->priv_data = calloc(1, sizeof(saver_worker_data_t))))
    {
        MSD_ERROR_LOG("Calloc priv_data failed!");
        return MSD_ERR;
    }
    worker_data = worker->priv_data;
    if(!(worker_data->redis_ip= msd_str_new(msd_conf_get_str_value(conf,"redis_ip",NULL))))
    {
        MSD_ERROR_LOG("Get redis_ip Failed!");
        return MSD_ERR;
    }
    if(!(worker_data->redis_port= msd_str_new(msd_conf_get_str_value(conf,"redis_port",NULL))))
    {
        MSD_ERROR_LOG("Get redis_port Failed!");
        return MSD_ERR;
    }
    if(!(worker_data->redis_db= msd_str_new(msd_conf_get_str_value(conf,"redis_db",NULL))))
    {
        MSD_ERROR_LOG("Get redis_db Failed!");
        return MSD_ERR;
    }
    
    worker_data->worker = worker;
 
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
        return MSD_ERR;
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
    // ������Ϣд��sendbuf 
    msd_str_cat_len(&(client->sendbuf), "ok\n", 3);

    msd_thread_worker_t *worker; 
    int i;
    cJSON *p_item1;
    cJSON *p_item2;
    cJSON *p_item3;
    cJSON *p_array;

    char *p_hostname;
    char *p_time;
    char *p_item_id;
    char *p_value;
    char *content_buff;

    redisContext *c;

    worker = msd_get_worker(client->worker_id);
    redis_connect(worker->priv_data, &c);
    
    content_buff = calloc(1, client->recv_prot_len+1);
    // The functions snprintf() write at most size bytes (including the trailing null byte ('\0')) to str./
    snprintf(content_buff, client->recv_prot_len+1, "%s", client->recvbuf->buf);
    
    //MSD_INFO_LOG("The Full Packet:%s.Len:%d", content_buff, client->recv_prot_len);
    
    // ��Ƶ�json��������Ŀǰ����ֻ��cJSON_Parse�ķ��ض�����Ҫ�ͷ� //
    cJSON *p_root = cJSON_Parse(content_buff+10);
    if(!p_root) 
        goto json_null;
    
    p_item1 = cJSON_GetObjectItem(p_root, "host");
    if(!p_item1) 
        goto json_null;
    p_hostname = p_item1->valuestring;
    //MSD_INFO_LOG("Hostname:%s", p_item1->valuestring);
 
    p_item1 = cJSON_GetObjectItem(p_root, "time");
    if(!p_item1) 
        goto json_null;
    p_time = p_item1->valuestring;
    //MSD_INFO_LOG("time:%s\n", p_item1->valuestring);
    
    p_array = cJSON_GetObjectItem(p_root, "data");
    if(!p_array) 
        goto json_null;
    
    int data_count = cJSON_GetArraySize(p_array);
    for(i=0; i<data_count; i++)
    {
        p_item1 = cJSON_GetArrayItem(p_array, i);
        if(!p_item1) goto json_null;

        p_item2 = cJSON_GetArrayItem(p_item1, 0);
        p_item3 = cJSON_GetArrayItem(p_item1, 1);
        if(!p_item2) 
            goto json_null;
        if(!p_item3) 
            goto json_null;
        
        p_item_id = p_item2->valuestring;
        p_value   = p_item3->valuestring;

        //MSD_INFO_LOG("item_id:%s\n", p_item_id);
        //MSD_INFO_LOG("value:%s\n", p_value);

        //�洢
        if(MSD_OK != redis_save(c, p_hostname, p_item_id, p_time, p_value)){
            goto json_null;
        }

        
    }   
    free(content_buff);
    redis_destroy(c);
    cJSON_Delete(p_root);
    return MSD_OK;
    
json_null:
    MSD_ERROR_LOG("Invalidate json:%s! Client:%s:%d", content_buff, client->remote_ip, client->remote_port);
    free(content_buff);
    redis_destroy(c);
    cJSON_Delete(p_root);
    // �Ծɷ���OK, ���ر�����
    return MSD_OK; 
}
