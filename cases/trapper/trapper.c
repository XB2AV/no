/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  trapper.c
 * 
 * Description :  trapper
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/

#include "trapper.h"

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

    /* ��ʼ��libcurl 
     * ����CURL_GLOBAL_ALL ��ʹlibcurl��ʼ�����е���ģ���һЩĬ�ϵ�ѡ�ͨ������һ���ȽϺõ�Ĭ�ϲ���ֵ��
     */
    curl_global_init(CURL_GLOBAL_ALL);
    return MSD_OK;
}

/*
static int _hash_print_entry(const msd_hash_entry_t *he, void *userptr)
{
    MSD_INFO_LOG("%s => %s\n", (char *)he->key, (char *)he->val);
    return MSD_OK;
}

static void hash_dump(msd_hash_t *ht)
{
    MSD_INFO_LOG("ht->slots:%d\n", ht->slots);
    MSD_INFO_LOG("ht->count:%d\n", ht->count);
    msd_hash_foreach(ht, _hash_print_entry, NULL);
}
*/

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
    msd_dlist_t *dlist;
    trap_worker_data_t *worker_data;
    msd_conf_block_t *block;
    
    if(!(worker->priv_data = calloc(1, sizeof(trap_worker_data_t))))
    {
        MSD_ERROR_LOG("Calloc priv_data failed!");
        return MSD_ERR;
    }
    worker_data = worker->priv_data;
    worker_data->back_end_alive_interval = msd_conf_get_int_value(conf,"back_end_alive_interval",60);
    worker_data->host_game_interval = msd_conf_get_int_value(conf,"host_game_interval", 900);
    worker_data->cal_send_back_interval = msd_conf_get_int_value(conf,"cal_send_back_interval", 180);
    worker_data->worker = worker;

    /* ��������˷��͵ĵ�ַ�б��ʼ�� */
    if(!(dlist = msd_dlist_init()))
    {
        MSD_ERROR_LOG("Msd_dlist_init failed!");
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

    /* hostname=>gameӳ���ϵ��ʼ�� */
    if(!(worker_data->host_game_map_intfs = msd_str_new(msd_conf_get_str_value(conf,"host_game_intfs",NULL))))
    {
        MSD_ERROR_LOG("Get Host_game_map_intfs Failed!");
        return MSD_ERR;
    }
    
    if(!(worker_data->host_game_hash = msd_hash_create(32)))
    {
        MSD_ERROR_LOG("Msd_hash_create Failed!");
        return MSD_ERR; 
    }
    MSD_HASH_SET_SET_KEY(worker_data->host_game_hash,  msd_hash_def_set);
    MSD_HASH_SET_SET_VAL(worker_data->host_game_hash,  msd_hash_def_set);
    MSD_HASH_SET_FREE_KEY(worker_data->host_game_hash, msd_hash_def_free);
    MSD_HASH_SET_FREE_VAL(worker_data->host_game_hash, msd_hash_def_free);
    MSD_HASH_SET_KEYCMP(worker_data->host_game_hash,   msd_hash_def_cmp);
    
    if(MSD_OK != update_host_game_map(worker_data->host_game_map_intfs->buf, worker_data->host_game_hash))
    {
        MSD_ERROR_LOG("Update_host_game_map Failed!");
        return MSD_ERR;        
    }

    /* ��Ҫ�����item_id=>��Ⱥ�����idӳ���ʼ�� */
    if(!(worker_data->item_to_clu_item_hash = msd_hash_create(16)))
    {
        MSD_ERROR_LOG("Msd_hash_create Failed!");
        return MSD_ERR; 
    }
    MSD_HASH_SET_SET_KEY(worker_data->item_to_clu_item_hash,  msd_hash_def_set);
    MSD_HASH_SET_SET_VAL(worker_data->item_to_clu_item_hash,  _hash_val_set_item_id);
    MSD_HASH_SET_FREE_KEY(worker_data->item_to_clu_item_hash, msd_hash_def_free);
    MSD_HASH_SET_FREE_VAL(worker_data->item_to_clu_item_hash, _hash_val_free_item_id);
    MSD_HASH_SET_KEYCMP(worker_data->item_to_clu_item_hash,   msd_hash_def_cmp);
    
    block = msd_conf_get_block(conf, "item_id_list");
    msd_hash_foreach(block->block.ht, _hash_deal_with_item_foreach, worker_data);

    /* ȫ�������ֵ�Ĵ�hash�� */
    if(!(worker_data->item_value_hash = msd_hash_create(16)))
    {
        MSD_ERROR_LOG("Msd_hash_create Failed!");
        return MSD_ERR; 
    }    
    MSD_HASH_SET_SET_KEY(worker_data->item_value_hash,  msd_hash_def_set);
    /* set����Ϊ�գ���ζ�Ž��������ⲿ������Դ */
    //MSD_HASH_SET_SET_VAL(worker_data->item_value_hash,  _hash_val_set_item_value);
    MSD_HASH_SET_FREE_KEY(worker_data->item_value_hash, msd_hash_def_free);
    MSD_HASH_SET_FREE_VAL(worker_data->item_value_hash, _hash_val_free_item_value);
    MSD_HASH_SET_KEYCMP(worker_data->item_value_hash,   msd_hash_def_cmp);    

    
    /* ע��ʱ���¼������back_end_list����Ч�� */
    if(msd_ae_create_time_event(worker->t_ael, 1000*worker_data->back_end_alive_interval, 
            check_fd_cron, worker, NULL) == MSD_ERR)
    {
        MSD_ERROR_LOG("Create time event failed");
        return MSD_ERR;
    }

    /* ע��ʱ���¼�������host=>gameӳ���ϵ */
    if(msd_ae_create_time_event(worker->t_ael, 1000*worker_data->host_game_interval, 
            update_host_game_cron, worker, NULL) == MSD_ERR)
    {
        MSD_ERROR_LOG("Create time event failed");
        return MSD_ERR; 
    }

    /* ע��ʱ���¼������ڼ���ʱ�䴰�ڣ������������˷��� */
    if(msd_ae_create_time_event(worker->t_ael, 1000*worker_data->cal_send_back_interval, 
            cal_send_back_cron, worker, NULL) == MSD_ERR)
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
    char *content_len_buf[15];
    char *err=NULL;
    
    memset(content_len_buf, 0, 15);
    
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
    msd_thread_worker_t *worker; 
    msd_dlist_node_t *value_node;
    msd_dlist_t *value_list;
    item_value_t *item_value;
    int i;
    cJSON *p_item1;
    cJSON *p_item2;
    cJSON *p_item3;
    cJSON *p_array;
    char *p_game_name;
    char *p_host_name;
    char *p_item_id;
    char *p_value;
    char *content_buff;
    
    // ������Ϣд��sendbuf 
    msd_str_cat_len(&(client->sendbuf), "ok\n", 3);
    
    worker = msd_get_worker(client->worker_id);

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
    
    content_buff = calloc(1, client->recv_prot_len+1);
    /* The functions snprintf() write at most size bytes (including the trailing null byte ('\0')) to str. */
    snprintf(content_buff, client->recv_prot_len+1, "%s", client->recvbuf->buf);
    
    MSD_INFO_LOG("The Full Packet:%s.Len:%d", content_buff, client->recv_prot_len);
    
    /* ��Ƶ�json��������Ŀǰ����ֻ��cJSON_Parse�ķ��ض�����Ҫ�ͷ� */
    cJSON *p_root = cJSON_Parse(content_buff+10);
    if(!p_root) goto json_null;
    p_item1 = cJSON_GetObjectItem(p_root, "host");
    if(!p_item1) goto json_null;
    p_host_name = p_item1->valuestring;
    p_game_name = msd_hash_get_val(((trap_worker_data_t *)worker->priv_data)->host_game_hash, p_item1->valuestring);
    if(!p_game_name) goto game_null;
    //MSD_INFO_LOG("Hostname:%s", p_item1->valuestring);
    //MSD_INFO_LOG("Game:%s", msd_hash_get_val(((trap_worker_data_t *)worker->priv_data)->host_game_hash, p_item1->valuestring));
    p_item1 = cJSON_GetObjectItem(p_root, "time");
    if(!p_item1) goto json_null;
    //MSD_INFO_LOG("Time:%s", p_item1->valuestring);   
    
    p_array = cJSON_GetObjectItem(p_root, "data");
    if(!p_array) goto json_null;
    
    int data_count = cJSON_GetArraySize(p_array);
    for(i=0; i<data_count; i++)
    {
        p_item1 = cJSON_GetArrayItem(p_array, i);
        if(!p_item1) goto json_null;

        p_item2 = cJSON_GetArrayItem(p_item1, 0);
        p_item3 = cJSON_GetArrayItem(p_item1, 1);
        if(!p_item2) goto json_null;
        if(!p_item3) goto json_null;
        
        p_item_id = p_item2->valuestring;
        p_value   = p_item3->valuestring;

        /* ����ǲ����ĵļ����(������㼯Ⱥ��صļ����)����ֱ�Ӻ���֮ */
        if(!(msd_hash_get_val(((trap_worker_data_t *)worker->priv_data)->item_to_clu_item_hash, p_item_id)))
        {
            MSD_DEBUG_LOG("Not care! Game:%s, Host:%s, Item:%s, Value:%s", 
                p_game_name, p_host_name, p_item_id, p_value);
            continue;
        }
        
        //MSD_INFO_LOG("Json item_id:%s, value:%s", p_item2->valuestring, p_item3->valuestring);
        /* ����game_name,�ҵ�item_value_hash���ж�Ӧ�Ľڵ㣬�˽ڵ���һ��dlist */
        if(!(value_list = msd_hash_get_val(
                ((trap_worker_data_t *)worker->priv_data)->item_value_hash, p_game_name)))
        {
            /* ���û���ҵ���������֮ */
            if(!(value_list = msd_dlist_init()))
            {
                MSD_ERROR_LOG("Msd_dlist_init failed!");
                return MSD_ERR;
            }
            msd_dlist_set_free(value_list, _free_node_item_value);
            msd_hash_insert(((trap_worker_data_t *)worker->priv_data)->item_value_hash
                            ,p_game_name, value_list);
        }

        /* ����host_name,�ҵ�item_value_list�����ж�Ӧ�Ľڵ� */
        if(!(value_node = item_value_list_search(value_list, p_host_name)))
        {
            /* ���û���ҵ���������֮ */
            if(!(item_value = item_value_node_init(p_host_name)))
            {
                MSD_ERROR_LOG("Item_value_init failed!");
                return MSD_ERR;
            }
            char buf[128] = {0};
            snprintf(buf, 128, "%s,%s;", p_item_id, p_value);
            msd_str_cat(&item_value->value_str, buf);
            
            msd_dlist_add_node_tail(value_list,item_value);
        }
        else
        {
            item_value = value_node->value;
            char buf[128] = {0};
            snprintf(buf, 128, "%s,%s;", p_item_id, p_value);
            msd_str_cat(&item_value->value_str, buf);
            item_value->time = time(NULL);
        }
        MSD_INFO_LOG("Handle Finished. Game:%s, Host:%s, Item:%s, Value:%s, Value_str:%s", 
                p_game_name, p_host_name, p_item_id, p_value, item_value->value_str->buf);
        
    }   
    free(content_buff);
    cJSON_Delete(p_root);
    return MSD_OK;
    
json_null:
    MSD_ERROR_LOG("Invalidate json:%s! Client:%s:%d", content_buff, client->remote_ip, client->remote_port);
    free(content_buff);
    cJSON_Delete(p_root);
    /* �Ծɷ���OK, ���ر����� */
    return MSD_OK;
    
game_null:
    MSD_ERROR_LOG("Null game_name. Json:%s! Client:%s:%d", content_buff, client->remote_ip, client->remote_port);
    free(content_buff);
    cJSON_Delete(p_root);
    /* �Ծɷ���OK, ���ر����� */
    return MSD_OK;    
}
