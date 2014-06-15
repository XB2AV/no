/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Mossad.c
 * 
 * Description :  Mossad main logic��
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/
#include "msd_core.h"

/* ������ʽ */
typedef enum start_mode{
    PROGRAM_START,
    PROGRAM_STOP,
    PROGRAM_RESTART
}start_mode_t;

/* �������б� */
static struct option const long_options[] = {
    {"config",  required_argument, NULL, 'c'},
    {"help",    no_argument,       NULL, 'h'},
    {"version", no_argument,       NULL, 'v'},
    {"info",    no_argument,       NULL, 'i'},
    {NULL, 0, NULL, 0}
};

msd_instance_t  *g_ins;
start_mode_t     g_start_mode;
msd_so_func_t    g_so;

/* Master������symsʵ�ֶ�so���������ı�����ʼ�� */
static msd_so_symbol_t syms[] = 
{
    /* symbol_name,     function pointer,         optional */
    {"msd_handle_init",        (void **)&g_so.handle_init,        1}, 
    {"msd_handle_worker_init", (void **)&g_so.handle_worker_init, 1}, 
    {"msd_handle_fini",        (void **)&g_so.handle_fini,        1}, 
    {"msd_handle_open",        (void **)&g_so.handle_open,        1}, 
    {"msd_handle_close",       (void **)&g_so.handle_close,       1}, 
    {"msd_handle_prot_len",    (void **)&g_so.handle_prot_len,    0},  /* ��ѡ */
    {"msd_handle_process",     (void **)&g_so.handle_process,     0},  /* ��ѡ */
    {NULL, NULL, 0}
};

/**
 * ����: ��ʼ��instance
 * ���أ��ɹ���instance ָ��
 **/
static msd_instance_t * msd_create_instance()
{
    msd_instance_t *instance;
    instance = malloc(sizeof(msd_instance_t));
    if(!instance)
    {
        MSD_BOOT_FAILED("Init instance faliled!");
    }
    
    instance->conf_path = NULL;
    instance->pid_file  = NULL;
    
    if(!(instance->conf = malloc(sizeof(msd_conf_t))))
    {
        MSD_BOOT_FAILED("Init malloc conf faliled!");
    }    

    MSD_LOCK_INIT(instance->thread_woker_list_lock);
    MSD_LOCK_INIT(instance->client_conn_vec_lock);

    //instance->log        = NULL;
    instance->pool       = NULL;
    instance->master     = NULL;
    instance->sig_worker = NULL;
    
    return instance;
}

/**
 * ����: ����instance
 **/
static void msd_destroy_instance(msd_instance_t *instance)
{
    msd_str_free(instance->pid_file);
    msd_str_free(instance->so_file);

    /* �������� */
    msd_str_free(instance->conf_path);
    msd_conf_free(instance->conf);
    free(instance->conf);
    MSD_INFO_LOG("Conf destroy!");
    
    /* ������־ */
    msd_log_close();
    MSD_INFO_LOG("Log destroy!");
    
    /* �����̳߳� */
    msd_thread_pool_destroy(instance->pool);

    /* ����master */
    msd_master_destroy(instance->master);
    
    /* �������ź��߳� */
    free(instance->sig_worker);
    MSD_INFO_LOG("Instance destroy!");
    
    /* ����so��� */
    msd_unload_so(&(instance->so_handle));              
    MSD_INFO_LOG("So destroy!");
    
    /* ������ */
    MSD_LOCK_DESTROY(instance->thread_woker_list_lock);
    MSD_LOCK_DESTROY(instance->client_conn_vec_lock);

    MSD_INFO_LOG("Mossad is closed! Bye bye!");
}

/**
 * ����: ��ӡ"����"��Ϣ
 **/
static void msd_print_info() 
{
    printf("[%s]: A async network server framework.\n"
           "Version: %s\n"
           "Copyright(c): hq, hq_cml@163.com\n"
           "Compiled at: %s %s\n", MSD_PROG_TITLE, MOSSAD_VERSION, 
           __DATE__, __TIME__);
}

