/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_signal.h 
 * 
 * Description :  Msd_signal, Mossad�źŴ�����ص�����.
 *                �źŴ����Ϊ��������:
 *                1.������Ϊ�����߼���Ҫ���������źţ���ר�ŵ��źŴ����߳�ͬ�����д���
 *                2.�Իᵼ�³���������ֹ���ź���SIGSEGV/SIGBUS�ȣ��ɸ����߳��Լ��첽����
 * 
 *     Created :  May 17, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/
#include "msd_core.h"

extern msd_instance_t *g_ins;

volatile msd_signal_t g_signals[] = 
{
    {SIGTERM, "SIGTERM"},
    {SIGQUIT, "SIGQUIT"},
    {SIGCHLD, "SIGCHLD"},
    {SIGPIPE, "SIGPIPE"},            
    {SIGINT,  "SIGINT"},            
    {SIGSEGV, "SIGSEGV"},
    {SIGBUS,  "SIGBUS"},
    {SIGFPE,  "SIGFPE"},
    {SIGILL,  "SIGILL"},
    {SIGHUP,  "SIGHUP"},
    {0, NULL}
};

static const char *msd_get_sig_name(int sig);
static void msd_sig_segv_handler(int sig, siginfo_t *info, void *secret);
static void* msd_signal_thread_cycle(void *arg);
static void msd_public_signal_handler(int signo, msd_thread_signal_t *sig_worker);

/**
 * ����: �����ź���ֵ��ȡ�ź�����
 * ����: @sig,�ź���
 * ˵��: 
 *       �����ȡ�������򷵻�NULL
 * ����:�ɹ�:�ź���; ʧ��:NULL
 **/
static const char *msd_get_sig_name(int sig)
{
    volatile msd_signal_t *psig;
    for (psig = g_signals; psig->signo != 0; ++psig) 
    {
        if (psig->signo == sig) 
        {
            return psig->signame;
        }
    }

    return NULL;
}

/**
 * ����: ˽���źų�ʼ��
 * ˵��: 
 *       ����SIGSEGV/SIGBUS/SIGFPE/SIGILL���ĸ��ź�
 *       �ɸ����߳����в�����
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_init_private_signals() 
{
    struct sigaction sa;

    /* �����źŴ������ʹ�õ�ջ�ĵ�ַ�����û�б����ù�����Ĭ�ϻ�ʹ���������û�ջ 
     * ע��: 
     *      �����Ƿ����ö���ջ�����յ�SIGSEGV�ź�֮���ܷ��ӡ��ջ����һ��������
     *      ����ԭ�����
     **/
    /*
    static char alt_stack[SIGSTKSZ];
    stack_t ss = {
        .ss_size = SIGSTKSZ,
        .ss_sp = alt_stack,
    };
    sigaltstack(&ss, NULL);
    */

    /* ��װ�ź�
     * SA_SIGINFO  :�źŴ������������������ 
     * SA_RESETHAND:�������źŴ�������Ϻ󣬽��źŵĴ���������ΪȱʡֵSIG_DFL
     * SA_NODEFER  :һ������£� ���źŴ���������ʱ���ں˽������ø����źš�
     *              ������������� SA_NODEFER��ǣ� ��ô�ڸ��źŴ���������ʱ���ں˽������������ź�
     * SA_ONSTACK  :��������˸ñ�־����ʹ��sigaltstack������sigstack���������˱����źŶ�ջ���źŽ�
     *              �ᴫ�ݸ��ö�ջ�еĵ��ý��̣������ź��ڵ�ǰ��ջ�ϴ��ݡ�
     * SA_RESTART  :�Ƿ��������жϵ�ϵͳ����
     **/
    sigemptyset(&sa.sa_mask);/* �źŴ�����ʱ���������κ������ź� */
    //sa.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND | SA_SIGINFO | SA_RESTART;
    sa.sa_flags = SA_RESETHAND | SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = msd_sig_segv_handler;
    sigaction(SIGSEGV, &sa, NULL); /* �����δ����ʱ���յ����ź� */
    sigaction(SIGBUS, &sa, NULL);  /* ���ߴ��󣬷Ƿ���ַ, �����ڴ��ַ����(alignment)���� */
    sigaction(SIGFPE, &sa, NULL);  /* �ڷ��������������������ʱ����,�����쳣 */
    sigaction(SIGILL, &sa, NULL);  /* ִ���˷Ƿ�ָ��. ͨ������Ϊ��ִ���ļ�������ִ���, ������ͼִ�� 
                                    * ���ݶ�. ��ջ���ʱҲ�п��ܲ�������ź�
                                    */
    return MSD_OK;
}

