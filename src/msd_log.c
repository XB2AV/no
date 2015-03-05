/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_log.c
 * 
 * Description :  Msd_log, a generic log implementation.
 *                �����汾����־�����̰汾���̰߳汾��
 *                ��mossad���ö����ʱ���ý��̰汾�����ö��߳�ʱ�����̰߳汾
 *                ����ӿڶ���ͳһ�ģ�ֻ��Ҫ��Makefile�ж�����Ҫ���͵ĺ�
 *                LOG_MODE = -D__MSD_LOG_MODE_THREAD__/-D__MSD_LOG_MODE_PROCESS__
 *
 *
 *     Version :  1.0.0
 * 
 *      Author :  HQ 
 *
 **/
#include "msd_core.h"

static char *msd_log_level_name[] = { /* char **msd_log_level_name */
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG"
};

static msd_log_t g_log; /* ȫ��Log��� */

/**
 * ����: ����һ�鹲���ڴ�(����!)
 * ����:
 *      1. ������ʽ�򿪹����ڴ棬����Ҫӳ���ļ�
 *      2. MAP_PRIVATE�����̵�˽�еĹ����ڴ棬����������������ӽ��̣����Ӵ�ӡ������msd_log_buffer
 *         ��ֵ������ͬ�ģ�������ȷʵ˽�еģ�����Ӱ��
 *      3. MAP_SHARED�������ܹ����ӽ��̻��๲��
 *      4. �������ԣ��ڶ��߳�ģʽ�£������ڴ�ķ�ʽ�ᷢ�����ң����Բ�ʹ�ù����ڴ�ģʽ
 * ����: �ɹ���0 ʧ�ܣ�-x

static int msd_get_map_mem()
{
    if(msd_log_buffer == MAP_FAILED)
    {
        //printf("mmap is called by pid %d\n", getpid());
        msd_log_buffer = mmap(0, MSD_LOG_BUFFER_SIZE, PROT_WRITE | PROT_READ,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                //MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (msd_log_buffer == MAP_FAILED)
        {
            fprintf(stderr, "mmap log buffer failed: %s\n",strerror(errno));
            return MSD_FAILED;
        }
    }
    return MSD_OK;
}
 **/
 
/**
 * ����: ����ok��ֵ�������ɫ������ɫ����Ļ���������80�ַ�
 **/
void msd_boot_notify(int ok, const char *fmt, ...)
{
    va_list  ap;
    int      n;

    char log_buffer[MSD_LOG_BUFFER_SIZE] = {0};
    va_start(ap, fmt);
    n = vsnprintf(log_buffer, MSD_SCREEN_COLS, fmt, ap);/*n �ǳɹ�д����ַ���*/
    va_end(ap);

    if(n > MSD_CONTENT_COLS)
    {
        printf("%-*.*s%s%s\n", MSD_CONTENT_COLS-5, MSD_CONTENT_COLS-5,/*����ǰ��*��������ܵĿ�ȣ����*��ָ������ַ�������*/
                log_buffer, " ... ", ok==0? MSD_OK_STATUS:MSD_FAILED_STATUS);
    }
    else
    {
        printf("%-*.*s%s\n", MSD_CONTENT_COLS, MSD_CONTENT_COLS,
                log_buffer, ok==0? MSD_OK_STATUS:MSD_FAILED_STATUS);
    }
}

/**
 * ����: ������־�ļ�fd
 * ����: @index
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
static int msd_log_reset_fd(int index)
{
    int status;
    if(g_log.g_msd_log_files[index].fd != -1)
    {
        close(g_log.g_msd_log_files[index].fd);
    }
    g_log.g_msd_log_files[index].fd = open(g_log.g_msd_log_files[index].path,
            O_WRONLY|O_CREAT|O_APPEND, 0644);
    if(g_log.g_msd_log_files[index].fd < 0)
    {
        fprintf(stderr, "open log file %s failed: %s\n",
               g_log.g_msd_log_files[index].path, strerror(errno));
        return MSD_FAILED;
    }

    /* in case exec.. */
    /*����ӽ��̱�exec�庯���滻���������޷��������ļ���*/
    status = fcntl(g_log.g_msd_log_files[index].fd, F_GETFD, 0);
    status |= FD_CLOEXEC;
    fcntl(g_log.g_msd_log_files[index].fd, F_SETFD, status);  
    return MSD_OK;
}

