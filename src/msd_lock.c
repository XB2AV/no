/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_lock.c
 * 
 * Description :  Msd_lock, a generic lock implementation.
 *                �ṩ���������ͣ�pthread_mutex, semaphore, fcntl����������
 *                �Ķ���ӿڶ���ͳһ�ģ��û�ֻ��Ҫ��msd_core.h�ж�����Ҫ���͵ĺ�
 *                #define   MSD_PTHREAD_LOCK_MODE(�Ƽ�)
 *                #define   MSD_SYSVSEM_LOCK_MODE
 *                #define   MSD_FCNTL_LOCK_MODE(�������ڽ���)
 *
 *                ����pthread_mutexģ�����ڶ���̵�����£���Ҫ���������ڹ����ڴ�
 *                ������Ч
 *
 *                ���÷�Χ��
 *                ���̣�MSD_PTHREAD_LOCK_MODE/MSD_SYSVSEM_LOCK_MODE/MSD_FCNTL_LOCK_MODE
 *                �̣߳�MSD_PTHREAD_LOCK_MODE/MSD_SYSVSEM_LOCK_MODE
 *
 *     Created :  Apr 5, 2012
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/
#include "msd_core.h"

#if defined(MSD_PTHREAD_LOCK_MODE)

/**
 * ����: Lock implementation based on pthread mutex.
 * ����: @ppl
 * ����:
 *      1. �����ڴ�֮�ϣ�����lock�����ӹ���֮������̵�ʱ������Ǹ��ӹ������������޷�ʵ��������
 *      2. ���û�������Ч��Χ:���������������ǽ���ר�õģ������ڣ�������Ҳ������ϵͳ��Χ�ڵ�
 *         �����̼䣩������Ҫ�ڶ�������е��߳�֮�乲�������������ڹ����ڴ��д��������������� 
 *         pshared ��������Ϊ PTHREAD_PROCESS_SHARED�� 
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_pthread_lock_init(msd_lock_t **ppl) 
{
    pthread_mutexattr_t     mattr;
    
	/* �����ڴ�֮�ϣ�����lock */
    *ppl = mmap(0, sizeof(msd_lock_t), PROT_WRITE | PROT_READ,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (*ppl == MAP_FAILED)
    {
        fprintf(stderr, "mmap lock failed: %s\n",strerror(errno));
        return MSD_ERR;
    }

    if (pthread_mutexattr_init(&mattr) != 0) 
    {
        return MSD_ERR;
    }

    /* ���û�������Χ: PTHREAD_PROCESS_SHARED */
    if (pthread_mutexattr_setpshared(&mattr, 
            PTHREAD_PROCESS_SHARED) != 0) 
    {
        pthread_mutexattr_destroy(&mattr);
        return MSD_ERR;
    }

    if (pthread_mutex_init(&((*ppl)->mutex), &mattr) != 0) 
    {
        pthread_mutexattr_destroy(&mattr);
        munmap(*ppl, sizeof(msd_lock_t));
        return MSD_ERR;
    }
    pthread_mutexattr_destroy(&mattr);
    //printf("Thread lock init\n");
    return MSD_OK;
}

/**
 * ����: lock
 * ����: @pl
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_pthread_do_lock(msd_lock_t *pl) 
{
    if (pthread_mutex_lock(&(pl->mutex)) != 0) 
    {
        return MSD_ERR;
    }
    return MSD_OK;
}

/**
 * ����: unlock
 * ����: @pl
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_pthread_do_unlock(msd_lock_t *pl) 
{
    if (pthread_mutex_unlock(&(pl->mutex)) != 0) 
    {
        return MSD_ERR;
    }
    return MSD_OK;
}

/**
 * ����: destroy lock
 * ����: @pl
 **/
void msd_pthread_lock_destroy(msd_lock_t *pl) 
{
    pthread_mutex_destroy(&(pl->mutex));
    munmap( pl, sizeof(msd_lock_t) );
}


#elif defined(MSD_SYSVSEM_LOCK_MODE)

#define PROJ_ID_MAGIC   0xCD
union semun
{ 
    int val;                  /* value for SETVAL */
    struct semid_ds *buf;     /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array;    /* array for GETALL, SETALL */
                              /* Linux specific part: */
    struct seminfo *__buf;    /* buffer for IPC_INFO */
}; 

