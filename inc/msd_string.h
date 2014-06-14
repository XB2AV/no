/*
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_string.h 
 * 
 * Description :  Msd_string, a C dynamic strings library.
 *                Derived from redis.
 *
 *        ע�� :  �����ڲ�����msd_str_expand_room�ĺ��������ص�msd_str_t *����
 *                ��һ���µ�ַ��������Ҫ����ʽ�����£�
 *                g_instance->conf_path = msd_str_cpy(g_instance->conf_path,
 *                                                    "./mossad.conf");
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/

#ifndef __MSD_STRING_H_INCLUDED__ 
#define __MSD_STRING_H_INCLUDED__
 
/*
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
*/
#define MSD_STRING_MAX_PREALLOC (10*1024*1024)

typedef struct _msd_string_s
{
    int  len;    /* ʵ���ַ�������,������'\0' */
    int  free;   /* ���г���,������'\0' */
                 /* ���ϣ�����buf���ܿռ�=len+free+1 */
    char buf[];  /* ���ݿ�ʼָ�� */
}msd_str_t;

msd_str_t* msd_str_new(const void *init);
msd_str_t* msd_str_new_empty(void);
msd_str_t* msd_str_dup(msd_str_t *init);
void       msd_str_free(msd_str_t *pstr);
void       msd_str_clear(msd_str_t *pstr);
void       msd_str_incr_len(msd_str_t *pstr, int incr);
msd_str_t* msd_str_cat_len(msd_str_t **pstr, const void *t, size_t len);
msd_str_t* msd_str_cat(msd_str_t **pstr, const void *t);
msd_str_t* msd_str_cat_msd_str(msd_str_t **pstr, msd_str_t *pstr1);
msd_str_t* msd_str_cpy_len(msd_str_t **pstr, const void *t, size_t len);
msd_str_t* msd_str_cpy(msd_str_t **pstr, const void *t);
msd_str_t* msd_str_cat_sprintf(msd_str_t **pstr, const char *fmt, ...);
msd_str_t* msd_str_sprintf(msd_str_t **pstr, const char *fmt, ...);
void       msd_str_trim(msd_str_t *pstr, const char *cset);
int        msd_str_range(msd_str_t *pstr, int start, int end);
void       msd_str_tolower(char *s);
void       msd_str_toupper(char *s);
int        msd_str_cmp(const char *s1, const char *s2);
int        msd_str_explode(unsigned char *buf, unsigned char *field[], \
           int n, const unsigned char *ifs);
#endif