/**
 * ����: log init
 * ����: @dir Ŀ¼λ��
 *       @filename �ļ���
 *       @level 
 *       @size
 *       @lognum
 *       @multi
 * ����:
 *      1. access(dir, mode)
 *         mode:��ʾ���Ե�ģʽ���ܵ�ֵ��:
 *              R_OK:�Ƿ���ж�Ȩ��             
 *              W_OK:�Ƿ���п�дȨ��
 *              X_OK:�Ƿ���п�ִ��Ȩ��             
 *              F_OK:�ļ��Ƿ����             
 *              ����ֵ:�����Գɹ��򷵻�0,���򷵻�-1
 *  
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_log_init(const char *dir, const char *filename, int level, int size, int lognum, int multi)
{
    assert(dir && filename);
    assert(level >= MSD_LOG_LEVEL_FATAL && level <= MSD_LOG_LEVEL_ALL);
    assert(size >= 0 && size <= MSD_LOG_FILE_SIZE);
    assert(lognum > 0 && lognum <= MSD_LOG_MAX_NUMBER);

    if(g_log.msd_log_has_init)
    {
        return MSD_ERR;
    }
    
    /* whether can be wirten */
    if (access(dir, W_OK)) 
    {
        fprintf(stderr, "access log dir %s failed:%s\n", dir, strerror(errno));
        return MSD_ERR;
    }

    g_log.msd_log_level = level;
    g_log.msd_log_size  = size;
    g_log.msd_log_num   = lognum;
    g_log.msd_log_multi = multi; /* !!multi */

    /* ��ʼ��roate�� */
    if (MSD_LOCK_INIT(g_log.msd_log_rotate_lock) != 0) 
    {
        return MSD_ERR;
    }
    
    if(g_log.msd_log_multi)/* multi log mode */
    {
        int i;
        for(i=0; i<=MSD_LOG_LEVEL_DEBUG; i++)
        {
            g_log.g_msd_log_files[i].fd = -1;
            memset(g_log.g_msd_log_files[i].path, 0, MSD_LOG_PATH_MAX);
            strcpy(g_log.g_msd_log_files[i].path, dir);
            if(g_log.g_msd_log_files[i].path[strlen(dir)] != '/')
            {
                strcat(g_log.g_msd_log_files[i].path,  "/");
            }
            strcat(g_log.g_msd_log_files[i].path, filename);
            strcat(g_log.g_msd_log_files[i].path, "_");
            strcat(g_log.g_msd_log_files[i].path, msd_log_level_name[i]);

            /*��ʼ��fd*/
            if(MSD_OK != msd_log_reset_fd(i))
            {
                return MSD_FAILED;
            }
        }
    }
    else
    {
        g_log.g_msd_log_files[0].fd = -1;
        memset(g_log.g_msd_log_files[0].path, 0, MSD_LOG_PATH_MAX);
        strcpy(g_log.g_msd_log_files[0].path, dir);
        if(g_log.g_msd_log_files[0].path[strlen(dir)] != '/')
        {
            strcat(g_log.g_msd_log_files[0].path, "/");
        }
        strcat(g_log.g_msd_log_files[0].path, filename);

        /*��ʼ��fd*/
        if(MSD_OK != msd_log_reset_fd(0))
        {
            return MSD_FAILED;
        }
    }

    /*
    if(msd_get_map_mem())
    {
        return MSD_ERR;
    }
    */

    g_log.msd_log_has_init = 1;
    return MSD_OK;
}

/**
 * ����: close log
 **/
