/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_daemon.c
 * 
 * Description :  Msd_daemon, daemon implementation.
 * 
 *     Created :  May 15, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/
  
#include "msd_core.h"

/* To change the process title in Linux and Solaris we have to
 * set argv[1] to NULL and to copy the title to the same place
 * where the argv[0] points to. Howerver, argv[0] may be too 
 * small to hold a new title. Fortunately, Linux and Solaris 
 * store argv[] and environ[] one after another. So we should
 * ensure that is the continuous memory and then we allocate
 * the new memory for environ[] and copy it. After this we could
 * use the memory starting from argv[0] for our process title.

 * The Solaris's standard /bin/ps does not show the changed process
 * title. You have to use "/usr/ucb/ps -w" instead. Besides, the 
 * USB ps does not show a new title if its length less than the 
 * origin command line length. To avoid it we append to a new title
 * the origin command line in the parenthesis. 

 * ��������Ľ��ͣ�argv�ĸ���Ԫ��ָ�����ַ�����ĵ�ַ�������ģ�argv������
 * ��������environ����ĵĵ�ַ
 **/

extern char **environ; /* ��������������ﶨ��ͷ���ռ�ģ�main�������صĵ����������� */
static char *arg_start;
static char *arg_end;
static char *env_start;

static void msd_daemon_set_title(const char* fmt, ...);
static char **msd_daemon_argv_dup(int argc, char *argv[]);

/**
 * ����: ���ݳ�������argv��ά����
 * ����: @argc, @argv
 * ����: �ɹ������ݵĵ�ַ, ʧ�ܣ�NULL
 **/
static char **msd_daemon_argv_dup(int argc, char *argv[]) 
{
    arg_start = argv[0];

    arg_end = argv[argc - 1] + strlen(argv[argc - 1]) + 1;
    env_start = environ[0];

    /* �����ڴ棬��argv[]����copyһ���������� */
    char **saved_argv = (char **)malloc((argc + 1) * sizeof(char *));
    saved_argv[argc] = NULL;
    if (!saved_argv) 
    {
        return NULL;
    }

    while (--argc >= 0) 
    {
        saved_argv[argc] = strdup(argv[argc]);
        if (!saved_argv[argc]) 
        {
            msd_daemon_argv_free(saved_argv);
            return NULL;
        }
    }
    return saved_argv;
}

/**
 * ����: �ͷ�����ı��ݳ���argv����
 * ����: @daemon_argv
 **/
void msd_daemon_argv_free(char **daemon_argv) 
{
    int i = 0;
    for (i = 0; daemon_argv[i]; ++i) 
    {
        free(daemon_argv[i]);
    }
    free(daemon_argv);
}

/**
 * ����: �޸Ľ��̵�title
 * ����: @argc, @argv
 * ˵����
 *      1. ���ȱ��ݳ����׵�argv�Ķ�ά���飬Ȼ���titleд�뵽ԭ����argv��Ӧ�Ŀռ���ȥ
 *      2. ���argv[]�Ŀռ仹����������titile���򽫽�������environҲ���ϣ������ȱ���
 * ע��:
 *      Before invoke this function, you should call msd_daemon_argv_dup first.
 *      Otherwise there will be a segment fault!
 *
 * ����: �ɹ������ݵĵ�ַ, ʧ�ܣ�NULL
 **/
static void msd_daemon_set_title(const char* fmt, ...) 
{
	char title[128];
	int i, tlen;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(title, sizeof(title) - 1, fmt, ap);
	va_end(ap);

	tlen = strlen(title) + 1;
    if(tlen>127)
        tlen=127;

	/* ���argv[]�Ŀռ仹���㹻������argv��environ��ַ�������� */
	if (arg_end - arg_start < tlen && env_start == arg_end) 
    {
        //fprintf(stderr, "dup the environ\n");
        /* Need to duplicate environ */
		char *env_end = env_start;
        /* environ is a array of char pointer. 
           Just copy environ strings */
        for (i = 0; environ[i]; ++i) 
        {
            env_end = environ[i] + strlen(environ[i]) + 1;
            environ[i] = strdup(environ[i]);
        }
		arg_end = env_end;
	}

    i = arg_end - arg_start;
    memset(arg_start, 0, i);
    strncpy(arg_start, title, i - 1);/* �۸ĳ�������� */

#ifdef __linux__
    /* printf("the macro __linux__ is defined\n"); */
    /* ���߳����� */
    prctl(PR_SET_NAME, title);
#endif /* __linux__ */
}


