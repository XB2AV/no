/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_string.c 
 * 
 * Description :  Msd_string, a C dynamic strings library.
 *                Derived from redis.
 *
 *        ע�� :  �����ڲ�����msd_str_expand_room�ĺ��������ص�msd_str_t *����
 *                ��һ���µ�ַ��������Ҫ����ʽ�����£�
 *                g_instance->conf_path = msd_str_cpy(g_instance->conf_path,
 *                                                    "./mossad.conf");
 * 
 *     Created :  Mar 18, 2012
 *     Version :  0.0.1 
 *      
 *      Author :  HQ 
 *     Company :  Qh 
 *
 **/

#include "msd_core.h"
 
/* Ĭ�Ϸָ��ַ���'\t',' ','\r','\n' */
static const unsigned char g_default_ifs[256] = {[9]=1, [10]=1, [13]=1, [32]=1};
 
/**
 * ����:����ַ������� 
 **/
static inline size_t msd_str_len(const msd_str_t *pstr)
{
    if(pstr)  
        return pstr->len;
    else    
        return -1; 
}

/**
 * ����:����ַ�����ʣ������(free) 
 **/
static inline size_t msd_str_avail(const msd_str_t *pstr)
{
    if(pstr)  
        return pstr->free;
    else    
        return -1;
}

/*
 * ����: ����ַ����ṹһ������Ŀռ� 
 * ע��: ����������״̬���ַ�����Ч��
 *       ���ڱ��ͷŵ��ִ������ش����ֵ
 **/
static inline size_t msd_str_alloc_size(const msd_str_t *pstr)
{
    return sizeof(msd_str_t)+pstr->len+pstr->free+1;
}

/**
 * ����:��ʼ���ַ��� 
 * ����:@init:��ʼ�ַ��� 
 *      @init_len:��ʼ�ַ������� 
 * ����:
 *      1. ��init��ʾ���ַ�������ʼ��һ���´�
 *      2. �˺��������⣬��Ϊ���init_len���ԣ�������ַ���"�ն�"
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t* msd_str_newlen(const void *init, size_t init_len)
{
    msd_str_t *pstr;

    if(init)
        pstr = malloc(sizeof(msd_str_t)+init_len+1);
    else
        pstr = calloc(1,sizeof(msd_str_t)+init_len+1);
    
    if(!pstr) 
        return NULL;

    pstr->len  = init_len;
    pstr->free = 0;

    if(init_len && init) 
        memcpy(pstr->buf, init, init_len);
    
    pstr->buf[init_len] = '\0';
    
    return pstr;
}

/**
 * ����:��ʼ���ַ��� 
 * ����:@init:��ʼ�ַ��� 
 * ����:��init��ʾ���ַ�������ʼ��һ���´�
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t* msd_str_new(const void *init)
{
    size_t init_len = (init==NULL)?0:strlen(init);
    return msd_str_newlen(init, init_len);
}

/**
 * ����:��ʼ�����ַ��� 
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t* msd_str_new_empty(void)
{
    return msd_str_newlen("", 0);
}    

/**
 * ����:�ַ���duplicate 
 * ����:@init:��ʼ�ַ��� 
 * ����:��init��ʾ���ַ�������¡һ���´�
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t* msd_str_dup(msd_str_t *init)
{
    if(*(init->buf))
        return msd_str_new((const void *)init->buf);
    else
        return NULL;
}

/**
 * ����:�ͷ��ַ��� 
 * ����:@str 
 **/
void msd_str_free(msd_str_t *pstr)
{
    if(pstr)
    {
        pstr->buf[0] = '\0';
        pstr->len    = 0;
        pstr->free   = 0;
        free(pstr);
    }
}

/**
 * ����:����ַ��� 
 * ����:@str 
 * ����:����գ����ͷ�
 **/
void msd_str_clear(msd_str_t *pstr)
{
    pstr->free   += pstr->len;
    pstr->len    = 0;
    pstr->buf[0] = '\0';
}

