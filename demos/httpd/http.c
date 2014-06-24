/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  echo.c
 * 
 * Description :  Mossad���Http������ʾ��
 *                ʵ��msd_plugin.h�еĺ��������ɽ��û��߼�Ƕ��mossad���
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/
#include "msd_core.h"

#define HTTP_METHOD_GET     0
#define HTTP_METHOD_PUT     1
#define HTTP_METHOD_POST    2
#define HTTP_METHOD_HEAD    3

#define RESPONSE_BUF_SIZE   4096

static char *g_doc_root;
static char *g_index_file;

/**
 * ����: ��ʼ���ص�
 * ����: @conf
 * ˵��: 
 *       1. ��ѡ����
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_handle_init(void *conf) 
{
    g_doc_root   = msd_conf_get_str_value(conf, "docroot", "./");
    g_index_file = msd_conf_get_str_value(conf, "index", "index.html");

    return MSD_OK;
}

/**
 * ����: client���ӱ�accept�󣬴����˻ص�
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����
 *       2. һ�����дһЩ��ӭ��Ϣ��client��ȥ 
 * ����:�ɹ�:0; ʧ��:-x
 **/
/*
int msd_handle_open(msd_conn_client_t *client) 
{
    int write_len;
    char buf[1024];
    sprintf(buf, "Hello, %s:%d, welcom to mossad!\n", client->remote_ip, client->remote_port);
    //��ӭ��Ϣд��sendbuf
    msd_str_cpy(&(client->sendbuf), buf);
    
    if((write_len = write(client->fd, client->sendbuf->buf, client->sendbuf->len))
        != client->sendbuf->len)
    {
        MSD_ERROR_LOG("Handle open error! IP:%s, Port:%d. Error:%s", client->remote_ip, client->remote_port, strerror(errno));
        //TODO����write����ae_loop
        return MSD_ERR;
    }
    return MSD_OK;
}
*/

/**
 * ����: mossad�Ͽ�client���ӵ�ʱ�򣬴����˻ص�
 * ����: @clientָ��
 *       @info�����ùر����ӵ�ԭ��
 * ˵��: 
 *       1. ��ѡ����
 * ����:�ɹ�:0; ʧ��:-x
 **/
/*
void msd_handle_close(msd_conn_client_t *client, const char *inf) 
{
    MSD_DEBUG_LOG("Connection from %s:%d closed", client->remote_ip, client->remote_port);
}
*/

/**
 * ����: ��̬Լ��mossad��client֮���ͨ��Э�鳤�ȣ���mossadӦ�ö�ȡ�������ݣ�����һ������
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����
 *       2. �����ʱ�޷�ȷ��Э�鳤�ȣ�����0��mossad�������client��ȡ����
 *       3. �������-1��mossad����Ϊ���ִ��󣬹رմ�����
 *       3. ��򵥵�HttpЭ��request��ʾ��:
 *           GET /www/index.html HTTP/1.1 \r\n
 *           Host: www.w3.org \r\n
 *           \r\n
 *           request-body.....
 * ����:�ɹ�:Э�鳤��; ʧ��:
 **/
int msd_handle_prot_len(msd_conn_client_t *client) 
{
    /* At here, try to find the end of the http request. */
    int content_len;
    char *ptr;
    char *header_end;
    
    /* �ҵ�httpЭ��request��Header��ĩβ"\r\n\r\n" */
    if (!(header_end = strstr(client->recvbuf->buf, "\r\n\r\n"))) 
    {
        /* û���ҵ�ĩβ������Ϊ�˰�������������0��mossad�������ȡ */
        return 0;
    }
    header_end += 4;
    
    /* find content-length header */
	/* Http����ͷ���û��content-length�ֶΣ��򳤶Ⱦ��������к�����ͷ�Լ�����������س����еĳ���֮�� 
	 * ������ټ���content-length�ֶ�
	 */
    if ((ptr = strstr(client->recvbuf->buf, "Content-Length:"))) 
    {
        content_len = strtol(ptr + strlen("Content-Length:"), NULL, 10);
        if (content_len == 0) 
        {
            MSD_ERROR_LOG("Invalid http protocol: %s", client->recvbuf->buf);
            return MSD_ERR;
        }
        return header_end - client->recvbuf->buf + content_len;
    } 
    else 
    {
        return header_end - client->recvbuf->buf;
    }
}