/**
 * ����: ˽���źŴ�����
 * ����: @sig,�ź���
 *       @info,�������
 *       @secret?
 * ˵��: 
 *       ����SIGSEGV/SIGBUS/SIGFPE/SIGILL���ĸ��ź�
 *       �����������ȴ�ӡ��ջ��Ϣ��Ȼ��ָ����źŵ�
 *       Ĭ�ϲ���֮���ٷ��Ͳ�����һ���ź�
 * ע��:
 *       1. ��Ϊע���źŵ�ʱ��ָ����SA_NODEFER���������Կ�����
 *          �źŴ������У��ٴν��յ����źţ���������
 *       2. ����˽���źŵ����ִ����Ǵ�ͳ��"�첽"��ʽ
 * ����:�ɹ�:0; ʧ��:-x
 **/
static void msd_sig_segv_handler(int sig, siginfo_t *info, void *secret) 
{
    void *trace[100];
    char **messages = NULL;
    int i, trace_size = 0;
    struct sigaction sa;
    const char *sig_name;
    if(!(sig_name = msd_get_sig_name(sig)))
    {
        MSD_FATAL_LOG("Thread[%lu] receive signal: %d", pthread_self(), sig);
    }
    else
    {
        MSD_FATAL_LOG("Thread[%lu] receive signal: %s", pthread_self(), sig_name);
    }
    /* Generate the stack trace. */
    trace_size = backtrace(trace, 100);
    messages = backtrace_symbols(trace, trace_size);
    MSD_FATAL_LOG("--- STACK TRACE BEGIN--- ");
    /* ��־�д�ӡ������ĵ���ջ */
    for (i = 0; i < trace_size; ++i) 
    {
        MSD_FATAL_LOG("%s", messages[i]);
    }
    MSD_FATAL_LOG("--- STACK TRACE END--- ");

    
    /* �źź����ٳ���Ĭ�ϲ�����Ϊ�˷�ֹĬ�ϲ�����ʲô��ҪЧ��������
     * ������Ҫ����ע��һ��Ĭ�ϲ������ٷ���һ���ź�
     */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
    sa.sa_handler = SIG_DFL;
    sigaction(sig, &sa, NULL);
    raise(sig); /* raise������������������źš��ɹ�����0��ʧ�ܷ���-1�� */
}

/**
 * ����: �����źų�ʼ��
 * ˵��:   
 *   1. ���߳������ź����룬�谭ϣ��ͬ��������ź�(���̵߳��ź�����ᱻ�䴴�����̼̳߳�)
 *   2. ���̴߳����źŴ����̣߳��źŴ����߳̽�ϣ��ͬ��������źż���Ϊ sigwait�����ĵ�һ������
 * ����:�ɹ�:0; ʧ��:-x
 **/
int msd_init_public_signals() 
{
    sigset_t       bset, oset;        
    pthread_t      ppid;   
    pthread_attr_t attr;
    char           error_buf[256];
    msd_thread_signal_t *sig_worker = calloc(1, sizeof(msd_thread_signal_t));
    if(!sig_worker)
    {
        MSD_ERROR_LOG("Create sig_worker failed");
        return MSD_ERR;
    }

    g_ins->sig_worker = sig_worker;
    
    /* 
     * ����ͨ�Źܵ�
     * signal��ȡ���ź�֮�󣬽���Ӧ����д��master
     **/
    int fds[2];
    if (pipe(fds) != 0)
    {
        MSD_ERROR_LOG("Thread signal creat pipe failed");
        return MSD_ERR;
    }
    sig_worker->notify_read_fd  = fds[0];
    sig_worker->notify_write_fd = fds[1];

    /* �ܵ�������������! */
    if((MSD_OK != msd_anet_nonblock(error_buf,  sig_worker->notify_read_fd))
        || (MSD_OK != msd_anet_nonblock(error_buf,  sig_worker->notify_write_fd)))
    {
        close(sig_worker->notify_read_fd);
        close(sig_worker->notify_write_fd);
        MSD_ERROR_LOG("Set signal pipe nonblock failed, err:%s", error_buf);
        return MSD_ERR;        
    }

    /* ���������ź� */    
    sigemptyset(&bset);    
    sigaddset(&bset, SIGTERM);   
    sigaddset(&bset, SIGHUP); 
    sigaddset(&bset, SIGQUIT);
    sigaddset(&bset, SIGCHLD);
    sigaddset(&bset, SIGPIPE);
    sigaddset(&bset, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &bset, &oset) != 0)
    {
        MSD_ERROR_LOG("Set pthread mask failed");
        return MSD_ERR;
    }

    /* ����רְ�źŴ����߳� */  
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);    
    pthread_create(&ppid, NULL, msd_signal_thread_cycle, sig_worker);      
    pthread_attr_destroy(&attr);
    
    return MSD_OK;
}