/**
 * ����:�ַ������� 
 * ����:@pstr:��ʼ�ַ���ָ�� 
 *      @add_len:���ݵ��� 
 * ����:
 *      1.���free�ռ��㹻����ֱ���˳�
 *      2.���free�ռ䲻����������������2��new_len
 *      3.ִ����ú�����len�ֶβ���仯���仯����free
 *      4.����realloc���ԣ��µĵ�ַ���ܺ�pstr��ͬ
 *      5.����realloc���ԣ������ͷ�ԭ����pstr(�Զ��ͷ�)
 * ����:�ɹ�:�ַ���ָ��(�������µ�ַ);ʧ��:NULL
 **/
static msd_str_t* msd_str_expand_room(msd_str_t *pstr, size_t add_len)
{
    msd_str_t *pstr_new = NULL;
    size_t cur_free     = msd_str_avail(pstr);
    size_t cur_len      = msd_str_len(pstr);
    size_t new_len      = 0;

    if(cur_free >= add_len)
        return pstr;
    
    new_len = cur_len+add_len;
    if(new_len < MSD_STRING_MAX_PREALLOC)
        new_len *= 2;
    else
        new_len += MSD_STRING_MAX_PREALLOC;

    pstr_new = realloc(pstr, sizeof(msd_str_t)+new_len+1);
    if(NULL == pstr_new)
    {
        //TODO log
        //danger: perhaps cause mem leak
        return NULL;
    }
    else if(pstr_new != pstr)
    {
        //TODO log
    }

    pstr_new->free = new_len-cur_len;
    return pstr_new;
}


/**
 * ����:�ַ���len���� 
 * ����:@pstr:�ַ���ָ�� 
 *      @incr:len����ֵ 
 * ����:
 *      1.һ�������ַ���������һ������֮��,len��֮����
 *      2.incr�����Ǹ�ֵ���Ӷ��ﵽ�ַ���rtrim��Ŀ��
 *      3.һ��ͺ���msd_str_expand_room()���̶�ģʽʹ��
 *      
 *          oldlen = msd_str_len(pstr); 
 *          pstr   = msd_str_expand_room(s, BUFFER_SIZE); 
 *          nread = read(fd, pstr->buf+oldlen, BUFFER_SIZE); 
 *          ... check for nread <= 0 and handle it ... 
 *          msd_str_incr_len(pstr, nhread);
 **/
void msd_str_incr_len(msd_str_t *pstr, int incr)
{
    assert(pstr->free >= incr);
    pstr->len  += incr;
    pstr->free -= incr;
    assert(pstr->free >= 0);
    pstr->buf[pstr->len] = '\0';
}

/**
 * ����:�ַ���ƴ�� 
 * ����:@pstr:��ʼ�ַ��� 
 *      @t:��ƴ�Ӵ� 
 *      @len:ƴ�ӳ��� 
 * ����:
 *      1.msd_str_expand_room����һ����Ļ�����(free����)
 *      2.���ص�pstr_new���ܺ�pstr��ͬ
 * ����:�ɹ�:���ַ���ָ��;ʧ��:NULL
 **/
msd_str_t *msd_str_cat_len(msd_str_t *pstr, const void *t, size_t len)
{
    size_t cur_len = msd_str_len(pstr);
    msd_str_t *pstr_new = msd_str_expand_room(pstr, len);
    
    if(!pstr_new)
        return NULL;
    memcpy(pstr_new->buf+cur_len, t, len);
    pstr_new->len = cur_len+len;
    pstr_new->free -= len;
    pstr_new->buf[cur_len+len] = '\0';
    
    return pstr_new;
}

/**
 * ����:�ַ���ƴ�� 
 * ����:@pstr:��ʼ�ַ��� 
 *      @t:��ƴ�Ӵ� 
 * ����:�ɹ�:���ַ���ָ��;ʧ��:NULL
 **/