/**
 * ����: ��ӡ"�汾"��Ϣ
 **/
static void msd_print_version()
{
    printf("Version: %s\n", MOSSAD_VERSION);
}

/**
 * ���ܣ���ӡusage
 **/
static void msd_usage(int status) 
{
    if (status != EXIT_SUCCESS) 
    {
        fprintf(stderr, "Try '%s --help' for more information.\n", MSD_PROG_NAME);
    } 
    else 
    {
        printf("Usage:./%6.6s [--config=<conf_file> | -c] [start|stop|restart]\n"
               "%6.6s         [--version | -v]\n"
               "%6.6s         [--help | -h]\n", MSD_PROG_NAME, " ", " ");
    }
}

/**
 * ���ܣ�����ѡ��
 **/
static void msd_get_options(int argc, char **argv) 
{
    int c;
    while ((c = getopt_long(argc, argv, "c:hvi", 
                    long_options, NULL)) != -1) 
    {
        switch (c) 
        {
        case 'c':
            g_ins->conf_path = msd_str_new(optarg);
            break;
        case 'h':
            msd_usage(EXIT_SUCCESS);
            exit(EXIT_SUCCESS);
            break;
        case 'i':
            msd_print_info();
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            msd_print_version();
            exit(EXIT_SUCCESS);
            break;            
        default:
            msd_usage(EXIT_FAILURE);
            exit(1);
        }
    }

    if (optind + 1 == argc) 
    {
        if (!strcasecmp(argv[optind], "stop")) 
        {
            g_start_mode = PROGRAM_STOP;
        } 
        else if (!strcasecmp(argv[optind], "start")) 
        {
            g_start_mode = PROGRAM_START;
        }
        else if (!strcasecmp(argv[optind], "restart")) 
        {
            g_start_mode = PROGRAM_RESTART;
        }        
        else 
        {
            msd_usage(EXIT_FAILURE); 
            exit(1);
        }
    } 
    else if (optind == argc) 
    {
        g_start_mode = PROGRAM_START;
    } 
    else 
    {
        msd_usage(EXIT_FAILURE);
        exit(1);
    }
}