/**
 * ����: רְ�źŴ����߳�
 * ˵��: 
 *      1.������߳������������źţ�ȫ�����뵽�Լ��ļ�����������
 *      2.����ѭ���������������ȴ��Լ��������źŵ���
 * ע��:
 *      1. ���ڹ����źŵ����ִ�����"ͬ��"��ʽ
 * ����:�ɹ�:0; ʧ��:-x
 **/
static void* msd_signal_thread_cycle(void *arg)
{    
    sigset_t   waitset;    
    siginfo_t  info;    
    int        rc;
    msd_thread_signal_t *sig_worker = (msd_thread_signal_t *)arg;
    
    MSD_INFO_LOG("Worker[Signal] begin to work");
    
    /* ������߳��������źţ�ȫ�������Լ��ļ�����Χ */
    sigemptyset(&waitset);
    sigaddset(&waitset, SIGTERM);    
    sigaddset(&waitset, SIGQUIT);
    sigaddset(&waitset, SIGCHLD);
    sigaddset(&waitset, SIGPIPE);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGHUP);

    /* ����ѭ���������ȴ��źŵ��� */
    while (1)  
    {       
        rc = sigwaitinfo(&waitset, &info);        
        if (rc != MSD_ERR) 
        {   
            /* ͬ�������ź� */
            msd_public_signal_handler(info.si_signo, sig_worker);
        } 
        else 
        {            
            MSD_ERROR_LOG("Sigwaitinfo() returned err: %d; %s", errno, strerror(errno));       
        }    

     }
     return (void *)NULL;
}

/**
  * ����: ͬ���źŴ���
  * ����: @�ź���ֵ
  * ˵��: 
  *      1.����յ�SIGPIPE/SIGCHLD�źţ�����
  *      2.����յ�SIGQUIT/SIGTERM/SIGINT��֪ͨmaster��worker�˳�
  *      3.�ڴ�ͳ���źŴ������У���Ҫ����ϵͳ�����ǿ�����ģ���
  *        ��������ͬ���źŴ����������ÿ���
  **/
static void msd_public_signal_handler(int signo, msd_thread_signal_t *sig_worker) 
{
    const char *sig_name;
    int to_stop = 0;
    if(!(sig_name = msd_get_sig_name(signo)))
    {
        MSD_INFO_LOG("Thread[%lu] receive signal: %d", pthread_self(), signo);
    }
    else
    {
        MSD_INFO_LOG("Thread[%lu] receive signal: %s", pthread_self(), sig_name);
    } 
    
    switch (signo) 
    {
        case SIGQUIT: 
        case SIGTERM:
        case SIGINT:
            /* prepare for shutting down */
            to_stop = 1;
            break;
        case SIGPIPE:
        case SIGCHLD:
        case SIGHUP:
            /* do noting */
            break;
    }

    if(to_stop)
    {
        /* ֪ͨmaster�رգ�����Ҫ����write����������� */
        if(4 != write(sig_worker->notify_write_fd, "stop", 4))
        {
            if(errno == EINTR)
            {
                write(sig_worker->notify_write_fd, "stop", 4);
                MSD_INFO_LOG("Worker[signal] exit!");
                pthread_exit(0);
            }
            else
            {
                MSD_ERROR_LOG("Pipe write error, errno:%d", errno);
            }
        }
        else
        {
            /* �������֮��signal�߳����ʹ�����˳� */
            MSD_INFO_LOG("Worker[signal] exit!");
            pthread_exit(0);
        }
    }
}


#ifdef __MSD_SIGNAL_TEST_MAIN__

void msd_thread_sleep(int s) 
{
    struct timeval timeout;
    timeout.tv_sec  = s;
    timeout.tv_usec = 0;
    select( 0, NULL, NULL, NULL, &timeout);
}

void segment_fault()
{
    char *ptr = 0;
    printf("%c", *ptr);
}

void* phtread_SIGSEGV(void *arg)
{
    msd_thread_sleep(1);
    int a = 0;
    printf("a=%d", a);
    /* ����δ��� */
    segment_fault();

    return (void *)NULL;
}


