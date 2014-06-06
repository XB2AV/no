/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_plugin.h
 * 
 * Description :  Msd_plugin. 
 *                ������mossad��ܵķ����������û�ֻ��Ҫʵ�ִ�ͷ�ļ��µ�
 *                6����������6�������������ʵ���ʱ�������е��á�
 * 
 *     Created :  May 6, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/

#ifndef __MSD_PLUGIN_H_INCLUDED__
#define __MSD_PLUGIN_H_INCLUDED__
/*
#include <sys/cdefs.h>
#include "msd_core.h"
*/
__BEGIN_DECLS

/**
 * ����: ��ʼ���ص�
 * ����: @conf
 * ˵��: 
 *       1. ��ѡ����
 *       2. mossad��ʼ���׶Σ����ô˺�����������һЩ��ʼ������
 *       3. ����˺���ʧ�ܣ�mossad��ֱ���˳�
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_handle_init(void *conf);

/**
 * ����: mossad�رյ�ʱ�򣬴����˻ص�
 * ����: @cycle
 * ˵��: 
 *       1. ��ѡ����
 *       2. �˺����ڲ�������һЩ���ٹ���
 **/
int msd_handle_fini(void *cycle);

/**
 * ����: client���ӱ�accept�󣬴����˻ص�
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����
 *       2. һ�����дһЩ��ӭ��Ϣ��client��ȥ 
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_handle_open(msd_conn_client_t *client);

/**
 * ����: mossad�Ͽ�client���ӵ�ʱ�򣬴����˻ص�
 * ����: @clientָ��
 *       @info�����ùر����ӵ�ԭ��
 * ˵��: 
 *       1. ��ѡ����
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_handle_close(msd_conn_client_t *client, const char *info);

/**
 * ����: ��̬Լ��mossad��client֮���ͨ��Э�鳤��
 *       ��mossadӦ�ô�client->recvbuf�ж�ȡ�������ݣ�����һ����������
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����!
 *       2. �����ʱ�޷�ȷ��Э�鳤�ȣ�����0��mossad�������client��ȡ����
 *       3. �������-1��mossad����Ϊ���ִ��󣬹رմ�����
 * ����:�ɹ�:Э�鳤��; ʧ��:
 **/
int msd_handle_prot_len(msd_conn_client_t *client);

/**
 * ����: ��Ҫ���û��߼�
 * ����: @clientָ��
 * ˵��: 
 *       1. ��ѡ����
 *       2. ÿ�δ�recvbuf��Ӧ��ȡ��recv_prot_len���ȵ����ݣ���Ϊһ����������
 *       3. ��������ϵĽ��������sendbuf��Ȼ���ͻ�ȥ(ע��aeд�������ɿ�ܷ��ͻ�ȥΪ��)
 * ����:�ɹ�:MSD_OK, MSD_END; ʧ��:-x
 *       MSD_OK: �ɹ������������Ӽ���
 *       MSD_END:�ɹ������ڼ�����mossad��responseд��client֮���Զ��ر�����
 *       MSD_ERR:ʧ�ܣ�mossad�ر�����
 **/
int msd_handle_process(msd_conn_client_t *client);

__END_DECLS
#endif /* __MSD_PLUGIN_H_INCLUDED__ */

