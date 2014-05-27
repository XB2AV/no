/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *     Filename:  Msd_presure_test.c 
 * 
 *  Description:  Mossad ѹ�����Թ���. 
 * 
 *      Created:  May 26, 2014 
 *      Version:  0.0.1 
 * 
 *       Author:  HQ 
 *      Company:  Qihoo 
 *
 **/

/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include "msd_ae.h"
#include "msd_dlist.h"
#include "msd_string.h"
#include "msd_anet.h"
*/
#include "msd_core.h"

typedef struct config {
    msd_ae_event_loop   *el;                /* ae��� */
    char                *hostip;            /* ����Ŀ��IP */
    int                  hostport;          /* ����Ŀ��˿� */
    int                  num_clients;       /* Ԥ��ͬһʱ��ͻ����� */
    int                  live_clients;      /* ʵ��Ŀǰ��Ծ�Ŀͻ��� */
    int                  requests;          /* �����������������������ʱָ�� */
    int                  requests_issued;   /* �Ѿ�����ȥ�������� */
    int                  requests_finished; /* ʵ����ɵ��������� */
    int                  quiet;             /* �Ƿ�ֻ��ʾqps��Ĭ�Ϸ� */
    int                  keep_alive;        /* 1 = keep alive, 0 = reconnect (default 1) */
    int                  loop;              /*  */
    long long            start;             /* ����ʼʱ�� */
    long long            total_latency;     /* �����ܺ�ʱ(����) */
    char                *title;             /* �������� */
    msd_dlist_t         *clients;           /* client���� */
} conf_t;

typedef struct client_st {
    int             fd;         /* client��fd */
    msd_str_t      *obuf;       /* client��sendbuf */
    unsigned int    written;    /* bytes of 'obuf' already written */
    unsigned int    read;       /* bytes already be read */
    long long       start;      /* start time of request */
    long long       latency;    /* request latency */
} client_t;

static conf_t g_conf;

static void client_done(client_t *c);