msd_str_t *msd_str_cat(msd_str_t *pstr, const void *t)
{
    return msd_str_cat_len(pstr, t, strlen(t));
}

/**
 * ����:�ַ���ƴ�� 
 * ����:@init:��ʼ�ַ��� 
 *      @init_len:��ʼ�ַ������� 
 * ����:��init��ʾ���ַ�������ʼ��һ���´�
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t *msd_str_cat_msd_str(msd_str_t *pstr, msd_str_t *pstr1)
{
    return msd_str_cat_len(pstr, pstr1->buf, pstr1->len);
}

/**
 * ����:�ַ������� 
 * ����:@pstr:��ʼ�ַ��� 
 *      @t:�������� 
 *      @len:������������ 
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t *msd_str_cpy_len(msd_str_t *pstr, const void *t, size_t len)
{
    size_t totlen = pstr->free+pstr->len;
    if(totlen < len)
    {
        /* maybe the pstr is not the orginal one */
        pstr = msd_str_expand_room(pstr, len-pstr->len);
        if(!pstr)
            return NULL;
        /* after expand room, the free/len maybe changed */
        totlen = pstr->free+pstr->len;
    }
    memcpy(pstr->buf, t, len);
    pstr->len = len;
    pstr->free = totlen-len;
    pstr->buf[len] = '\0';
    return pstr;
}

/**
 * ����:��ʼ���� 
 * ����:@pstr:��ʼ�ַ��� 
 *      @t:�������� 
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t *msd_str_cpy(msd_str_t *pstr, const void *t)
{
    return msd_str_cpy_len(pstr, t, strlen(t));
}

/**
 * ����:��ʽ��ƴ���ַ��� 
 * ����:@pstr:��ʼ�ַ��� 
 *      @fmt:��ʽ
 *      @...:��������
 * ����:
 *      1. ����̽��buf���˵ĳ��ȣ�Ȼ�����ƴ��
 *      2. ����vsnprintf�����ԣ�������Ҫѡ�����ڶ�λ��Ϊ
 *         ������ж��Ƿ���乻���㹻�ĳ��� 
 * ˵��:
 *      1. vsnprintf���Զ���ĩβ����'\0',��ռ�����һ���ֽ�
 *      2. vsnprintf������Ҫ��ӡ���ַ�����������������'\0'
 *         (����len�����ƣ�����ʵ�ʴ�ӡ��û����ô��)
 *      3. vsnprintf���len�ĳ���С����Ҫ��ӡ�ĳ��ȣ���ֻ��
 *         ����len-1���ֽڣ����һ������'\0'
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t * msd_str_cat_sprintf(msd_str_t *pstr, const char *fmt, ...)
{
    va_list ap;
    va_list ap_cpy;
    char    *buf = NULL;
    size_t  buf_len = 16;

    va_start(ap, fmt);
    while(1)
    {
        /* try time and time again */
        if(!(buf = malloc(buf_len)))
            return NULL;
        buf[buf_len-2] = '\0'; //penult
        va_copy(ap_cpy, ap);   //backup ap
        vsnprintf(buf, buf_len, fmt, ap_cpy);
        if(buf[buf_len-2] != '\0')
        {
            /* not enough */
            free(buf);
            buf_len *= 2;
            continue;
        }
        break;
    }
    va_end(ap);
    pstr = msd_str_cat(pstr, buf);
    free(buf);
    return pstr;
}

/**
 * ����:��ʽ���ַ��� 
 * ����:@pstr:��ʼ�ַ��� 
 *      @fmt:��ʽ
 *      @...:��������
 * ����:
 *      1. ����̽��buf���˵ĳ��ȣ�Ȼ����п���
 *      2. ����vsnprintf�����ԣ�������Ҫѡ�����ڶ�λ��Ϊ
 *         ������ж��Ƿ���乻���㹻�ĳ��� 
 * ˵��:
 *      1. vsnprintf���Զ���ĩβ����'\0',��ռ�����һ���ֽ�
 *      2. vsnprintf������Ҫ��ӡ���ַ�����������������'\0'
 *         (����len�����ƣ�����ʵ�ʴ�ӡ��û����ô��)
 *      3. vsnprintf���len�ĳ���С����Ҫ��ӡ�ĳ��ȣ���ֻ��
 *         ����len-1���ֽڣ����һ������'\0'
 * ����:�ɹ�:�ַ���ָ��;ʧ��:NULL
 **/
