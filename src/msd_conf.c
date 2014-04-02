/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_conf.c
 * 
 * Description :  Msd_conf, a configure library. Support include and block.
 * 
 *     Created :  Mar 31, 2014 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/

#include "msd_conf.h"
//#include "msd_core.h"
#include "msd_hash.h"
/**
 * ����: str2int
 * ����: @strval
 *       @def,Ĭ��ֵ
 * ����: �ɹ��� 0 ʧ�ܣ�-x
 **/
static int msd_str2int(const char *strval, int def)
{
    if(isdigit(strval[0]) || (strval[0] == '-' && isdigit(strval[1])))
    {
        return strtol(strval, NULL, 10);
    }

    if(!strcasecmp(strval, "on"))
    {
        return 1;
    }
    else if(!strcasecmp(strval, "off"))
    {
        return 0;
    }
    else if(!strcasecmp(strval, "yes"))
    {
        return 1;
    }
    else if(!strcasecmp(strval, "no"))
    {
        return 0;
    }
    else if(!strcasecmp(strval, "true"))
    {
        return 1;
    }
    else if(!strcasecmp(strval, "false"))
    {
        return 0;
    }
    else if(!strcasecmp(strval, "enable"))
    {
        return 1;
    }
    else if(!strcasecmp(strval, "disable"))
    {
        return 0;
    }
    else if(!strcasecmp(strval, "enabled"))
    {
        return 1;
    }
    else if(!strcasecmp(strval, "disabled"))
    {
        return 0;
    }

    return def;
}

/**
 * ����: key_dup
 **/
static void *msd_key_dup(const void *key)
{
    return strdup((char *)key);
}

/**
 * ����: key_cmp
 * ����:
 *      1. ע�ⷵ��ֵ����strcmp�������෴
 * ����: ��ͬ��1 ��ͬ��0
 **/
static int msd_key_cmp(const void *key1, const void *key2)
{
    return strcmp((char *)key1, (char *)key2) == 0;
}

/**
 * ����: free_key
 **/
static void msd_free_key(void *key)
{
    free(key);
}

/**
 * ����: free the hash entry val for hash_table
 * ����: @hash���е�ĳһ��ֵ��ָ��
 * ����:
 *      1. ÿ��block����һ�������conf���ṹ��������Ҫ����msd_conf_free
 **/
static void msd_free_val(void *val)
{
    msd_conf_value_t *cv = (msd_conf_value_t *)val;
    
    if(cv->type == MSD_CONF_TYPE_ENTRY)
    {
        msd_conf_entry_t *ce, *next;
        ce = (msd_conf_entry_t *)cv->value;
        while (ce)/*ɾ������*/
        {
            next = ce->next;
            free(ce->value);
            free(ce);
            ce = next;
        }
        free(cv);
    }
    else if(cv->type == MSD_CONF_TYPE_BLOCK)
    {
        msd_conf_block_t *next_cb, *cb = (msd_conf_block_t *)(cv->value);
        while(cb)
        {
            next_cb = cb->next;
            /*ɾ������(�ݹ�)*/
            msd_conf_free(&cb->block);
            free(cb);
            cb = next_cb;
        }
        free(cv);
    }
    else
    {
        /* never get here */
        assert(1==0);
    }
}

