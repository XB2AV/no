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

int check_fd_cron(msd_ae_event_loop *el, long long id, void *privdate) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)privdate;
    msd_dlist_iter_t dlist_iter;
    msd_dlist_node_t *node;
    back_end_t *back_end;
    trap_worker_data_t *worker_data = worker->priv_data;
    msd_dlist_t *dlist = worker_data->back_end_list;
    
    MSD_DEBUG_LOG("Worker[%d] check fd cron begin!", worker->idx);
   
    msd_dlist_rewind(dlist, &dlist_iter);
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        back_end = node->value; 
        msd_dlist_node_t *t_node = msd_dlist_index(back_end->address_list, back_end->idx);
        tos_addr_t       *addr = t_node->value; 
        
        MSD_DEBUG_LOG("Current fd:%d. IP:%s, Port:%d", back_end->fd, addr->back_ip, addr->back_port);
        if(MSD_OK != chose_one_avail_fd(back_end, worker))
        {
            MSD_DEBUG_LOG("Chose_one_avail_fd failed!");
        }
    }
    
    return 1000*worker_data->back_end_alive_interval;
}

/* ���ڸ���hostname=>game��ӳ���ϵ */
int update_host_game_cron(msd_ae_event_loop *el, long long id, void *privdate) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)privdate;
    trap_worker_data_t *worker_data = worker->priv_data;

    //TODO �������и����⣬����ɾ���Ļ�����hostname���޷����ֲ�ɾ������hashע����Խ��Խ��
    //     ���ǲ�Ӱ�����ݵ���ȷ�ԣ��ݲ�������Ϊ��ͬ��key���滻ԭ�е�val������ȷ�ͷ�
    update_host_game_map(worker_data->host_game_map_intfs->buf, 
            worker_data->host_game_hash); 

    /*
    hash_dump(worker_data->host_game_hash);
    hash_dump2(worker_data->item_to_clu_item_hash);
    */
    return 1000*worker_data->host_game_interval;
}

/* 
 * ���temp_hash���foreach���㺯��
 * ��ǰitem_id����������Ӧ����host���ڵ�max,min,avg,sum 
 */