/**
 * ����: Lock implementation based on System V semaphore.
 * ����: @ppl @pathname
 * ����:
 *      1. ���ӹ���֮����ʵ�˴�������MAP_SHARED����MAP_PRIVATE���ǿ��Ե�ԭ������
 *         ����ʹ�õ�ԭ����ͨ���ź�����id�������pthread��ģʽ��ֻ����MAP_SHARED��
 *         �����޷�ʵ��������
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_sysv_sem_init(msd_lock_t **ppl, const char *pathname) 
{
    int rc;
    union semun arg;
    int semid;
    int oflag = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR;
    key_t key = IPC_PRIVATE;
    
	/* �����ڴ�֮�ϣ�����lock */
    *ppl = mmap(0, sizeof(msd_lock_t), PROT_WRITE | PROT_READ,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (*ppl == MAP_FAILED)
    {
        fprintf(stderr, "mmap lock failed: %s\n",strerror(errno));
        return MSD_ERR;
    }
	
    if (pathname) 
    {
        key = ftok(pathname, PROJ_ID_MAGIC);
        if (key < 0) 
        {
            return MSD_ERR;
        }
    }
    /* �ź������� */
    if ((semid = semget(key, 1, oflag)) < 0) 
    {
        return MSD_ERR;
    }
    /* �ź������� */
    arg.val = 1;
    do 
    {
        rc = semctl(semid, 0, SETVAL, arg);
    } while (rc < 0);

    if (rc < 0) 
    {
        do 
        {
            semctl(semid, 0, IPC_RMID, 0);
        } while (rc < 0 && errno == EINTR);
        return MSD_ERR;
    }
    
    /* !! ��id��ֵ��ppl */
    (*ppl)->semid = semid;
    //printf("Sem lock init\n");
    return semid;
}

/**
 * ����: lock
 * ����: @semid
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_sysv_sem_lock(int semid) 
{
    int rc;
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = SEM_UNDO;
    /* P */
    rc = semop(semid, &op, 1);
    return rc;
}

/**
 * ����: unlock
 * ����: @semid
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_sysv_sem_unlock(int semid) 
{
    int rc;
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = SEM_UNDO;
    /* V */
    rc = semop(semid, &op, 1);
    return rc;
}

/**
 * ����: destroy lock
 * ����: @pl, @semid
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
void msd_sysv_sem_destroy(msd_lock_t *pl, int semid) 
{
    int rc;
    do 
    {
        rc = semctl(semid, 0, IPC_RMID, 0);
    } while (rc < 0 && errno == EINTR);
	
	/* �ͷ��� */
	munmap( pl, sizeof(msd_lock_t) );
}

#elif defined(MSD_FCNTL_LOCK_MODE)

#ifndef MAXPATHLEN
#   ifdef PAHT_MAX
#       define MAXPATHLEN PATH_MAX
#   elif defined(_POSIX_PATH_MAX)
#       define MAXPATHLEN _POSIX_PATH_MAX
#   else
#       define MAXPATHLEN   256
#   endif
#endif

static int strxcat(char *dst, const char *src, int size) 
{
    int dst_len = strlen(dst);
    int src_len = strlen(src);
    if (dst_len + src_len < size) 
    {
        memcpy(dst + dst_len, src, src_len + 1);/* ��1����Ҫ����srcĩβ��'\0' */
        return MSD_OK;
    } 
    else 
    {
        memcpy(dst + dst_len, src, (size - 1) - dst_len);
        dst[size - 1] = '\0';
        return MSD_ERR;
    }
}

/**
 * ����: Lock implemention based on 'fcntl'. 
 * ����: @
 * ����:
 *      1. mkstemp������ϵͳ����Ψһ���ļ�������һ���ļ����򿪣�ֻ��һ��������
 *         ��������Ǹ��ԡ�XXXXXX����β�ķǿ��ַ�����mkstemp�������������������
 *         �����滻��XXXXXX������֤���ļ�����Ψһ�ԡ�
 *      2. ��ʱ�ļ�ʹ����ɺ�Ӧ��ʱɾ����������ʱ�ļ�Ŀ¼������������mkstemp����
 *         ��������ʱ�ļ������Զ�ɾ��������ִ���� mkstemp������Ҫ����unlink������
 *         unlink����ɾ���ļ���Ŀ¼��ڣ�����ʱ�ļ�������ͨ���ļ����������з��ʣ�
 *         ֱ�����һ���򿪵Ľ��̹� ���ļ������������߳����˳�����ʱ�ļ����Զ����׵�ɾ����
 * ����: �ɹ��� ʧ�ܣ�
 **/
