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
int msd_thread_worker_create(msd_thread_pool_t *pool, void* (*worker_task)(void *), int idx)
{
    msd_thread_worker_t *worker  = NULL;
    if(!(worker = calloc(1, sizeof(msd_thread_worker_t))))
    {
        MSD_ERROR_LOG("Thread_worker_creat failed, idx:%d", idx);
        return MSD_ERR;
    }
    
    worker->pool                 = pool;
    worker->idx                  = idx;
    pool->thread_worker_array[idx] = worker; /* �����Ӧ��λ�� */

    /* ����ͨ�Źܵ�
     * master�߳̽���Ҫ�����fdд��ܵ���woker�̴߳ӹܵ�ȡ��
     **/
    int fds[2];
    if (pipe(fds) != 0)
    {
        free(worker);
        MSD_ERROR_LOG("Thread_worker_creat pipe failed, idx:%d", idx);
        return MSD_ERR;
    }
    worker->notify_read_fd = fds[0];
    worker->notify_write_fd = fds[1];
    
    /*
    worker->ev_base = (struct event_base*) event_init();
    if (!worker->ev_base)
    {
        GKOLOG(FATAL, "Worker event base initialize error");
        return -1;
    }
    */
    
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
int msd_thread_worker_destroy(msd_thread_worker_t *worker) 
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
void* msd_thread_worker_task(void* arg) 
{
    msd_thread_worker_t *worker = (msd_thread_worker_t *)arg;
    MSD_INFO_LOG("Thread no.%d begin to work", worker->idx);
    
    while(1)
    {
        msd_thread_sleep(1);
        //printf("I am thread %d, My tid:%lu\n", worker->idx, (long unsigned int)worker->tid);
    }
    return NULL;
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