msd_str_t * msd_str_sprintf(msd_str_t *pstr, const char *fmt, ...)
{
    va_list ap;
    va_list ap_cpy;
    char    *buf = NULL;
    size_t  buf_len = 16;

    va_start(ap, fmt);
    while(1)
    {
        /* try time and time again */
        if(!(buf = malloc(buf_len)))
            return NULL;
        buf[buf_len-2] = '\0'; //penult
        va_copy(ap_cpy, ap);   //backup ap
        vsnprintf(buf, buf_len, fmt, ap_cpy);
        if(buf[buf_len-2] != '\0')
        {
            /* not enough */
            free(buf);
            buf_len *= 2;
            continue;
        }
        break;
    }
    va_end(ap);
    pstr = msd_str_cpy(pstr, buf);
    free(buf);
    return pstr;
}

/**
 * ����:trim
 * ����:@pstr 
 *      @cset,��Ҫtrim���ַ����� 
 * ����:
 **/
void msd_str_trim(msd_str_t *pstr, const char *cset)
{
    char *start, *end, *sp, *ep;
    size_t len;

    sp=start=pstr->buf;
    ep=end  =pstr->buf+msd_str_len(pstr)-1;
    while(sp<=end && strchr(cset, *sp))
        sp++;
    while(ep>=start && strchr(cset, *ep))
        ep--;
    len = (sp>ep)? 0:((ep-sp)+1);
    if(pstr->buf != sp)
        memmove(pstr->buf, sp, len);
    pstr->buf[len] = '\0';
    pstr->free = pstr->free+(pstr->len-len);
    pstr->len = len;
}

/**
 * ����:�ַ�������������Χ�����ಿ���˳� 
 * ����:@pstr 
 *      @start, ��ʼ��Χ, [0, len-1] 
 *      @end,   ������Χ, [0, len-1] 
 * ����:
 *      ͨ�����ڴ��ַ���������������֮����иģʽ��
 *      buflen = xxx; 
 *      memcpy(buf, pstr->buf, buflen);
 *      msd_str_range(pstr, buflen, msd_str_len(pstr)-1);
 *      ....handle buf...
 * ����:
 *      �ɹ���0��ʧ�ܣ�-1��
 **/
int msd_str_range(msd_str_t *pstr, int start, int end)
{
    size_t new_len;
    size_t cur_len = pstr->len;

    if(start<0 || start>(cur_len-1))
        return MSD_ERR;
    if(end<0   || end>(cur_len-1))
        return MSD_ERR;
    if(end<start)
        return MSD_ERR;

    new_len = end-start+1;
    memmove(pstr->buf, pstr->buf+start, new_len);
    pstr->buf[new_len] = '\0';
    pstr->free = pstr->free+pstr->len-new_len;
    pstr->len  = new_len;
    return MSD_OK;
}

/**
 * ����:ת����Сд 
 * ����:@char *s
 **/
void msd_str_tolower(char *s)
{
    int len=strlen(s);
    int i;
    for(i=0; i<len; i++)
        s[i] = tolower(s[i]);
}

/**
 * ����:ת���ɴ�д 
 * ����:@char *s
 **/
void msd_str_toupper(char *s)
{
    int len=strlen(s);
    int i;
    for(i=0; i<len; i++)
        s[i] = toupper(s[i]);
}

/**
 * ����:�ַ����Ƚ� 
 * ����:@char *s1 
 *      @char *s2 
 * ����:int
 **/