/* ���ó������� */
char ** msd_set_program_name(int argc, char *argv[], const char *name)
{
    char **saved_argv = msd_daemon_argv_dup(argc, argv);
    if (!saved_argv) 
    {
        fprintf(stderr, "daemon_argv_dup failed\n");
        exit(1);
    }

    msd_daemon_set_title(name);

    return saved_argv;
}


/**
 * ����: �ض���
 **/
void msd_redirect_std() 
{
    int fd;
    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) 
    {
        /*
         * int dup2(int oldfd, int newfd);
         * dup2 makes newfd be the copy of oldfd, closing newfd first if necessary.
         * ����д����������롢�������Ϣ�������ض���/dev/null
         */
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) 
        {
            close(fd);
        }
    }
}

/**
 * ����: daemon
 * ����: @is_chdir: �Ƿ��л�Ŀ¼
 *       @is_close: �Ƿ��ض���0,1,2
 **/
void msd_daemonize(int is_chdir, int is_redirect) 
{
    /* �����ӽ��̣��������˳� */
    if (fork() != 0) 
    {
        sleep(1);/* ͣ��һ�����Ƴ�����ʱ���ӡ������Ϣ */
        exit(0); /* parent exits */
    } 

    /* ��һ�ӽ��̴����µĻỰ����Ϊ�»Ự�ͽ�������鳤 */
    setsid(); /* Create a new session. */
    
    /* ��һ�ӽ����˳����ڶ��ӽ��̼������ڶ��ӽ��̲����ǻỰ�鳤 */
    if (fork() != 0) 
    {
        exit(0); /* parent exits */
    }     
    
    if (is_chdir)  
    {
        /* �е���Ŀ¼ */
        if (chdir("/") != 0) 
        {
            fprintf(stderr, "chdir failed:%s", strerror(errno));
            exit(1);
        }
    }

    /* �ض���1��2��3 */
    if (is_redirect) 
    {
        msd_redirect_std();
    }
}

/**
 * ����: �޸���Ӳ����
 **/
void msd_rlimit_reset() 
{
    struct rlimit rlim;
    /* RLIMIT_NOFILE ����ָ���˽��̿��Դ򿪵�����ļ����� */
    rlim.rlim_cur = MSD_MAX_FD; /* ������ */
    rlim.rlim_max = MSD_MAX_FD; /* Ӳ���� */
    setrlimit(RLIMIT_NOFILE, &rlim);

    /* RLIMIT_CORE����ָ���˽��̿��Դ��������core�ļ��Ĵ�С�������������Ϊ0�������ܴ����� */
    rlim.rlim_cur = 1 << 29; 
    rlim.rlim_max = 1 << 29;
    setrlimit(RLIMIT_CORE, &rlim);
}

/**
 * ����: �ļ���
 * ����: @fd, @enable 1:���� 0:����
 * ����: �ɹ���0, ʧ�ܣ�-x
 **/
static int msd_pid_file_lock(int fd, int enable) 
{
    struct flock f;
    memset(&f, 0, sizeof(f));
    f.l_type = enable ? F_WRLCK : F_UNLCK;
    /* �����ļ����� */
    f.l_whence = SEEK_SET;
    f.l_start = 0;
    f.l_len = 0;

    if (fcntl(fd, F_SETLKW, &f) < 0) 
    {
        if (enable && errno == EBADF) 
        {
            f.l_type = F_RDLCK;
            if (fcntl(fd, F_SETLKW, &f) >= 0) 
            {
                return MSD_OK;
            }
        }

        fprintf(stderr, "fcntl(F_SETLKW) failed:%s\n", strerror(errno));
        return MSD_ERR;
    }
    return MSD_OK;
}

/**
 * ����: ����pid�ļ�
 * ����: @pid_file
 * ����: �ɹ���0, ʧ�ܣ�-x
 **/