/**
 * ����: parse included file in the conf
 * ����: @conf, conf�ṹ��ַ
 *       @cur_file, ��ǰ�ļ�
 *       @inc_file, �������ļ�
 * ����:
 *      1. ����include_file���з�������ֳ����ļ���·�����ļ���
 *      2. ������·������������ļ�������Ŀ¼����cur_file����
 *      3. �ҵ���include���ļ���Ȼ��conf_init
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
static int msd_conf_parse_include(msd_conf_t *conf, char *cur_file, char *inc_file)
{
    char dirc[PATH_MAX];    /* PATH_MAX 4095 */
    char basec[PATH_MAX];
    char *dname;            /* Ŀ¼�� */
    char *bname;            /* �ļ��� */
    DIR *dir = NULL;
    struct dirent *entry;
    int ret = 0, rc;
    char resolved_path[PATH_MAX];

    strncpy(dirc, (const char *)inc_file, PATH_MAX);
    strncpy(basec, (const char *)inc_file, PATH_MAX);
    dname = dirname(dirc);      /* include <libgen.h>������·����Ŀ¼���� */
    bname = basename(basec);    /* inlcude <libgen.h>������·���е��ļ������� */

    if(!strcmp(bname, ".") || !strcmp(bname, "..")
            || !strcmp(bname, "/"))/* ����ļ�������.����..����/ */
    {
        fprintf(stderr, "invalid include directive:%s\n", inc_file);
        return MSD_ERR;
    }

    if(!(dir = opendir(dname)))
    {
        fprintf(stderr, "opendir %s failed:%s\n", dname, strerror(errno));
        return MSD_ERR;
    }

    while((entry = readdir(dir)))
    {
        char fullpath[PATH_MAX];
#ifdef _DIRENT_HAVE_D_TYPE
        if(entry->d_type == DT_DIR)
        {
            continue;/* skip the directory */
        }
#else
        struct stat st;
        if(lstat((const char *)entry->d_name, &st) != MSD_OK)
        {
            fprintf(stderr, "lstat %s failed:%s\n", entry->d_name, strerror(errno));
            ret = MSD_ERR;
            break;
        }

        if(S_ISDIR(st.st_mode))
        {
            continue;/* skip the directory */
        }
#endif /* _DIRENT_HAVE_D_TYPE */
        
        snprintf(fullpath, PATH_MAX-1, "%s/%s", dname, entry->d_name);

        /*
         * realpath()����������path��ָ�����·��ת���ɾ���·������ڲ���resolved_path��ָ���ַ��������ָ����
         */
        if(!realpath(fullpath, resolved_path))/* realpath, success then return the pointer of resolved_path else return null*/
        {
            fprintf(stderr, "realpath %s failed:%s\n", fullpath, strerror(errno));
            ret = MSD_ERR;
            break;
        }

        if(!strcmp(cur_file, resolved_path))
        {
            /* skip the current file */
            continue;
        }

        /* fnmatch
         * DESCRIPTION
         *      The  fnmatch() function checks whether the string argument matches the pattern
         *      argument, which is a shell wildcard pattern.
         *
         *RETURN VALUE
         *        Zero if string matches pattern, FNM_NOMATCH if there is no match or another
         *        non-zero value if there is an error.

          FNM_PATHNAME
              If this flag is set, match a slash in string only with a slash in pattern and not by an asterisk (*) or  a
              question mark (?) metacharacter, nor by a bracket expression ([]) containing a slash.

          FNM_PERIOD
              If  this  flag  is  set,  a  leading period in string has to be matched exactly by a period in pattern.  A
              period is considered to be leading if it is the first character in string, or if both FNM_PATHNAME is  set
              and the period immediately follows a slash.
         */
        if(!(rc = fnmatch(bname, entry->d_name, FNM_PATHNAME|FNM_PERIOD)))
        {
            if(msd_conf_init(conf, (const char *)fullpath) != MSD_OK)
            {
                ret = MSD_ERR;
            }
        }
        else if(rc == FNM_NOMATCH)
        {
            continue;
        }
        else
        {
            ret = MSD_ERR;
            break;
        }

    }

    closedir(dir);
    return ret;
}

 /**
 * ����: parse the conf file
 * ����: @conf �ṹ��ַ
 *       @resolved_path ��ǰ�ļ�����
 *       @fp ��ǰ�ļ���FILE��ָ��
 *       @bolck ��ǰ�Ƿ�Ϊ��
 * ����:
 *      1. block���λ: 0--is not block  1--is block
 *      2. �����Ӧ��key�����ڣ��������key/val�ԣ�val������block����entry����֮һ
 *      3. �����Ӧ��key���ڣ�����жϸ�key�����ͣ��͵�ǰ�����Ƿ�һ��
 *         ���һ�£���Ὣ��ǰֵ��������ͷ���������һ�£��򱨴�
 * ע�⣺ 
 *         �����ļ���ʽҪ�������block����һ��Ҫ:" block_name { "
 *         'block name' �� '{' ����Ҫ��ͬһ�У����ո����
 * ����: �ɹ���0  ʧ�ܣ�-x
 **/