int msd_str_cmp(const char *s1, const char *s2)
{
    size_t len1, len2, minlen;
    int cmp;

    len1 = strlen(s1);
    len2 = strlen(s2);
    minlen = (len1<len2)? len1:len2;
    cmp = memcmp(s1, s2, minlen);
    if (0==cmp)
        return len1 - len2;
    return cmp;
}

/**
 * ����: string explode
 * ����:@ buf���и���ַ���
 *      @ field�и���ɺ�ÿ�ݵ��׵�ַ
 *      @ n���и�ķ���
 *      @ const unsigned char *ifs��ָ���ķָ���
 * ����:
 *      1.��buf�и�����n�ݣ�ÿ�ݶ���\0��β��ÿ�ݵ��׵�ַ����field
 * ����: ��ʵ�и�ķ���
 * ע��: 
 *      1.msd_str_t->buf����ֱ�Ӵ��룬����ɿ׶�
 **/
int msd_str_explode(unsigned char *buf, unsigned char *field[], 
                    int n, const unsigned char *ifs)
{
    int i=0,j,len;
    unsigned char *tempifs;

    /* when ifs is NULL, use the default ifs. else , if the first byte of ifs is not NULL
     * use the specified ifs.otherwise ,rerurn error
     */
    if(NULL == ifs)
    {
        ifs = g_default_ifs;
    }
    else if(*ifs)
    {
        tempifs = (unsigned char *)alloca(256);/*��ջ������ռ�*/
        memset((void*)tempifs, 0, 256);
        while(*ifs)
        {
            tempifs[*ifs++] = 1;
        }
        ifs = tempifs;
    }
    else
    {
        return MSD_ERR;
    }

    i = 0;
    while(1)
    {
        /* trim the leading separators */
        while(ifs[*buf])
        {
            buf++;
        }

        if(!*buf)/* �ַ������� */
        {
            break;
        }

        field[i++] = buf;

        if(i >= n)
        {
            /* process the last field. */
            len = strlen((char *)buf);
            j = 0;
            while(!ifs[*buf] && j<len)
            {
                j++;
                buf++;
            }
            *buf = '\0';
            break;
        }

        while(*buf && !ifs[*buf])
        {
            /* *buf is not null and is not separator */
            buf++;
        }

        if(!*buf)/* �ַ������� */
        {
            break;
        }

        *buf++ = '\0';
    }
    return i;
}

#ifdef __MSD_STRING_TEST_MAIN__
void msd_str_dump(msd_str_t *pstr)
{
    printf("The string data : %s\n", pstr->buf);
    //printf("The string size of msd_str_t : %d\n", (int)sizeof(msd_str_t)); #8
    printf("The string size: %d\n", (int)msd_str_alloc_size(pstr));
    printf("The string len: %d\n", (int)msd_str_len(pstr));
    printf("The string free: %d\n", (int)msd_str_avail(pstr));
    //printf("The size of pstr %d:\n\n", (int)sizeof(pstr)); #8
    if(msd_str_alloc_size(pstr) != msd_str_len(pstr)
            + msd_str_avail(pstr) + sizeof(msd_str_t) +1)
    {
        printf("Error:Something error!\n");
    }
    if(strlen(pstr->buf) != msd_str_len(pstr))
    {
        printf("Error:Hole exist!\n");
        printf("strlen(pstr->buf):%d\n", strlen(pstr->buf));
        printf("msd_str_len(pstr):%d\n", msd_str_len(pstr));
    }
    printf("\n");
}