/*------------------------ Main -----------------------*/
int main(int argc, char **argv)
{
    int pid,i;
    char **saved_argv;
    
    g_ins = msd_create_instance();
    
    msd_get_options(argc, argv);
    
    /* ��ʼ��conf */
    if(!g_ins->conf_path)
    {
        if(!(g_ins->conf_path = msd_str_new("./mossad.conf")))
        {
            MSD_ERROR_LOG("Init Conf_path Faliled!");
            MSD_BOOT_FAILED("Init Conf_path Faliled!");
        }
    }    
    if(MSD_OK != msd_conf_init(g_ins->conf, g_ins->conf_path->buf))
    {
        MSD_ERROR_LOG("Init Conf Faliled!");
        MSD_BOOT_FAILED("Init Conf Faliled!");
    }
    MSD_BOOT_SUCCESS("Mossad Begin To Init");
    MSD_BOOT_SUCCESS("Init Configure File");

    /* ��ʼ��log */
    if (msd_log_init(msd_conf_get_str_value(g_ins->conf, "log_path", "./"),
            msd_conf_get_str_value(g_ins->conf, "log_name", MSD_PROG_NAME".log"),
            msd_conf_get_int_value(g_ins->conf, "log_level", MSD_LOG_LEVEL_ALL),
            msd_conf_get_int_value(g_ins->conf, "log_size", MSD_LOG_FILE_SIZE),
            msd_conf_get_int_value(g_ins->conf, "log_num", MSD_LOG_FILE_NUM),
            msd_conf_get_int_value(g_ins->conf, "log_multi", MSD_LOG_MULTIMODE_NO)) < 0) 
    {
        MSD_ERROR_LOG("Init Log Failed");
        MSD_BOOT_FAILED("Init Log Failed");
    }
    MSD_BOOT_SUCCESS("Init Log");
    MSD_INFO_LOG("Mossad Begin To Init");
    MSD_INFO_LOG("Init Conf Success");
    MSD_INFO_LOG("Init log Success");  

    /* Daemon */
    if(msd_conf_get_int_value(g_ins->conf, "daemon", 1))
    {
        //msd_daemonize(0, 1);
        msd_daemonize(0, 0);
        //fprintf(stderr, "Can you see me?\n");
    }
    
    /* ��ȡpid */
    g_ins->pid_file = msd_str_new(msd_conf_get_str_value(g_ins->conf, "pid_file", "/tmp/mossad.pid"));
    if((pid = msd_pid_file_running(g_ins->pid_file->buf)) == -1) 
    {
        MSD_ERROR_LOG("Checking Running Daemon:%s", strerror(errno));
        MSD_BOOT_FAILED("Checking Running Daemon:%s", strerror(errno));
    }
    
    /* �������������������start����stop */
    if (g_start_mode == PROGRAM_START) 
    {
        MSD_BOOT_SUCCESS("Begin To Start");
        MSD_INFO_LOG("Begin To Start");
 
        if (pid > 0) 
        {
            MSD_ERROR_LOG("The mossad have been running, pid=%u", (unsigned int)pid);
            MSD_BOOT_FAILED("The mossad have been running, pid=%u", (unsigned int)pid);
        }
    
        if ((msd_pid_file_create(g_ins->pid_file->buf)) != 0) 
        {
            MSD_ERROR_LOG("Create pid file failed: %s.Path:%s", strerror(errno), g_ins->pid_file->buf);
            MSD_BOOT_FAILED("Create pid file failed: %s", strerror(errno));
        }
        MSD_BOOT_SUCCESS("Create Pid File");
        MSD_INFO_LOG("Create Pid File");
    } 
    else if(g_start_mode == PROGRAM_STOP) 
    {
        MSD_BOOT_SUCCESS("Begin To Stop");
        MSD_INFO_LOG("Begin To Stop");
        
        if (pid == 0) 
        {
            MSD_ERROR_LOG("No mossad daemon running now");
            MSD_BOOT_FAILED("No mossad daemon running now");
        }

        /* 
         * ����pid_file�ļ����ص�pid������SIGTERM�źţ���ֹ���� 
         **/
        if (kill(pid, SIGTERM) != 0) 
        {
            fprintf(stderr, "kill %u failed\n", (unsigned int)pid);
            exit(1);
        }
        MSD_BOOT_SUCCESS("The Mossad Process Stop! Pid:%u", (unsigned int)pid);
        return 0;
    } 
    else if(g_start_mode == PROGRAM_RESTART)
    {
        /* �ر� */
        MSD_BOOT_SUCCESS("Begin To Restart");
        MSD_INFO_LOG("Begin To Restart");
        
        if (pid == 0) 
        {
            MSD_ERROR_LOG("No mossad daemon running now");
            MSD_BOOT_FAILED("No mossad daemon running now");
        }
        if (kill(pid, SIGTERM) != 0) 
        {
            fprintf(stderr, "kill %u failed\n", (unsigned int)pid);
            exit(1);
        }
        MSD_BOOT_SUCCESS("The Mossad Process Stop! Pid:%u", (unsigned int)pid);

        for(i=0; i<20; i++)
        {
            //msd_thread_sleep(1);
            write(1, ".", 1);
        }
        write(1, "\n", 1);
        
        /* ���� */
        if ((msd_pid_file_create(g_ins->pid_file->buf)) != 0) 
        {
            MSD_ERROR_LOG("Create pid file failed: %s", strerror(errno));
            MSD_BOOT_FAILED("Create pid file failed: %s", strerror(errno));
        }
        MSD_BOOT_SUCCESS("Create Pid File");
        MSD_INFO_LOG("Create Pid File");

    }
    else 
    {
        msd_usage(EXIT_FAILURE);
        return 0;
    }    
    
    /* ����so��̬�⣬�û����߼���д����so�� */
    g_ins->so_file = msd_str_new(msd_conf_get_str_value(g_ins->conf, "so_file", NULL));
    if (msd_load_so(&(g_ins->so_handle), syms, g_ins->so_file->buf) < 0) 
    {
        MSD_ERROR_LOG("Load so file %s", g_ins->so_file->buf ? g_ins->so_file->buf : "(NULL)");
        MSD_BOOT_FAILED("Load so file %s", g_ins->so_file->buf ? g_ins->so_file->buf : "(NULL)");
    }
    g_ins->so_func = &g_so;
    
    if (g_ins->so_func->handle_init) 
    {
        /* ����handle_init */
        if (g_ins->so_func->handle_init(g_ins->conf) != MSD_OK) 
        {
            MSD_ERROR_LOG("Invoke hook handle_init in master");
            MSD_BOOT_FAILED("Invoke hook handle_init in master");
        }
    }
    MSD_BOOT_SUCCESS("Load So File");
    MSD_INFO_LOG("Load So File Success");
    
    /* �ſ���Դ���� */
    msd_rlimit_reset();
    MSD_BOOT_SUCCESS("Reset Limit");
    MSD_INFO_LOG("Reset Limit Success");

    /* �����ź�ע�ἰ��ʼ���źŴ����߳� */
    if(MSD_OK != msd_init_private_signals())
    {
        MSD_ERROR_LOG("Init private signals failed");
        MSD_BOOT_FAILED("Init private signals failed");
    }
    MSD_BOOT_SUCCESS("Register Private Signals");
    MSD_INFO_LOG("Register Private Signals Success");
    
    if(MSD_OK != msd_init_public_signals())
    {
        MSD_ERROR_LOG("Init Public Signals Failed");
        MSD_BOOT_FAILED("Init Public Signals Failed"); 
    }
    MSD_BOOT_SUCCESS("Start Public Signals Thread");
    MSD_INFO_LOG("Start Public Signals Thread Success");
    
    /* ��ʼ���̳߳� */
    if(!(g_ins->pool = msd_thread_pool_create(
            msd_conf_get_int_value(g_ins->conf, "worker_num", 5),
            msd_conf_get_int_value(g_ins->conf, "stack_size", 10*1024*1024),
            msd_thread_worker_cycle)))
    {   
        MSD_ERROR_LOG("Create threadpool failed");
        MSD_BOOT_FAILED("Create threadpool failed");
    }
    MSD_BOOT_SUCCESS("Create Threadpool");
    MSD_INFO_LOG("Create Threadpool Success");

    /* �޸Ľ��̵�Title */
    saved_argv = msd_set_program_name(argc, argv, msd_conf_get_str_value(g_ins->conf, "pro_name", "Mossad"));

    MSD_BOOT_SUCCESS("Mossad Begin To Run. Program name:%s", msd_conf_get_str_value(g_ins->conf, "pro_name", "Mossad"));
    MSD_INFO_LOG("Mossad Begin To Run Program name:%s", msd_conf_get_str_value(g_ins->conf, "pro_name", "Mossad"));
    /* �ض���STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO */
    int org_fd = msd_redirect_std();
    fprintf(stderr, "You will never see me!\n");
    
    /* Master��ʼ���� */ 
    if(MSD_OK != msd_master_cycle())
    {
        /* �ָ���׼��� */
        dup2(org_fd, STDOUT_FILENO);
        MSD_ERROR_LOG("Create Master Failed");
        MSD_BOOT_FAILED("Create Master Failed");
    }
    
    msd_daemon_argv_free(saved_argv);
    msd_destroy_instance(g_ins);
    return 0;
}