static int msd_conf_parse(msd_conf_t *conf, char *resolved_path,
        FILE *fp, int block)
{
    int n;
    int ret = 0;
    char buf[MAX_LINE];
    unsigned char *field[2];
    msd_conf_value_t *cv;

    if(!conf->ht)
    {
        conf->ht = msd_hash_create(MSD_HASH_INIT_SLOTS);
        if(!conf->ht)
        {
            perror("hash_create failed");
            return MSD_ERR;
        }

        MSD_HASH_SET_SET_KEY(conf->ht,  msd_key_dup);
        MSD_HASH_SET_FREE_KEY(conf->ht, msd_free_key);
        MSD_HASH_SET_FREE_VAL(conf->ht, msd_free_val);
        MSD_HASH_SET_KEYCMP(conf->ht,   msd_key_cmp);
    }

    while(fgets(buf, MAX_LINE, fp))
    {
        /* get a line */
        n = strlen(buf);
        if(buf[n-1] == '\n' || buf[n-1] == '\r')
        {
            buf[n-1] = '\0';
        }

        if(*buf == '#')
        {
            /* skip the comment */
            continue;
        }
        else if(msd_str_explode((unsigned char *)buf, field, 2, NULL) == 2)
        {
            /* stand line ,such as "port 8000" or "my_block {" .etc */
            int is_block = 0;

            /* process include directive recursly */
            if(!strcmp((char *)field[0], "include"))
            {
                if(msd_conf_parse_include(conf, resolved_path, (char *)field[1]) != MSD_OK)
                {
                    ret = MSD_ERR;
                    goto error;
                }
                continue;
            }

            if(!strcmp((char *)field[1], "{"))
            {
                is_block = 1; /* meet a block */
            }

            /* process a key/value config */
            cv = (msd_conf_value_t *)msd_hash_get_val(conf->ht, (void *)field[0]);

            if(!cv)
            {
                /* not exist, then, add a conf value */
                cv = calloc(1, sizeof(msd_conf_value_t));
                if(!cv)
                {
                    ret = MSD_ERR;
                    goto error;
                }

                /* insert key/value pair into hash table, the cv is the value, 
                 * which will be fiexd later. 
                 */
                if(msd_hash_insert(conf->ht, (void *)field[0], cv) != MSD_OK)
                {
                    free(cv);
                    ret = MSD_ERR;
                    goto error;
                }

                if(!is_block)
                {
                    /* stand value */
                    msd_conf_entry_t *ce = NULL;
                    ce = calloc(1, sizeof(msd_conf_entry_t));
                    if(!ce)
                    {
                        ret = MSD_ERR;
                        goto error;
                    }

                    ce->value = strdup((char *)field[1]);
                    ce->next = NULL;
                    /* fixed the value which is insert into hash just now */
                    cv->type = MSD_CONF_TYPE_ENTRY;
                    cv->value = ce;
                }
                else
                {
                    /* if meet block, then recursily */
                    msd_conf_block_t *cb = NULL;
                    cb = calloc(1, sizeof(msd_conf_block_t));
                    if(!cb)
                    {
                        ret = MSD_ERR;
                        goto error;
                    }
                    
                    /* recursion */
                    if(msd_conf_parse(&cb->block, resolved_path, fp, 1) != MSD_OK)/*����ط������fp��ָ���������һ�ж�ȡ*/
                    {
                        free(cb);
                        ret = MSD_ERR;
                        goto error;
                    }
                    cb->next = NULL;
                    /* fixed the value which is insert into hash just now */
                    cv->type = MSD_CONF_TYPE_BLOCK;
                    cv->value = (void *)cb;
                }
            }/* if(!cv) */
            else /* �Ѿ�������ͬ��key */
            {
                if( cv->type == MSD_CONF_TYPE_ENTRY )
                {
                    msd_conf_entry_t *ce;
                    
                    /* key��Ӧ����entry������˿�����һ��block���򱨴� */
                    if(is_block)
                    {
                        ret = MSD_ERR;
                        goto error;
                    }

                    ce = calloc(1, sizeof(msd_conf_entry_t));
                    if(!ce)
                    {
                        ret = MSD_ERR;
                        goto error;
                    }
                    ce->value = strdup((char *)field[1]);
                    /* insert at before the head */
                    ce->next = (msd_conf_entry_t *)(cv->value);
                    cv->value = ce;
                }
                else
                {
                    msd_conf_block_t *cb = NULL;
                    /* key��Ӧ����block������˿�����һ��entry���򱨴� */
                    if(!is_block)
                    {
                        ret = MSD_ERR;
                        goto error;
                    }

                    cb = calloc(1, sizeof(msd_conf_block_t));
                    if(!cb)
                    {
                        ret = MSD_ERR;
                        goto error;
                    }

                    /* recursion */
                    if(msd_conf_parse(&cb->block, resolved_path, fp, 1) != MSD_OK)/*����ط������fp��ָ���������һ�ж�ȡ*/
                    {
                        free(cb);
                        ret = MSD_ERR;
                        goto error;
                    }
                    /* insert before head */
                    cb->next = (msd_conf_block_t *)(cv->value);
                    cv->value = cb;

                }
                
            }
        }/* if(msd_str_explode(NULL, (unsigned char *)buf, field, 2) == 2) */
        else
        {
            /* not stand line, such as "}" or "\r\n\t " or other single word .etc */
            if(field[0] && field[0][0] == '}')
            {
                if(block)
                {
                    return MSD_OK;
                }
                else
                {
                    ret = MSD_ERR;
                    goto error;
                }
            }
        }
    } /* while(fgets(buf, MAX_LINE, fp)) */

error:
    if(ret == MSD_ERR)
    {
        msd_conf_free(conf);
    }
    return ret;
}