void msd_log_close()
{
    int i;
    if(g_log.msd_log_multi)
    {
        for(i=0; i<=MSD_LOG_LEVEL_DEBUG; i++)
        {
            if(g_log.g_msd_log_files[i].fd != -1)
            {
                close(g_log.g_msd_log_files[i].fd);
                g_log.g_msd_log_files[i].fd = -1;
            }
        }
    }
    else
    {
        if(g_log.g_msd_log_files[0].fd != -1)
        {
            close(g_log.g_msd_log_files[0].fd);
            g_log.g_msd_log_files[0].fd = -1;
        }
    }
    
    MSD_LOCK_DESTROY(g_log.msd_log_rotate_lock); /* ������ */
}

#ifdef MSD_LOG_MODE_PROCESS
/**
 * ����: ��־�и���̰汾
 * ����: @
 * ����:
 *      1. rotate��ʱ�򣬼�����������ֹ����
 *      2. Ϊ�˱�֤�����ģʽ�µĿɿ����ֱ������fstat��stat
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
static int msd_log_rotate(int fd, const char* path, int level)
{
    char tmppath1[MSD_LOG_PATH_MAX];
    char tmppath2[MSD_LOG_PATH_MAX];
    int  i;
    struct stat st;
    int index;
    
    index = g_log.msd_log_multi? level:0;

    /*
     * ע��!
     * �˴�����fstat���������Ǵ�����stat��Ϊ�˼��ݶ���̵�������˴�����fd����Ϊ��Ľ����п���
     * �Ѿ�rotate����־�ļ����������path���Ϳ��ܼ�����Сû�г�����ֵ(��Ϊ��Ľ�����rotate)��
     * ����д���ʱ���õ���fd����ͳ������⣬��Ϊ�ý���fdָ����Ѿ���xxx.log.max����������xxx.log
     * ��������־��д���Ѿ�rotate�˵�xx.log.max����ȥ����������˴���fd���Ͳ������������⣬��Ϊ��
     * ���ļ����ֱ仯�ˣ�����fd��Ȼָ�����ԭ�����ļ���������Ȼ�ж�test.log.max����Ȼ���Է�����־����
     **/
    /* get the file satus, store in st */
    if(MSD_OK != fstat(fd, &st))
    {
        /*�������쳣����������fd�������Ƿ�ɹ�������FAILED*/
        //MSD_LOCK_LOCK(g_log.msd_log_rotate_lock);
        msd_log_reset_fd(index);
        //MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
        return MSD_FAILED;
    }

    if(st.st_size >= g_log.msd_log_size)
    {
        //����
        MSD_LOCK_LOCK(g_log.msd_log_rotate_lock);
        printf("process %d get the lock\n", getpid());
        //sleep(5);
        /*
         * ע��!
         * �ٴ��ж��Ƿ񳬱꣬�˴�������stat���������Ǵ�������fstat����Ϊ�������fd����Ȼ������־����
         * ���Ͼ������Ѿ�������һ�Ρ���ʱ�п������������Ѿ������roate�����Ե���path�����ж�һ�Σ���
         * 1.�����Ȼ����: ˵��û����������rotate�����Ǳ����̸���rotate
         * 2.������ٳ���: ˵�����������Ѿ������roate��������fd�Ѿ�ָ����xx.log.max��
         *   �򱾽���ֻ��Ҫ�����Լ���fd
         **/
        if(MSD_OK != stat(path, &st))
        {
            printf("process %d relase the lock, stat error\n", getpid());
            perror("the reason of the error:");
            msd_log_reset_fd(index);
            MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
            return MSD_FAILED;
        }
        
        /*
         * ����˿̷��֣���־�ļ��Ѿ�������roate�������ˣ�˵�����Ѿ�����������rotate�ˣ�
         * ��˿�Ӧ�÷���OK����NONEED��������Ȼ�������Լ���fd
         **/        
        if(st.st_size < g_log.msd_log_size)
        {
            close(g_log.g_msd_log_files[index].fd);
            g_log.g_msd_log_files[index].fd = -1;

            if(MSD_OK != msd_log_reset_fd(index))
            {
                MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
                return MSD_FAILED;               
            }
            
            printf("process %d relase the lock,ohter process roate\n", getpid());
            /*
             * ����˼·:�����Է��ָ����ϵ�һ��Ч���Ϻ�
             * 1.��������������OK�����Լ�д��֮�����
             * 2.ֱ�ӽ���������NONEED��Ȼ����write����д��  
             */
            /*����wirte����д����Ϻ����ͷ�*/
            //MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
            //return MSD_NONEED;
            return MSD_OK;
        }
        
        /* ��Ȼ���꣬˵����������û�н���rotate�����ɱ�������� */
        /* find the first not exist file name */
        for(i = 0; i < g_log.msd_log_num; i++)
        {
            snprintf(tmppath1, MSD_LOG_PATH_MAX, "%s.%d", path, i);
       
            /* whether exist */        
            if(access(tmppath1, F_OK))
            {
                /* the file tmppath1 not exist */
                rename(path, tmppath1);/*rename(from, to)*/
                printf("process %d find the unexist file\n", getpid());

                close(g_log.g_msd_log_files[index].fd);
                g_log.g_msd_log_files[index].fd = -1;

                if(MSD_OK != msd_log_reset_fd(index))
                {
                    MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
                    return MSD_FAILED;               
                }
                
                /* ��ʵʩ��rotate����Ӧ�ý�����Ӧ�õȴ���д���ˣ��Ž��� */
                //MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
                //sleep(5);
                return MSD_OK;
            }
        }
        
        /* all path.n exist, then ,rotate */
        /* �����־���Ѿ��ﵽ���ޣ���Ὣ���еĴ���׺����־����ǰ�ƣ�������ǰ��־��������׺���� */
        for(i=1; i<g_log.msd_log_num; i++)
        {
            snprintf(tmppath1, MSD_LOG_PATH_MAX, "%s.%d", path, i);
            snprintf(tmppath2, MSD_LOG_PATH_MAX, "%s.%d", path, i-1);
            /* rename file.(delete the file.n which n is low) */
            rename(tmppath1, tmppath2);
        }

        /*����ǰ��־��������׺����*/
        snprintf(tmppath2, MSD_LOG_PATH_MAX, "%s.%d", path, g_log.msd_log_num-1);
        rename(path, tmppath2);

        close(g_log.g_msd_log_files[index].fd);
        g_log.g_msd_log_files[index].fd = -1;
        
        if(MSD_OK != msd_log_reset_fd(index))
        {
            MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
            return MSD_FAILED;               
        }
        
        printf("process %d roate the file\n", getpid());
        /*��ʵʩ��rotate����Ӧ�ý�����Ӧ�õȴ���д���ˣ��Ž���*/
        //MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
        //sleep(2);
        return MSD_OK;

    }
    
    return MSD_NONEED;

}
/**
 * ����: ��־д�룬���̰汾
 * ����: @level�� @fmt , @...
 * ����:
 *      1. ÿ��д��ǰ���ᴥ��rotate��� 
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_log_write(int level, const char *fmt, ...)
{
    struct tm   tm; /* time struct */
    int         pos;
    int         end;
    va_list     ap;
    time_t      now;
    int         index;
    int         rotate_result;
    char        log_buffer[MSD_LOG_BUFFER_SIZE] = {0};
    
    if(!g_log.msd_log_has_init)
    {
        return MSD_ERR;
    }

    /* only write the log whose levle low than system initial log level */
    if(level > g_log.msd_log_level)
    {
        return MSD_NONEED;
    }
    /*
    if(msd_get_map_mem() != MSD_OK)
    {
        return MSD_FAILED;
    }
    */
    now = time(NULL);
    /*localtime_r() ����������ʱ��timepת��Ϊ�û�ָ����ʱ����ʱ�䡣���������Խ����ݴ洢���û��ṩ�Ľṹ���С�*/
    localtime_r(&now, &tm);

    pos = sprintf(log_buffer, "[%04d-%02d-%02d %02d:%02d:%02d][%05d][%5s]",
                tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, getpid(),
                msd_log_level_name[level]);

    /*�ӹ���־����*/
    va_start(ap, fmt);
    end = vsnprintf(log_buffer+pos, MSD_LOG_BUFFER_SIZE-pos, fmt, ap);
    va_end(ap);

    /* Խ�紦�� */
    if(end >= (MSD_LOG_BUFFER_SIZE-pos))
    {
        log_buffer[MSD_LOG_BUFFER_SIZE-1] = '\n';
        memset(log_buffer+MSD_LOG_BUFFER_SIZE-4, '.', 3);
        end = MSD_LOG_BUFFER_SIZE-pos-1;
    }
    else
    {
        log_buffer[pos+end] = '\n';
    }    

    index = g_log.msd_log_multi? level:0;

    /*�쳣����*/
    if(g_log.g_msd_log_files[index].fd == -1)
    {
        //MSD_LOCK_LOCK(g_log.msd_log_rotate_lock);
        if(MSD_OK == msd_log_reset_fd(index))
        {
            return MSD_FAILED;
        }
        //MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
    }


    if(MSD_NONEED == (rotate_result = msd_log_rotate(g_log.g_msd_log_files[index].fd, (const char*)g_log.g_msd_log_files[index].path, level)))
    {
        if(write(g_log.g_msd_log_files[index].fd, log_buffer, end + pos + 1) != (end + pos + 1))/* +1 ��Ϊ��'\n' */            
        {
            fprintf(stderr,"write log to file %s failed: %s\n", g_log.g_msd_log_files[index].path, strerror(errno));
            return MSD_FAILED;
        }
    }
    else if(MSD_OK == rotate_result)
    {
        /* ����MSD_OK ˵�����ɱ�����ִ����roate�������ڴ�������״̬�е�ʱ�򣬱�Ľ��������roate����Ӧ���������д�����֮���ٽ��� */
        if(write(g_log.g_msd_log_files[index].fd, log_buffer, end + pos + 1) != (end + pos + 1))       
        {
            fprintf(stderr,"write log to file %s failed: %s\n", g_log.g_msd_log_files[index].path, strerror(errno));
            return MSD_FAILED;
        }
        /*����*/
        printf("process %d relase the lock\n", getpid());
        MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);        
    }
    else
    {
        /*do nothing*/
        return MSD_FAILED;
    }

    return MSD_OK;
}
#else