/**
 * ����: ��Ҫ���û��߼�
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����
 *       2. ÿ�δ�recvbuf��Ӧ��ȡ��recv_prot_len���ȵ����ݣ���Ϊһ����������
 *       3. ��򵥵�HttpЭ��reponse��ʾ��:
 *              HTTP/1.1 200 OK \r\n
 *              Content-Type: text/html \r\n
 *              Content-Length: 158 \r\n
 *              Server: Apache-Coyote/1.1 \r\n
 *              \r\n
 *              response-body.....
 * ����:�ɹ�:0; ʧ��:-x
 *       MSD_OK: �ɹ������������Ӽ���
 *       MSD_END:�ɹ������ڼ�����mossad��responseд��client���Զ��ر�����
 *       MSD_ERR:ʧ�ܣ�mossad�ر�����
 **/
int msd_handle_process(msd_conn_client_t *client) 
{
    /* Parse request and generate response. */
    int n = 0;
    int file_size;
    char *ptr = client->recvbuf->buf;
    char *ptr2;
    char file[512] = {};
    char *buf;
    int fd;
    
    if (!strncmp(ptr, "GET", 3)) 
    {  
        /* Only support method GET */
        ptr += 4;
        while(*ptr == '/')
        {
            ptr++;
        }
    } 
    else 
    {
        //TODO unsupported methods
        return MSD_ERR;
    }

    if (!(ptr2 = strstr(ptr, "HTTP/"))) 
    {
        return MSD_ERR;
    }

    *--ptr2 = '\0';
    
    /* Generate filename accessed */
    if( ptr[strlen(ptr) - 1] == '/' )
    {
        snprintf(file, sizeof(file), "%s/%s%s", g_doc_root, ptr, g_index_file);
    }
    else
    {
        snprintf(file, sizeof(file), "%s/%s", g_doc_root, ptr);
    }
    
    fd = open(file, O_RDONLY);
    if (fd < 0) 
    {
        /* 404 error */
        MSD_ERROR_LOG("Open file [%s] failed, 404 Error returned", file);
        snprintf(file, sizeof(file), "%s/404.html", g_doc_root);
        fd = open(file, O_RDONLY);
        file_size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        msd_str_sprintf(&(client->sendbuf), "HTTP/1.1 404 Not Found\r\nServer: mossad \r\n"
                    "Content-Length: %d\r\n\r\n",file_size);
    }
    else
    {
        MSD_INFO_LOG("Open file [%s]", file);
        file_size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET); 
        msd_str_sprintf(&(client->sendbuf), "HTTP/1.1 200 OK\r\nServer: mossad \r\n"
                    "Content-Length: %d\r\n\r\n",file_size); 
    }
    buf = (char *)calloc(1, file_size+1);
    ptr = buf;
    ptr2 = buf + file_size;
    while(( n = read(fd, ptr, ptr2 - ptr)) > 0)
    {
        ptr += n;
    }
    *ptr = '\0';
    
    //֧�ֶ������ļ�
    //msd_str_cat(&(client->sendbuf), buf);
    msd_str_cat_len(&(client->sendbuf), buf, file_size);
    free(buf);
    close(fd);
    
    /*
    // ע���д�¼�--�������ڲ�ʵ��
    worker = msd_get_worker(client->worker_id);
    if (msd_ae_create_file_event(worker->t_ael, client->fd, MSD_AE_WRITABLE,
                msd_write_to_client, client) == MSD_ERR) 
    {
        msd_close_client(client->idx, "create file event failed");
        MSD_ERROR_LOG("Create write file event failed for connection:%s:%d", client->remote_ip, client->remote_port);
        return MSD_ERR;
    }
    */
    return MSD_END;
}

/**
 * ����: mossad�رյ�ʱ�򣬴����˻ص�
 * ����: @cycle
 * ˵��: 
 *       1. ��ѡ����
 **/
/*
void handle_fini(void *cycle) 
{

}
*/