int main()
{
    int i,j;
    msd_str_t *pstr = NULL;
    msd_str_t *pstr_new = NULL;
    char buf[100];
    memset(buf, 0, 100); 
    /*
    pstr = msd_str_newlen("hello",10);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*
    pstr = msd_str_newlen("hello",strlen("hello"));
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*
    pstr = msd_str_new("hello");
    pstr_new = msd_str_dup(pstr);
    msd_str_dump(pstr_new);
    msd_str_free(pstr);
    msd_str_free(pstr_new);
    */ 
    /*
    pstr = msd_str_new_empty();
    msd_str_dump(pstr);
    pstr_new = msd_str_dup(pstr);
    //msd_str_dump(pstr_new);#segment fault
    msd_str_free(pstr);
    msd_str_free(pstr_new);
    */ 
    
    //test mem leak
    for(i=0; i<10000000; i++)
    {
        pstr = msd_str_new("hello");
        msd_str_free(pstr);
    }
    printf("sleep 100s\n");
    sleep(100);
    
    /*
    pstr = msd_str_new("hello");
    msd_str_dump(pstr);
    msd_str_clear(pstr);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /* 
    pstr = msd_str_new("hello");
    msd_str_dump(pstr);
    msd_str_expand_room(pstr, 100);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*
    pstr = msd_str_new("hello");
    msd_str_expand_room(pstr, 100);
    i = read(0, pstr->buf+pstr->len, 100);
    printf("read len:%d\n", i); 
    msd_str_incr_len(pstr, i);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */ 
    /*    
    pstr = msd_str_new("hello");
    pstr = msd_str_cat(pstr, " world!");  
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*
    pstr = msd_str_new("hello");
    pstr_new = msd_str_new(" world!");  
    pstr = msd_str_cat_msd_str(pstr, pstr_new);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    msd_str_free(pstr_new);
    */
    /*
    pstr = msd_str_new("hello");
    pstr_new = msd_str_new("hello");  
    pstr = msd_str_cpy(pstr, "abcdefghigklmn");
    pstr_new = msd_str_cpy(pstr_new, "a");
    msd_str_dump(pstr);
    msd_str_dump(pstr_new);
    msd_str_free(pstr);
    msd_str_free(pstr_new);
    */
    /*
    pstr = msd_str_new("hello");
    pstr = msd_str_cat_sprintf(pstr, " %s, I am %s, I'm %d years old"
                , "every one", "hq", 28);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*
    pstr = msd_str_new("abcde");
    pstr = msd_str_sprintf(pstr, "hello %s, I am %s, I'm %d years old"
                , "every one", "hq", 28);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*  
    pstr = msd_str_new("   abcde    ");
    msd_str_dump(pstr);
    msd_str_trim(pstr, " ");
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*
    pstr = msd_str_new("   abcde    ");
    msd_str_dump(pstr);
    msd_str_range(pstr, 3, 7);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*    
    pstr = msd_str_new("abcdefghigklmn");
    msd_str_dump(pstr);
    i=5;
    memcpy(buf, pstr->buf, i);
    printf("read : %s\n", buf);
    msd_str_range(pstr, i, msd_str_len(pstr)-1);
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /* 
    pstr = msd_str_new("abcde");
    pstr = msd_str_sprintf(pstr, "hello %s, I am %s, I'm %d years old"
                , "every one", "hq", 28);
    msd_str_dump(pstr);
    msd_str_toupper(pstr->buf); 
    msd_str_dump(pstr);
    msd_str_tolower(pstr->buf); 
    msd_str_dump(pstr);
    msd_str_free(pstr);
    */
    /*
    pstr = msd_str_new("abcde");
    pstr_new = msd_str_new("abcdE");
    printf("%d\n", msd_str_cmp(pstr->buf, pstr_new->buf));
    printf("%d\n", msd_str_cmp(pstr_new->buf, pstr->buf));
    msd_str_free(pstr);
    msd_str_free(pstr_new);
    */
    strcpy(buf, "abcd\t\nefgh ijkl \rmn");
    //strcpy(buf, "abcd");
    i=5;
    unsigned char *filed[i];
    i = msd_str_explode(buf, filed, i, NULL);
    printf("Rreturn:%d\n", i);
    for(j=0; j<i; j++)
    {
        printf("data:%s, len:%d\n", filed[j], strlen(filed[j]));
    }
    return 0;
}
#endif