/**
 * ����: free conf �ṹ
 **/
void msd_conf_free(msd_conf_t *conf)
{
    msd_hash_free(conf->ht);
} 
 
/**
 * ����: init conf struct
 * ����: @conf conf�ṹָ��
 *       @filename �����ļ���ַ
 * ע��:
 *      1. before calling this function first time,please initialize 
 *       the msd_conf_t' struct with zero. such as msd_conf_t conf = {};
 * ����: �ɹ���0 ʧ�ܣ�-1
 **/

/* before calling this function first time,please
 * initialize the msd_conf_t' struct with zero.
 * such as msd_conf_t conf = {};
 */
int msd_conf_init(msd_conf_t *conf, const char *filename)
{
    FILE *fp;
    char resolved_path[PATH_MAX];

    if(!realpath(filename, resolved_path))
    {
        fprintf(stderr, "%s\n",strerror(errno));
        return MSD_ERR;
    }

    if(!(fp = fopen(filename, "r")))
    {
        perror("fopen failed");
        return MSD_FAILED;
    }

    if(msd_conf_parse(conf, resolved_path, fp, 0) != 0)
    {
        fclose(fp);
        return MSD_ERR;
    }
    fclose(fp);
    return MSD_OK;
}

/**
 * ����: ����conf��ĳ��key��Ӧ��entry�����е�ÿ��Ԫ�ؽ���ĳ������
 * ����: @conf
 *       @key
 *       @foreach
 *       @userptr
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
static int msd_conf_entry_foreach(msd_conf_t *conf, char *key,
        int (*foreach)(void *key, void* value, void *userptr),
        void *userptr)
{
    msd_conf_entry_t *ce, *next;
    msd_conf_value_t *cv = (msd_conf_value_t *)msd_hash_get_val(conf->ht, key);

    if(!cv)
    {
        return MSD_ERR;
    }

    if(cv->type != MSD_CONF_TYPE_ENTRY)
    {
        return MSD_ERR;
    }

    ce = (msd_conf_entry_t *)cv->value;
    while(ce)
    {
        next = ce->next;
        if(foreach(key, ce->value, userptr) != 0)
        {
            return MSD_ERR;
        }
        ce = next;
    }
    return MSD_OK;
}

/**
 * ����: ����conf��ĳ��key��Ӧ��block�����е�ÿ��Ԫ�ؽ���ĳ������
 * ����: @conf
 *       @key
 *       @foreach
 *       @userptr
 * ����: �ɹ���0 ʧ�ܣ�-x
 **/