/**
 * ����: ��־�и�̰߳汾
 * ����: @
 * ����:
 *      1. rotate��ʱ�򣬼�����������ֹ����
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
static int msd_log_rotate(int fd, const char* path, int level)
{
    char tmppath1[MSD_LOG_PATH_MAX];
    char tmppath2[MSD_LOG_PATH_MAX];
    int  i;
    struct stat st;
    int index;
    
    index = g_log.msd_log_multi? level:0;

    /* get the file satus, store in st */
    if(MSD_OK != fstat(fd, &st))
    {
        /* �������쳣����������fd�������Ƿ�ɹ�������FAILED
         * �������������з��֣��ᷢ���Լ����Լ���ס
         */
        //MSD_LOCK_LOCK(g_log.msd_log_rotate_lock);
        msd_log_reset_fd(index);
        //MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
        return MSD_FAILED;
    }

    if(st.st_size >= g_log.msd_log_size)
    {
        //����
        MSD_LOCK_LOCK(g_log.msd_log_rotate_lock);
        printf("thread %lu get the lock\n", (unsigned long)pthread_self());
        //sleep(5);
        /*
         * ע��!
         * �ٴ��ж��Ƿ񳬱꣬�˴��п������������Ѿ������roate���������ж�һ�Σ���
         * 1.�����Ȼ����: ˵��û�����߳�rotate�����Ǳ����̸���rotate
         * 2.������ٳ���: ˵�������߳��Ѿ������roate��fd�Ѿ����£���ֱ���˳�
         **/
        if(MSD_OK != fstat(fd, &st))
        {
            printf("thread %lu relase the lock, stat error\n", (unsigned long)pthread_self());
            perror("the reason of the error:");
            msd_log_reset_fd(index);
            MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
            return MSD_FAILED;
        }
        
        /* ˵�������߳��Ѿ������roate��fd�Ѿ����£���ֱ���˳� */        
        if(st.st_size < g_log.msd_log_size)
        {
        
            printf("thread %lu relase the lock,ohter thread roate\n", (unsigned long)pthread_self());
            MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
            return MSD_NONEED;
        }
        
        /* ��Ȼ���꣬˵�������߳�û�н���rotate�����ɱ�������� */
        /* find the first not exist file name */
        for(i = 0; i < g_log.msd_log_num; i++)
        {
            snprintf(tmppath1, MSD_LOG_PATH_MAX, "%s.%d", path, i);
       
            /* whether exist */        
            if(access(tmppath1, F_OK))
            {
                /* the file tmppath1 not exist */
                rename(path, tmppath1);/*rename(from, to)*/
                printf("thread %lu find the unexist file\n", (unsigned long)pthread_self());

                close(g_log.g_msd_log_files[index].fd);
                g_log.g_msd_log_files[index].fd = -1;

                if(MSD_OK != msd_log_reset_fd(index))
                {
                    MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
                    return MSD_FAILED;               
                }

                MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
                //sleep(5);
                return MSD_OK;
            }
        }
        
        /* all path.n exist, then ,rotate */
        /* �����־���Ѿ��ﵽ���ޣ���Ὣ���еĴ���׺����־����ǰ�ƣ�������ǰ��־��������׺���� */
        for(i=1; i<g_log.msd_log_num; i++)
        {
            snprintf(tmppath1, MSD_LOG_PATH_MAX, "%s.%d", path, i);
            snprintf(tmppath2, MSD_LOG_PATH_MAX, "%s.%d", path, i-1);
            /* rename file.(delete the file.n which n is low) */
            rename(tmppath1, tmppath2);
        }

        /*����ǰ��־��������׺����*/
        snprintf(tmppath2, MSD_LOG_PATH_MAX, "%s.%d", path, g_log.msd_log_num-1);
        rename(path, tmppath2);

        close(g_log.g_msd_log_files[index].fd);
        g_log.g_msd_log_files[index].fd = -1;
        
        if(MSD_OK != msd_log_reset_fd(index))
        {
            MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
            return MSD_FAILED;               
        }
        
        printf("thread %lu roate the file\n", (unsigned long)pthread_self());
        MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
        //sleep(2);
        return MSD_OK;

    }
    
    return MSD_NONEED;

}
/**
 * ����: ��־д�룬�̰߳汾
 * ����: @level�� @fmt , @...
 * ����:
 *      1. ÿ��д��ǰ���ᴥ��rotate��� 
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_log_write(int level, const char *fmt, ...)
{
    struct tm   tm; /* time struct */
    int         pos;
    int         end;
    va_list     ap;
    time_t      now;
    int         index;
    int         rotate_result;
    char        log_buffer[MSD_LOG_BUFFER_SIZE] = {0};
    
    if(!g_log.msd_log_has_init)
    {
        return MSD_ERR;
    }

    /* only write the log whose level low than system initial log level */
    if(level > g_log.msd_log_level)
    {
        return MSD_NONEED;
    }
    /*
    if(msd_get_map_mem() != MSD_OK)
    {
        return MSD_FAILED;
    }
    */ 
    now = time(NULL);
    /*localtime_r() ����������ʱ��timepת��Ϊ�û�ָ����ʱ����ʱ�䡣���������Խ����ݴ洢���û��ṩ�Ľṹ���С�*/
    localtime_r(&now, &tm);

    pos = sprintf(log_buffer, "[%04d-%02d-%02d %02d:%02d:%02d][%lu][%5s]",
                tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, pthread_self(),
                msd_log_level_name[level]);

    /*�ӹ���־����*/
    va_start(ap, fmt);
    end = vsnprintf(log_buffer+pos, MSD_LOG_BUFFER_SIZE-pos, fmt, ap);
    va_end(ap);

    /* Խ�紦�� */
    if(end >= (MSD_LOG_BUFFER_SIZE-pos))
    {
        log_buffer[MSD_LOG_BUFFER_SIZE-1] = '\n';
        memset(log_buffer+MSD_LOG_BUFFER_SIZE-4, '.', 3);
        end = MSD_LOG_BUFFER_SIZE-pos-1;
    }
    else
    {
        log_buffer[pos+end] = '\n';
    }    

    index = g_log.msd_log_multi? level:0;

    /*�쳣����*/
    if(g_log.g_msd_log_files[index].fd == -1)
    {
        //MSD_LOCK_LOCK(g_log.msd_log_rotate_lock);
        if(MSD_OK == msd_log_reset_fd(index))
        {
            return MSD_FAILED;
        }
        //MSD_LOCK_UNLOCK(g_log.msd_log_rotate_lock);
    }


    if(MSD_NONEED == (rotate_result = msd_log_rotate(g_log.g_msd_log_files[index].fd, (const char*)g_log.g_msd_log_files[index].path, level)))
    {
        if(write(g_log.g_msd_log_files[index].fd, log_buffer, end + pos + 1) != (end + pos + 1))/* +1 ��Ϊ��'\n' */            
        {
            fprintf(stderr,"write log to file %s failed: %s\n", g_log.g_msd_log_files[index].path, strerror(errno));
            return MSD_FAILED;
        }
    }
    else if(MSD_OK == rotate_result)
    {
        /* ����MSD_OK ˵�����ɱ�����ִ����roate����д����� */
        if(write(g_log.g_msd_log_files[index].fd, log_buffer, end + pos + 1) != (end + pos + 1))       
        {
            fprintf(stderr,"write log to file %s failed: %s\n", g_log.g_msd_log_files[index].path, strerror(errno));
            return MSD_FAILED;
        }      
    }
    else
    {
        /*do nothing*/
        return MSD_FAILED;
    }

    return MSD_OK;
}