static int calculate_hash(const msd_hash_entry_t *he, void *userptr)
{
    cal_struct_t *cal = userptr;
    trap_worker_data_t *worker_data = cal->worker_data;
    char *item_id = he->key;
    msd_dlist_t *dlist = he->val;
    msd_dlist_iter_t dlist_iter;
    msd_dlist_t *cluster_item_list;
    msd_dlist_node_t *node;
    char *clu_item_id;
    float sum = 0;
    float max = FLT_MIN;
    float min = FLT_MAX;
    float avg = 0;
    int cnt = 0;
    char buf[128];
    back_end_t *back_end;
    cJSON *p_root;
    cJSON *p_data_arr;
    cJSON *p_data;
    cJSON *p_value;
    char  *json;
    char result[256] = {0};
    
    MSD_INFO_LOG("Item_id:%s",item_id);
    msd_dlist_rewind(dlist, &dlist_iter);
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        host_value_t *host_value = node->value;
        MSD_INFO_LOG("host:%s, value:%f",host_value->hostname, host_value->value);
        if(host_value->value > max)
        {
            max = host_value->value;
        }

        if(host_value->value < min)
        {
            min = host_value->value;
        }
        sum += host_value->value;
        cnt ++;
    }
    avg = sum/cnt;

    cnt = 0;
    cluster_item_list = msd_hash_get_val(worker_data->item_to_clu_item_hash, item_id);
    MSD_INFO_LOG("Cluster Name: cluster_%s", cal->game);


    /* ƴװJSON������ */
    //0000000106{"type": "0", "host": "huaqi_3", "data": [["10001", "1", "1"], ["10002", "2", "1"]], "time": "1402972148"}
    p_root = cJSON_CreateObject();

    cJSON_AddStringToObject(p_root,"type", "0");
    memset(buf, 0, 128);
    snprintf(buf, 128, "cluster_%s", (char *)cal->game);
    cJSON_AddStringToObject(p_root,"host", buf);
    memset(buf, 0, 128);
    snprintf(buf, 128, "%d", (int)time(NULL));
    cJSON_AddStringToObject(p_root,"time", buf);
    
    msd_dlist_rewind(cluster_item_list, &dlist_iter);
    p_data_arr = cJSON_CreateArray();
    
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        clu_item_id = node->value;
        p_data = cJSON_CreateArray();
        switch (cnt)
        {
            case 0:
                memset(buf, 0, 128); snprintf(buf, 128, "%s", clu_item_id);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);

                memset(buf, 0, 128); snprintf(buf, 128, "%.2f", max);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);                

                p_value = cJSON_CreateString("1");
                cJSON_AddItemToArray(p_data, p_value); 

                MSD_INFO_LOG("MAX(item_id:%s):%f",clu_item_id, max);
                break;
            case 1:
                memset(buf, 0, 128); snprintf(buf, 128, "%s", clu_item_id);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);

                memset(buf, 0, 128); snprintf(buf, 128, "%.2f", min);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);                

                p_value = cJSON_CreateString("1");
                cJSON_AddItemToArray(p_data, p_value); 

                MSD_INFO_LOG("MIN(item_id:%s):%f",clu_item_id, min);
                break;
            case 2:
                memset(buf, 0, 128); snprintf(buf, 128, "%s", clu_item_id);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);

                memset(buf, 0, 128); snprintf(buf, 128, "%.2f", avg);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);                

                p_value = cJSON_CreateString("1");
                cJSON_AddItemToArray(p_data, p_value); 

                MSD_INFO_LOG("AVG(item_id:%s):%f",clu_item_id, avg);
                break;
            case 3:
                memset(buf, 0, 128); snprintf(buf, 128, "%s", clu_item_id);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);

                memset(buf, 0, 128); snprintf(buf, 128, "%.2f", sum);
                p_value = cJSON_CreateString(buf);
                cJSON_AddItemToArray(p_data, p_value);                

                p_value = cJSON_CreateString("1");
                cJSON_AddItemToArray(p_data, p_value); 
                
                MSD_INFO_LOG("SUM(item_id:%s):%f",clu_item_id, sum);
                break;                
                
        }
        cnt++;
        cJSON_AddItemToArray(p_data_arr, p_data); 
    }
    cJSON_AddItemToObject(p_root, "data", p_data_arr);
    json = cJSON_PrintUnformatted(p_root);

    snprintf(result, 256, "%010d%s", (int)strlen(json), json);
    MSD_INFO_LOG("JSON_RESULT:%s", result);
    
   /* ע������д�¼����������������� */
    dlist = worker_data->back_end_list;
	msd_dlist_rewind(dlist, &dlist_iter);
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        back_end = node->value;  
        // ��Ϣд��sendbuf 
        msd_str_cat_len(&(back_end->sendbuf), result, strlen(result));

        if (back_end->status!=B_BAD 
            && back_end->fd != -1 
            && msd_ae_create_file_event(worker_data->worker->t_ael, back_end->fd, MSD_AE_WRITABLE,
                send_to_back, back_end) == MSD_ERR) 
        {
            MSD_ERROR_LOG("Create write file event failed for backend_end fd:%d", back_end->fd);
            return MSD_ERR;
        }
    }

    free(json);
    /* �ݹ�ɾ�����м����赥��ɾ�������򷴶�����ֶ����ͷ� */
    cJSON_Delete(p_root);
    
    return MSD_OK;
}

/*
 * ����item_value_hash����һ����Ŀ(һ��ҵ��)�����㵥��ҵ������м�Ⱥ������ֵ
 */