int msd_conf_block_foreach(msd_conf_t *conf, char *key,
        int (*foreach)(void *key, msd_conf_block_t* block, void *userptr),
        void *userptr)
{
    msd_conf_block_t *cb, *next;
    msd_conf_value_t *cv = (msd_conf_value_t *)msd_hash_get_val(conf->ht, key);

    if(!cv)
    {
        return MSD_ERR;
    }

    if(cv->type != MSD_CONF_TYPE_BLOCK)
    {
        return MSD_ERR;
    }

    cb = (msd_conf_block_t *)cv->value;
    while(cb)
    {
        next = cb->next;
        if(foreach(key, cb, userptr) != 0)
        {
            return MSD_ERR;
        }
        cb = next;
    }
    return MSD_OK;
}

/**
 * ����: 
 * ����: @
 * ����:
 *      1. 
 * ����: �ɹ���  ʧ�ܣ�
 **/
 
 
 /**
 * ����: 
 * ����: @
 * ����:
 *      1. 
 * ����: �ɹ���  ʧ�ܣ�
 **/
 
 
 /**
 * ����: 
 * ����: @
 * ����:
 *      1. 
 * ����: �ɹ���  ʧ�ܣ�
 **/
#ifdef __MSD_CONF_TEST_MAIN__

static int msd_print_conf(void *key, void *value, void *userptr)
{
    printf("%-20s %-30s\n", (char *)key, (char *)value);
    return 0;
}

static int msd_print_block_conf(void *key, msd_conf_block_t *cb, void *userptr)
{
    printf("%s {\n", (char *)key);
    msd_conf_dump(&cb->block);/*�ݹ�*/
    printf("}\n");
    return 0;
}

static int msd_conf_print_foreach(const msd_hash_entry_t *he, void *userptr)
{
    msd_conf_value_t *cv = (msd_conf_value_t *)he->val;
    if(cv->type == MSD_CONF_TYPE_ENTRY)
    {
        return msd_conf_entry_foreach((msd_conf_t*)userptr, he->key,
                msd_print_conf, NULL);
    }
    else if(cv->type == MSD_CONF_TYPE_BLOCK)
    {
        return msd_conf_block_foreach((msd_conf_t*)userptr, he->key,
                msd_print_block_conf, NULL);
    }
    else
    {
        return MSD_ERR;
    }
}

void msd_conf_dump(msd_conf_t *conf)
{
    msd_hash_foreach(conf->ht, msd_conf_print_foreach, (void*)conf);
}

int main(int argc, char *argv[])
{   
    /*
    printf("%d\n", msd_str2int("abc",0));
    printf("%d\n", msd_str2int("5",0));
    printf("%d\n", msd_str2int("disable",0));
    printf("%d\n", msd_str2int("yes",0));
    */
    msd_conf_t conf = {};// init conf ,gcc�����ַ�ʽ��һ�Ի�����
    if(argc < 2)
    {
        fprintf(stderr, "useage: ./a.out <conf_file>\n");
        exit(1);
    }

    if(msd_conf_init(&conf, argv[1]) != MSD_OK)
    {
        fprintf(stderr, "conf_init error\n");
        exit(1);
    }

    msd_conf_dump(&conf);
    /*
    printf("PORT: %d\n", msd_conf_get_int_value(&conf, "porta", 7777));
    printf("PORT: %d\n", msd_conf_get_int_value(&conf, "log_multi", 7777));
    printf("LOG_NAME: %s\n", msd_conf_get_str_value(&conf, "log_name", "NULL"));
    printf("LOG_NAME: %s\n", msd_conf_get_str_value(&conf, "log_namexx", "NULL"));
    */
    msd_conf_free(&conf);
    
    return 0;
}
#endif /* __MSD_CONF_TEST_MAIN__ */