int msd_pid_file_create(char *pid_file) 
{
    int fd, locked, len, ret = MSD_ERR;
    char buf[16];
    mode_t old_mode;

    //��ʱ�ı�umask
    old_mode = umask(022);

    /* O_EXCL ���O_CREAT Ҳ������, ��ָ���ȥ����ļ��Ƿ����. �ļ����������������ļ�, ���򽫵��´��ļ�����. */
    //if ((fd = open(pid_file, O_CREAT | O_RDWR | O_EXCL, 0644)) < 0) 
    if ((fd = open(pid_file, O_CREAT | O_RDWR , 0644)) < 0) 
    {
        perror("open failed");
        goto finish;
    }

    /* ���� */
    if ((locked = msd_pid_file_lock(fd, 1)) < 0) 
    {
        perror("lock failed");
        int saved_errno = errno;
        unlink(pid_file);
        errno = saved_errno;
        goto finish;
    }

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)getpid());
    len = strlen(buf);
    if (write(fd, buf, len) != len) 
    {
        perror("write failed");
        int saved_errno = errno;
        unlink(pid_file);
        errno = saved_errno;
        goto finish;
    }

    ret = 0;

finish:
    if (fd >= 0) 
    {
        int saved_errno = errno;
        /* ���� */
        if (locked >= 0) 
        {
            msd_pid_file_lock(fd, 0);
        }
        close(fd);
        errno = saved_errno;
    }

    //��ԭumask
    umask(old_mode);
    return ret;
}

/**
 * ����: ��ȡpid_file�ļ����Զ����Ľ��̺ŷ����źţ��ж�����
 *       ���pid_file�����ڣ���ֱ�ӷ���0��������ڣ����ǽ��̺�
 *       ��Ӧ�Ľ����ѹ�����ɾ��pid_file�ļ�������0�����򷵻�
 *       ��ʵ����pid
 * ����: @pid_file
 * ˵����
 *       1. �ú������ڳ���ʼ�����ж��Ƿ�������ʵ���Ѿ�������
 * ����: �ɹ���pid�ļ����ݣ�����0, ʧ�ܣ�-1
 **/
pid_t msd_pid_file_running(char *pid_file) 
{
    
    int fd, locked, len;
    char buf[16];
    pid_t pid = (pid_t)-1;

    if ((fd = open(pid_file, O_RDONLY, 0644)) < 0) 
    {
        pid = 0;
        //perror("Open pid_file:");
        goto finish;
    }
    
    /* ���� */
    if ((locked = msd_pid_file_lock(fd, 1)) < 0) 
    {
        goto finish;
    }

    if ((len = read(fd, buf, sizeof(buf) - 1)) < 0) 
    {
        goto finish;
    }

    buf[len] = '\0';
    while (len--) 
    {
        if (buf[len] == '\n' || buf[len] == '\r') 
        {
            buf[len] = '\0';
        }
    }

    pid = (pid_t)strtol(buf, NULL, 10);
    //printf("pid:%d\n", pid);
    if (pid == 0) 
    {
        fprintf(stderr, "PID file [%s] corrupted, removing\n", pid_file);
        unlink(pid_file);
        errno = EINVAL;
        goto finish;
    }
    
    /* If sig is 0, then no signal is sent, but error checking is still performed; 
     * this can be used to check for the existence of a process ID or process group 
     * ID.
     * EPERM,The process does not have permission to send the signal to any of the 
     * target processes.
     **/
    if (kill(pid, 0) != 0 && errno != EPERM) 
    {
        int saved_errno = errno;
		//fprintf(stderr, "kill failed, %d\n", pid);
        unlink(pid_file);
        errno = saved_errno;
        pid = 0;
        goto finish;
    }

finish:
    if (fd >= 0) 
    {
        int saved_errno = errno;
        if (locked >= 0) 
        {
            /* ���� */
            msd_pid_file_lock(fd, 0);
        }
        close(fd);
        errno = saved_errno;
    }
    return pid;
}

#ifdef __MSD_DAEMON_TEST_MAIN__
int main(int argc, char *argv[]) 
{
    //msd_daemonize(1,1);
    msd_daemonize(1,0);
    msd_rlimit_reset();
    
    char **saved_argv = msd_daemon_argv_dup(argc, argv);
    int i = 1;
    printf("pid=%d\n",getpid()); 
    if (!saved_argv) 
    {
        fprintf(stderr, "daemon_argv_dup failed\n");
        exit(1);
    }

    msd_daemon_set_title("%s:%d", "It's a very long process title,"
        "please contact with huaqi", 3);
    while (i-- > 0) 
    {
        sleep(1);
    }
    msd_daemon_argv_free(saved_argv);

    msd_pid_file_create("/tmp/qbench.pid");
    msd_pid_file_running("/tmp/qbench.pid");
    sleep(5);
    printf("\nif the msd_daemonize's seconde argument is 1, you will not see the line\n");
    sleep(20);
    return 0;
}
#endif /* __MSD_DAEMON_TEST_MAIN__ */