#endif
#ifdef __MSD_LOG_TEST_MAIN__

void test(void *arg)
{
    int j=0;
    for(j=0; j < 1; j++)
    {
        MSD_FATAL_LOG("%s", "chil"); //һ����60���ֽ�
    }
}

int main()
{
    /******** ��ͨ���� **********/
    /*
    printf("%s\n",MSD_OK_STATUS);
    printf("%s\n",MSD_FAILED_STATUS);

    msd_boot_notify(0,"hello %s","world");
    msd_boot_notify(0,"hello world hello world hello world hello world hello world hello world hello world hello world");
    msd_boot_notify(1,"hello world hello world hello world hello world hello world hello world hello world hello world");
    
 
    MSD_BOOT_SUCCESS("This is ok:%s", "we'll continue");
    MSD_BOOT_SUCCESS("This is ok:%s", "This is a very long message which will extended the limit of the screen");
    */
    
    /* 
    msd_log_init("./logs","test.log", MSD_LOG_LEVEL_ALL, 1024*1024*256, 3, 0);
 
    //msd_log_init("./logs","test.log", MSD_LOG_LEVEL_ALL, 1024*1024*256, 3, 1); 
    MSD_FATAL_LOG("Hello %s", "world!");
    MSD_ERROR_LOG("Hello %s", "birth!");
    msd_log_write(MSD_LOG_LEVEL_DEBUG, "hei %s", "a!");
    msd_log_write(MSD_LOG_LEVEL_NOTICE, "hello %s", "b!");
    msd_log_write(MSD_LOG_LEVEL_DEBUG, "morning %s", "again!");
    */
    /*
    srand(time(NULL));
    int i=0;
    while(1)
    {
        msd_log_write(rand()%(MSD_LOG_LEVEL_DEBUG +1),
            "[%s:%d]%s:%d", __FILE__, __LINE__, "Test mossad log", i++);
    }
    */

    /**************����rotate�ڶ����ģʽ�µ��л���������**************/  
    /*
     * ����1:�ļ���С����Ϊ6000��log_num=9��1000�����̲���д��60�ֽ�
     *       �������д����10���ļ����޴��ң��޶�ʧ�����Կ�������ԭʼ
     *       �汾�ȽϷǳ�����
     */
    /*
     * ����2:�ļ���С����Ϊ60000��log_num=9��40�����̲���д��600000�ֽ�
     *       ����̰汾ʱ�������ļ��Դ�����⣬������־�������ȶ���ԭʼ
     *       �汾��־�ļ���Сû�ף���־����Ҳûʲô��
     */
    /*
     * ����3:�ļ���С����Ϊ60000��log_num=9��1000�����̲���д��600000�ֽ�
     *       ����̰汾�ܹ���ǿӦ��������΢�ĳ���д������⣬���ǻ�����־���� 
     *       �ȶ����ȶ���һֱ������10����ԭʼ�汾û�����ˣ�������־��С�Լ�
     *       ��־��������ָ�궼û��
     */
    /*
     * ����4:�ļ���С����Ϊ1G��log_num=9��1000�����̲���д��6000000�ֽ�
     *       ����̰汾Ҳ�е㿸��ס����־roate����1.1G���ң�ԭʼ�汾�Ͳ�����  
     */
    /*
     * ����5:�ļ���С����Ϊ6000000��log_num=9��40�����̲���д��600000*2�ֽ�
     *       ���Զ���̰汾�е�����������������rotate����־��������˼·
     *       1.��������������OK�����Լ�д��֮�����
     *       2.ֱ�ӽ���������NONEED��Ȼ��д��  
     *       ���۵�һ�ָ����ϽϺ�
     */
    /*
    MSD_BOOT_SUCCESS("the programe start");
    msd_log_init("./logs","test.log", MSD_LOG_LEVEL_ALL, 6000, 9, 0);

    int i = 0;
    int child_cnt = 1000;
    //int child_cnt = 40;
    printf("father start! pid: %d\n", getpid());
    for(i=0; i<child_cnt; i++)
    {
        pid_t pid;
        if((pid = fork()) == 0)
        {
            //printf("I am child:%d \n", getpid());
            int j=0;
            for(j=0; j < 1; j++)
            {
                MSD_FATAL_LOG("%s", "chil"); //һ����60���ֽ�
                //MSD_FATAL_LOG("%s", "chil");
            }
            exit(0);
        }
        else if(pid>0)
        {
            printf("spawn child %d\n", pid);
        }
        else
        {
            printf("spawn error %d\n", pid);
            perror("");
        }
    }

    //�����̵ȴ��ӽ����˳�
    int status;
    for(i=0; i<child_cnt; i++)
    {
        waitpid( -1, &status, 0 );
    }
    printf("father exit! pid: %d, the child cnt: %d\n", getpid(), child_cnt);
 
    msd_log_close();

    if(0 > execl("/bin/ls", "ls", "-l" ,"/home/huaqi/mossad/test/log/logs/", NULL))
    {
        perror("ls error");
    }
    */
    /**************����rotate�ڶ��߳�ģʽ�µ��л���������**************/ 
    /*
     * ����1,2,3,4: �͵�һ�����̵�������ͬ�������߳�
     * ���ۣ��ڼ���������棬Ҳ������ȫ��֤ÿ����־��С�����ǻ����ܹ��ȶ�����־������С
     *       �����ȶ���֤
     */
    MSD_BOOT_SUCCESS("the programe start");
    msd_log_init("./logs","test.log", MSD_LOG_LEVEL_ALL, 1<<30, 9, 0);    
    int i;
    int child_cnt = 1000;
    //int child_cnt = 40;    
    pthread_t thread[child_cnt];               /*�����̺߳�*/

    for(i=0; i<child_cnt; i++)
    {
        pthread_create(&thread[i], NULL, (void *)test, NULL);
        //printf("create thread %ul\n", (unsigned long )thread[i]);
    }    
    //join
    for(i=0; i<child_cnt; i++)
    {
        pthread_join(thread[i], NULL);
    }  
    
    msd_log_close();

    if(0 > execl("/bin/ls", "ls", "-l" ,"/home/huaqi/mossad/test/log/logs/", NULL))
    {
        perror("ls error");
    }    
    
    
    return 0;
}
#endif /* __MSD_LOG_TEST_MAIN__ */

