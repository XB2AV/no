/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Mossad.c
 * 
 * Description :  Mossad main logic��
 * 
 *     Created :  Apr 11, 2012 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
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
    {"msd_handle_init",     (void **)&g_so.handle_init,       1}, 
    {"msd_handle_fini",     (void **)&g_so.handle_fini,       1}, 
    {"msd_handle_open",     (void **)&g_so.handle_open,       1}, 
    {"msd_handle_close",    (void **)&g_so.handle_close,      1}, 
    {"msd_handle_input",    (void **)&g_so.handle_input,      0},  /* ��ѡ */
    {"msd_handle_process",  (void **)&g_so.handle_process,    0},  /* ��ѡ */
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

    instance->log       = NULL;
    instance->pool      = NULL;
    instance->master    = NULL;
    
    return instance;
}

/**
 * ����: ����instance
 **/
static void msd_destroy_instance()
{
    //TODO
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
        printf("Usage:./%6.6s [--config=<conf_file> | -c] [start|stop]\n"
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
    int pid;
    g_ins = msd_create_instance();
    
    msd_get_options(argc, argv);
    
    /* ��ʼ��conf */
    if(!g_ins->conf_path)
    {
        if(!(g_ins->conf_path = msd_str_new("./mossad.conf")))
        {
            MSD_BOOT_FAILED("Init conf_path faliled!");
        }
    }    
    if(MSD_OK != msd_conf_init(g_ins->conf, g_ins->conf_path->buf))
    {
        MSD_BOOT_FAILED("Init conf faliled!");
    }
    MSD_BOOT_SUCCESS("Init conf success");

    /* ��ʼ��log */
    if (msd_log_init(msd_conf_get_str_value(g_ins->conf, "log_path", "./"),
            msd_conf_get_str_value(g_ins->conf, "log_name", MSD_PROG_NAME".log"),
            msd_conf_get_int_value(g_ins->conf, "log_level", MSD_LOG_LEVEL_ALL),
            msd_conf_get_int_value(g_ins->conf, "log_size", MSD_LOG_FILE_SIZE),
            msd_conf_get_int_value(g_ins->conf, "log_num", MSD_LOG_FILE_NUM),
            msd_conf_get_int_value(g_ins->conf, "log_multi", MSD_LOG_MULTIMODE_NO)) < 0) 
    {
        MSD_BOOT_FAILED("Init log failed");
    }
    MSD_BOOT_SUCCESS("Init log success");
    MSD_INFO_LOG("Init conf success");
    MSD_INFO_LOG("Init log success");  

    /* Daemon */
    if(msd_conf_get_int_value(g_ins->conf, "daemon", 1))
    {
        msd_daemonize(0, 1);
        fprintf(stderr, "Can you see me?\n");
    }
    
    /* ��ȡpid */
    g_ins->pid_file = msd_str_new(msd_conf_get_str_value(g_ins->conf, "pid_file", "/tmp/mossad.pid"));
    if((pid = msd_pid_file_running(g_ins->pid_file->buf)) == -1) 
    {
        MSD_BOOT_FAILED("Checking running daemon:%s", strerror(errno));
    }
    
    /* �������������������start����stop */
    if (g_start_mode == PROGRAM_START) 
    {
        MSD_BOOT_SUCCESS("Mossad begin to start");
        MSD_INFO_LOG("Mossad begin to run");
 
        if (pid > 0) 
        {
            MSD_BOOT_FAILED("The mossad have been running, pid=%u", (unsigned int)pid);
        }
    
        if ((msd_pid_file_create(g_ins->pid_file->buf)) != 0) 
        {
            MSD_BOOT_FAILED("Create pid file failed: %s", strerror(errno));
        }
        MSD_BOOT_SUCCESS("Create pid file");
        MSD_INFO_LOG("Create pid file");
    } 
    else if(g_start_mode == PROGRAM_STOP) 
    {
        MSD_BOOT_SUCCESS("Mossad begin to start");
        MSD_INFO_LOG("Mossad begin to run");
        
        if (pid == 0) 
        {
            MSD_BOOT_FAILED("No mossad daemon running now");
        }

        /* 
         * ����pid_file�ļ����ص�pid������SIGKILL�źţ���ֹ���� 
         * SIGQUIT��ʹ�ý��ս��̲���coredump��Ϊ�˷�ֹ��ᣬ������SIGKILL
         **/
        if (kill(pid, SIGKILL) != 0) 
        {
            fprintf(stderr, "kill %u failed\n", (unsigned int)pid);
            exit(1);
        }
        MSD_BOOT_SUCCESS("The mossad process stop! Pid:%u", (unsigned int)pid);
        return 0;
    } 
    else if(g_start_mode == PROGRAM_RESTART)
    {
        //TODO
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
        MSD_BOOT_FAILED("Load so file %s", g_ins->so_file->buf ? g_ins->so_file->buf : "(NULL)");
    }
    //TODO g_ins->so_handle��Ҫ�еط��ͷ�
    g_ins->so_func = &g_so;
    
    if (g_ins->so_func->handle_init) 
    {
        /* ����handle_init */
        if (g_ins->so_func->handle_init(NULL) != MSD_OK) 
        {
            MSD_BOOT_FAILED("Invoke hook handle_init in master");
        }
    }

    
    /* �ſ���Դ���� */
    msd_rlimit_reset();
    
    /* ��ʼ���̳߳� */
    if(!(g_ins->pool = msd_thread_pool_create(
            msd_conf_get_int_value(g_ins->conf, "worker_num", 10),
            msd_conf_get_int_value(g_ins->conf, "stack_size", 10*1024*1024),
            msd_thread_worker_cycle)))
    {        
        MSD_BOOT_FAILED("Create threadpool failed");
    }
 
    
    /* �����ź�ע�ἰ��ʼ���źŴ����߳� */


    /* �û��Զ����ʼ�ص� */

    
    /* �޸Ľ��̵�Title */


    /* Master��ʼ���� */ 
    if(MSD_OK != msd_master_cycle())
    {
        MSD_BOOT_FAILED("Create master failed");
    }
    printf("before sleep\n");
    msd_thread_sleep(60);
    msd_destroy_instance();
    return 0;
}




 