int msd_fcntl_init(msd_lock_t **ppl, const char *pathname) 
{
    char s[MAXPATHLEN];
    int fd;

    /* �����ڴ�֮�ϣ�����lock�� */
    *ppl = mmap(0, sizeof(msd_lock_t), PROT_WRITE | PROT_READ,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (*ppl == MAP_FAILED)
    {
        fprintf(stderr, "mmap lock failed: %s\n",strerror(errno));
        return MSD_ERR;
    }
    
    strncpy(s, pathname, MAXPATHLEN - 1);
    strxcat(s, ".sem.XXXXXX", MAXPATHLEN);    
    
    /* ����Ψһ���ļ������� */
    fd = mkstemp(s);
    if (fd < 0) 
    {
        return MSD_ERR;
    }
    
    /* ȥ������ */
    unlink(s);

    (*ppl)->fd = fd;
    //printf("Thread lock init\n");
    return fd;
}
/**
 * ����: lock
 * ����: @fd
 *       @flag, ������д��
 * ����: �ɹ��� ʧ�ܣ�
 **/
int msd_fcntl_lock(int fd, int flag) 
{
    int rc;
    struct flock l;

    /*�����ļ�����*/
    l.l_whence  = SEEK_SET;
    l.l_start   = 0;
    l.l_len     = 0;
    l.l_pid     = 0;

    if (flag == MSD_LOCK_RD) 
    {
        l.l_type = F_RDLCK; /* ������ȡ�� */
    } 
    else 
    {
        l.l_type = F_WRLCK; /* ������д�� */
    }
    
    /*F_SETLKW ��F_SETLK ������ͬ�������޷���������ʱ���˵��û�һֱ�ȵ����������ɹ�Ϊֹ��*/
    rc = fcntl(fd, F_SETLKW, &l);
    return rc;
}

/**
 * ����: unlock
 * ����: @fd
 * ����: �ɹ��� ʧ�ܣ�
 **/
int msd_fcntl_unlock(int fd) 
{
    int rc;
    struct flock l;
    l.l_whence = SEEK_SET;
    l.l_start = 0;
    l.l_len = 0;
    l.l_pid = 0;
    l.l_type = F_UNLCK;/* ������ɾ������ */

    rc = fcntl(fd, F_SETLKW, &l);
    return rc;
}

/**
 * ����: destroy lock
 * ����: @pl @fd
 * ����: �ɹ��� ʧ�ܣ�
 **/
void msd_fcntl_destroy(msd_lock_t *pl, int fd) 
{
    close(fd);

    /* �ͷ��� */
	munmap( pl, sizeof(msd_lock_t) );
}
#endif

#ifdef __MSD_LOCK_TEST_MAIN__

static msd_lock_t *msd_lock;/* �� */

void call_lock()
{
    /*
        ������Ч����������ִ��ʱ����Ϊ5��(�������̲���)
        ������������ִ��ʱ����Ϊ10��(����Ч�ˣ���֮��Ĵ�������ٽ���)
    */
    MSD_LOCK_LOCK(msd_lock);
    printf("child call lock %d\n", getpid());
    sleep(5);
    MSD_LOCK_UNLOCK(msd_lock);
}

/*������*/
void msd_test_lock()
{
    int i;
    for(i=0; i<2; i++)
    {
        pid_t pid;
        if((pid = fork()) == 0)
        {
            if(getpid()%2)
            {
                printf("child odd %d\n", getpid());
                call_lock();
            }
            else
            {
                printf("child even %d\n", getpid());
                call_lock();
            }
            exit(0);
        }
        else
        {
            printf("spawn child %d\n", pid);
        }
    }
    //�����̵ȴ��ӽ����˳�
    int status;
    for(i=0; i<2; i++)
    {
        waitpid( -1, &status, 0 );
    }    
}

void call_lock_in_pthread(void *arg)
{
    msd_lock_t *lock = (msd_lock_t *)arg;
    MSD_LOCK_LOCK(lock);
    printf("child call lock %ul\n", (unsigned long )pthread_self());
    sleep(5);
    MSD_LOCK_UNLOCK(lock);
}

void msd_test_lock_in_pthread()
{
    int i;
    pthread_t thread[2];               /*�����̺߳�*/

    for(i=0; i<2; i++)
    {
        pthread_create(&thread[i], NULL, (void *)call_lock_in_pthread, msd_lock);
        printf("create thread %ul\n", (unsigned long )thread[i]);
    }    
    //join
    for(i=0; i<2; i++)
    {
        pthread_join(thread[i], NULL);
    }   
    printf("Master thread exit!\n");
}
 
int main()
{
    if (MSD_LOCK_INIT(msd_lock) != 0) 
    {
        return MSD_ERR;
    }
    
    /* �������ڶ�����е�Ч�� */
    //msd_test_lock();

    /* �������ڶ��߳��е�Ч�� */
    msd_test_lock_in_pthread();
    
    MSD_LOCK_DESTROY(msd_lock);
    return MSD_OK;
}
#endif