static int handle_game_data(const msd_hash_entry_t *he, void *userptr)
{
    MSD_INFO_LOG("================= Game : %s Begin handle ===================", (char *)he->key);
    trap_worker_data_t *worker_data = userptr;
    msd_dlist_t *dlist = he->val;
    msd_dlist_iter_t dlist_iter;
    msd_dlist_node_t *node;
    item_value_t *item_value;
    msd_dlist_t *host_value_list;
    int cnt, i;
    cal_struct_t cal;

    /* ����һ����ʱhash��ר�Ŵ洢����֮�������
      * key��item_to_clu_item_hash��ͬ��һ��item_id
      * value��item_value_hash��ͬ��һ������ÿ��node�������µ�ֵ
      * ������list���ɻ�ø�item_id��Ӧ��max,min,avg,sum
      */
    msd_hash_t *temp_hash;
    if(!(temp_hash = msd_hash_create(16)))
    {
        MSD_ERROR_LOG("Msd_hash_create Failed!");
        return MSD_ERR; 
    }
    MSD_HASH_SET_SET_KEY(temp_hash,  msd_hash_def_set);
    /* û��set��������ʾ��Ҫ���ⲿ��������value */
    //MSD_HASH_SET_SET_VAL(temp_hash,  msd_hash_def_set);
    MSD_HASH_SET_FREE_KEY(temp_hash, msd_hash_def_free);
    MSD_HASH_SET_FREE_VAL(temp_hash, _hash_val_free_host_value);
    MSD_HASH_SET_KEYCMP(temp_hash,   msd_hash_def_cmp);

    /* ����item_value_hash��һ��value(list),���쵱ǰҵ�����ʱhash */
    msd_dlist_rewind(dlist, &dlist_iter);
    while ((node = msd_dlist_next(&dlist_iter))) 
    {
        item_value = node->value;
        MSD_INFO_LOG("Host:%s, Value_str:%s, Time:%d", item_value->hostname, item_value->value_str->buf, item_value->time);
        unsigned char buf[2048] = {0};
        /* ������20����Ҫ���㼯Ⱥ��صļ���ʱ�䴰����3���������20*3���ָ� */
        unsigned char *i_v[60];
        strncpy((char *)buf, (const char *)item_value->value_str->buf, (size_t)2048);
        /* �и�value_str������item_id���й���ȥ�� */
        cnt = msd_str_explode(buf, i_v, 60, (const unsigned char *)";");
        for(i=0; i<cnt; i++)
        {
            unsigned char *id_val[2];
            if(2 != msd_str_explode(i_v[i], id_val, 2, (const unsigned char *)","))
            {
                MSD_ERROR_LOG("Msd_str_explode failed:%s", i_v[i]);
            }
            if(!(host_value_list = msd_hash_get_val(temp_hash, id_val[0])))
            {
                /* ���û���ҵ���������֮ */
                if(!(host_value_list = msd_dlist_init()))
                {
                    MSD_ERROR_LOG("Msd_dlist_init failed!");
                    return MSD_ERR;
                }
                msd_dlist_set_free(host_value_list, _free_node_host_value);
                msd_hash_insert(temp_hash, id_val[0], host_value_list);
            }
            
            /* ����host_name,�ҵ�item_value_list�����ж�Ӧ�Ľڵ� */
            msd_dlist_node_t *value_node;
            if(!(value_node = host_value_list_search(host_value_list, item_value->hostname)))
            {
                host_value_t *host_value = calloc(1, sizeof(host_value_t));
                strncpy(host_value->hostname, item_value->hostname, 128);
                host_value->value = atof((const char *)id_val[1]);
                msd_dlist_add_node_tail(host_value_list, host_value);
            }
            else
            {
                host_value_t *host_value = value_node->value;
                strncpy(host_value->hostname, item_value->hostname, 128);
                host_value->value = atof((const char *)id_val[1]);
            }
        }
    }

    /* ������ʱ��ϣ���ڵ�ǰҵ�����棬���item_id����ͳ�ƻ��� */
    cal.game = he->key;
    cal.worker_data = worker_data;
    msd_hash_foreach(temp_hash, calculate_hash, &cal);

    /* �����ʱhash�� */
    msd_hash_free(temp_hash);

    // ���item_value_hash��Ӧ��ҵ�����һ֧dlist. �����ͳһ�ͷ�
    // msd_hash_remove_entry(worker_data->item_value_hash, he->key);
    
    return MSD_OK;
}

/* ʱ���¼������ڼ���ʱ�䴰�ڣ������������˷��� */
int cal_send_back_cron(msd_ae_event_loop *el, long long id, void *privdate) 
{
    MSD_INFO_LOG("######################## BEGIN TO CALCULATE ##############################\n");
    msd_thread_worker_t *worker = (msd_thread_worker_t *)privdate;
    trap_worker_data_t *worker_data = worker->priv_data;

    msd_hash_t *item_value_hash = worker_data->item_value_hash;

    /* ������ϣ�����ҵ����м���! */
    msd_hash_foreach(item_value_hash, handle_game_data, worker_data);

    /* ������ϣ�ͳһ����ȫ����ϣ */
    msd_hash_foreach(item_value_hash, _hash_delete_foreach, item_value_hash); /*��ÿ��Ԫ�����*/
    MSD_INFO_LOG("########################## END TO CALCULATE ##############################\n");
    return 1000*worker_data->cal_send_back_interval;
}