/* ��õ�ǰ΢���� */
static long long ustime() 
{
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

/* ��õ�ǰ������ */
static long long mstime() 
{
    struct timeval tv;
    long long mst;

    gettimeofday(&tv, NULL);
    mst = ((long)tv.tv_sec) * 1000;
    mst += tv.tv_usec / 1000;
    return mst;
}

/* �ͷ�client */
static void free_client(client_t *c) 
{
    msd_dlist_node_t *node;
    msd_ae_delete_file_event(g_conf.el, c->fd, MSD_AE_WRITABLE);
    msd_ae_delete_file_event(g_conf.el, c->fd, MSD_AE_READABLE);
    close(c->fd);
    msd_str_free(c->obuf);
    --g_conf.live_clients;
    node = msd_dlist_search_key(g_conf.clients, c);
    assert(node != NULL);
    msd_dlist_delete_node(g_conf.clients, node);
    free(c);
}

/* ��ȡ�ص����� */
static void read_handler(msd_ae_event_loop *el, int fd, void *priv, int mask) 
{
    client_t *c = (client_t *)priv;
    int nread;
    char buffer[4096];

    nread = read(fd, buffer, 4096);
    if (nread == -1) 
    {
        if (errno == EAGAIN) 
        {
            /* ��ʱ���ɶ�����ȡnonblock��fdʱ�����ܻ����� */
            return;
        }
        fprintf(stderr, "Error: %s\n", strerror(errno));
        exit(1);
    } 
    else if (nread == 0) 
    {
        fprintf(stderr, "Error: %s\n", "Server close connection.");
        exit(1);
    }

    c->read += nread;
    /* ����ȡ�������ݣ���д��ȥ���������ʱ�����������һ������ */
    if (c->read == c->obuf->len) 
    {
        c->latency = ustime() - c->start;
        ++g_conf.requests_finished;
        client_done(c);
    }
}

/* д�ص�����(��������)
 * һֱд��ֱ������д��(client->obuf)ȫ��д���ˣ�������read�ص�
 */
static void write_handler(msd_ae_event_loop *el, int fd, void *priv, int mask) 
{
    client_t *c = (client_t *)priv;

    /* Initialize request when nothing was written. */
    if (c->written == 0) 
    {
        if (g_conf.requests_issued++ >= g_conf.requests) 
        {
            free_client(c);
            return;
        }

        c->start = ustime();
        c->latency = -1;
    }

    if (c->obuf->len > c->written) 
    {
        char *ptr = c->obuf->buf + c->written;
        int nwritten = write(c->fd, ptr, c->obuf->len - c->written);
        if (nwritten == -1) 
        {
            /* When a process writes to a socket that has received an RST, the SIGPIPE signal is sent to the process. */
            if (errno != EPIPE) 
            {
                fprintf(stderr, "write failed:%s\n", strerror(errno));
            }
            free_client(c);
            return;
        }
        c->written += nwritten;

        if (c->obuf->len == c->written) 
        {
            /* ɾ��д�¼� */
            msd_ae_delete_file_event(g_conf.el, c->fd, MSD_AE_WRITABLE);
            /* �������¼� */
            msd_ae_create_file_event(g_conf.el, c->fd, MSD_AE_READABLE, read_handler, c);
        }
    }

}

/* ������һ��client */
static client_t *create_client(const char *content) 
{
    client_t *c = (client_t *)malloc(sizeof(*c));
    c->fd = msd_anet_tcp_nonblock_connect(NULL, g_conf.hostip, g_conf.hostport);
    if (c->fd == MSD_ERR) 
    {
        fprintf(stderr, "Connect to %s:%d failed\n", g_conf.hostip, g_conf.hostport);
        exit(1);
    }
    c->obuf = msd_str_new(content);
    c->written = 0;
    c->read = 0;
    msd_ae_create_file_event(g_conf.el, c->fd, MSD_AE_WRITABLE, write_handler, c);
    msd_dlist_add_node_tail(g_conf.clients, c);
    ++g_conf.live_clients;
    return c;
}

/* �����ڶ�������n��client */
static void create_missing_clients(client_t *c) 
{
    int n = 0;
    while (g_conf.live_clients < g_conf.num_clients) 
    {
        create_client(c->obuf->buf);
        /* listen backlog is quite limited on most system */
        if (++n > 64) 
        {
            usleep(50000);
            n = 0;
        }
    }
}

/* ����client,Ȼ������һ��д/������ */
static void reset_client(client_t *c) 
{
    msd_ae_delete_file_event(g_conf.el, c->fd, MSD_AE_WRITABLE);
    msd_ae_delete_file_event(g_conf.el, c->fd, MSD_AE_READABLE);
    msd_ae_create_file_event(g_conf.el, c->fd, MSD_AE_WRITABLE, write_handler, c);
    c->written = 0;
    c->read = 0;
}

/* ��client�����һ��д/������֮����� */
static void client_done(client_t *c) 
{
    /* ����ﵽ��Ԥ���������������ֹͣ */
    if (g_conf.requests_finished == g_conf.requests) 
    {
        free_client(c);
        msd_ae_stop(g_conf.el);
        return;
    }

    /* ���keep_alive�������¿�ʼclient��д/������
     * ���!keep_alive,������client,Ȼ��ʼд/������
     */
    if (g_conf.keep_alive) 
    {
        reset_client(c);
    } 
    else 
    {
        --g_conf.live_clients;
        create_missing_clients(c);
        ++g_conf.live_clients;
        free_client(c);
    }
}

/* �ͷ�ȫ��client */
static void free_all_clients() {
    msd_dlist_node_t *node = g_conf.clients->head, *next;
    while (node) 
    {
        next = node->next;
        free_client((client_t *)node->value);
        node = next;
    }
}

/* ��ӡ���Ա��� */
static void show_latency_report(void) 
{
    float reqpersec;

    /* ����ÿ�봦������� */
    reqpersec = (float)g_conf.requests_finished 
        / ((float)g_conf.total_latency / 1000);

    if (!g_conf.quiet) 
    {
        printf("========== %s ==========\n", g_conf.title);
        printf(" All %d requests has send\n", g_conf.requests);        
        printf(" All %d requests completed\n", g_conf.requests_finished);
        printf(" Complete percent:%.2f\n", 100*(float)(g_conf.requests_finished/g_conf.requests));
        printf(" Use time %.2f seconds\n", (float)g_conf.total_latency/1000);
        printf(" Parallel %d clients\n", g_conf.num_clients);
        printf(" keep alive: %d\n", g_conf.keep_alive);
        printf("\n");

        printf("%.2f requests per second\n\n", reqpersec);
    } 
    else 
    {
        printf("%s:%.2f requests per second\n\n", g_conf.title, reqpersec);
    }
}

/* ����ѹ������ */
static void benchmark(char *title, char *content) 
{
    client_t  *c;
    g_conf.title = title;
    g_conf.requests_issued = 0;
    g_conf.requests_finished = 0;

    /* ����ָ����Ŀ��client */
    c = create_client(content);
    create_missing_clients(c);

    g_conf.start = mstime();
    msd_ae_main_loop(g_conf.el);
    g_conf.total_latency = mstime() - g_conf.start;

    show_latency_report();
    free_all_clients();
}

/* ��ʱ��ӡÿ�봦�������� */
static int show_throughput(msd_ae_event_loop *el, long long id, void *priv) 
{
    float dt = (float)(mstime() - g_conf.start) / 1000.0;
    float rps = (float)g_conf.requests_finished / dt;
    printf("%s: %.2f\n", g_conf.title, rps);
    return 250; /* every 250ms */
}

static void usage(int status) 
{
    puts("Usage: benchmark [-h <host>] [-p <port>] "
            "[-c <clients>] [-n requests]> [-k <boolean>]\n");
    puts(" -h <hostname>    server hostname (default 127.0.0.1)");
    puts(" -p <port>        server port (default 8773)");
    puts(" -c <clients>     number of parallel connections (default 50)");
    puts(" -n <requests>    total number of requests (default 10000)");    
    puts(" -k <boolean>     1 = keep alive, 0 = reconnect (default 1)");
    puts(" -q               quiet. Just show QPS values");
    puts(" -l               loop. Run the tests forever");
    puts(" -H               show help information\n");
    exit(status);
}

static void parse_options(int argc, char **argv) 
{
    char c;
    
    while ((c = getopt(argc, argv, "h:p:c:n:k:qlH")) != -1) 
    {
        switch (c) {
        case 'h':
            g_conf.hostip = strdup(optarg);
            break;
        case 'p':
            g_conf.hostport = atoi(optarg);
            break;
        case 'c':
            g_conf.num_clients = atoi(optarg);
            break;
        case 'n':
            g_conf.requests = atoi(optarg);
            break;
        case 'k':
            g_conf.keep_alive = atoi(optarg);
            break;
        case 'q':
            g_conf.quiet = 1;
            break;
        case 'l':
            g_conf.loop = 1;
            break;
        case 'H':
            usage(0);
            break;
        default:
            usage(1);
        }
    }
}

int main(int argc, char **argv) 
{
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    msd_log_init("./logs","bench.log", MSD_LOG_LEVEL_ALL, 1<<30, 9, 0); 
    MSD_INFO_LOG("Pressure Test Begin!");
    
    g_conf.num_clients = 50;
    g_conf.requests = 10000;
    g_conf.live_clients = 0;
    g_conf.keep_alive = 1;
    g_conf.loop = 0;
    g_conf.quiet = 0;
    g_conf.el = msd_ae_create_event_loop();
    //msd_ae_create_time_event(g_conf.el, 1, show_throughput, NULL, NULL);
    g_conf.clients = msd_dlist_init();
    g_conf.hostip = "127.0.0.1";
    g_conf.hostport = 9527;
    
    parse_options(argc, argv);
    
    if (optind < argc) 
    {
        usage(1);
    }

    if (!g_conf.keep_alive) 
    {
        puts("WARNING:\n"
            " keepalive disabled, at linux, you probably need    \n"
            " 'echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse' and \n"
            " 'sudo sysctl -w net.inet.tcp.msl=1000' for Mac OS X\n"
            " in order to use a lot of clients/requests\n");
    }

    do {
        benchmark("Mossad QPS benchmark", "hello mossad");
    } while (g_conf.loop);

    exit(0);
}