void sig_int_handler(int sig)
{
    const char *sig_name;
    if(!(sig_name = msd_get_sig_name(sig)))
    {
        MSD_FATAL_LOG("Thread[%lu] receive signal: %d", pthread_self(), sig);
    }
    else
    {
        MSD_FATAL_LOG("Thread[%lu] receive signal: %s", pthread_self(), sig_name);
    }    
}

int sig_int_action()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);/* �źŴ�����ʱ���������κ������ź� */

    /* ����SA_RESETHAND��Ч�����û�д˱�־���յ�SIGINT��Σ��͵���sig_int_handler��Σ�����ֻ�ܵ���һ�� */ 
    sa.sa_flags = SA_RESTART;
    //sa.sa_flags = SA_RESETHAND | SA_RESTART;
    sa.sa_handler = sig_int_handler;
    sigaction(SIGINT, &sa, NULL); /* �����δ����ʱ���յ����ź� */ 
    return 0;
}

void * phtread_SIGINT(void *arg)
{
    while(1)
    {
        printf("%lu\n", pthread_self());
        msd_thread_sleep(10);
    }
    return (void *)NULL;
}


int main(int argc, char *argv[]) 
{
    int ret = msd_init_private_signals();
    msd_log_init( "./", "signal.log", 4, 100000000, 1, 0);
    MSD_INFO_LOG("Init ret:%d", ret);
    
    /* ����1�����̶߳δ��� */
    /*
    char *ptr = NULL;
    // *ptr = 'c';
    printf("%c", *ptr);      
    */

    /* ����2�����̶߳δ����Ƿ��ܴ�ӡ��ջ����������ԭ����ʱ���� */
    /*
    int i;
    char *ptr = NULL;
    pthread_t      ppid;    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    for(i=0; i<10; i++)
    {
        if(pthread_create(&ppid, &attr, phtread_SIGSEGV, NULL) != 0)
        {
            MSD_ERROR_LOG("Thread creat failed");
            return MSD_ERR;        
        }        
        else
        {
            MSD_INFO_LOG("Thread creat %lu", ppid);
        }
    }
    
    pthread_attr_destroy(&attr);
    //���̶߳δ��������ߣ������̷߳����δ���
    msd_thread_sleep(10);
    printf("%c", *ptr); 
    */

    /* ����3�����Ե��߳��յ�SIGINT��˳�����SA_RESETHANDЧ�� */
    /*
    sig_int_action();
    msd_thread_sleep(100);
    printf("After receive sigint1\n");
    msd_thread_sleep(100);
    printf("After receive sigint2\n");
    */
    
    /* ����4���ڲ����޶�������£��鿴�����źŵ��߳��Ƿ�������� */
    /*
    //���Խ����ʾ��ò����Զ�����߳̽��յ��źţ������Ͻ���������
    //�����޷�ȷ�����Ƿ��Ǳ�Ȼ����
    sig_int_action();
    pthread_t      ppid;    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int i=0;
    for(i=0; i<10; i++)
    {
        if(pthread_create(&ppid, &attr, phtread_SIGINT, NULL) != 0)
        {
            MSD_ERROR_LOG("Thread creat failed");
            return MSD_ERR;        
        }
        else
        {
            MSD_INFO_LOG("Thread creat %lu", ppid);
        }
    }
    pthread_attr_destroy(&attr);
    //���ߣ��鿴�����ĸ��߳��ܹ��յ��ź�
    while(1)
        msd_thread_sleep(100);
    */

    /* ����5������ר���ź��̣߳������߳������ź� */
    msd_init_public_signals();
    
    pthread_t      ppid;    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int i=0;
    for(i=0; i<10; i++)
    {
        if(pthread_create(&ppid, &attr, phtread_SIGINT, NULL) != 0)
        {
            MSD_ERROR_LOG("Thread creat failed");
            return MSD_ERR;        
        }
        else
        {
            MSD_INFO_LOG("Thread creat %lu", ppid);
        }
    }
    pthread_attr_destroy(&attr);
    //���ߣ��鿴�����ĸ��߳��ܹ��յ��ź�
    while(1)
    {
        msd_thread_sleep(10);
        printf("%lu\n", pthread_self());
        /* ���Թ����źź�˽���źŶ����ڵ���� */
        //char *ptr = NULL;
        //*ptr = 'c';
    }
    return 0;
    
}

#endif /* __MSD_SIGNAL_TEST_MAIN__ */
