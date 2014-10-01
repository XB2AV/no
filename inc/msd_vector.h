/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_vector.h
 * 
 * Description :  Msd_vector, a vector based on array. Support any type.
 *                ��һ��Ƭ(size*slots)�������ڴ��У�ģ����������Ϊ��
 * 
 *     Created :  Apr 4, 2012
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 *    Modified :  ȥ���˴���bug��iter
 *
 **/

#ifndef __MSD_VECTOR_H_INCLUDED__
#define __MSD_VECTOR_H_INCLUDED__

/*
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MSD_OK       0
#define MSD_ERR     -1
*/

typedef struct msd_vector
{
    unsigned int  count; /* vector��Ԫ�ظ�����Vector��������0��ʼ. count<=slots 
                              * ע��!���ֵ���������!Vector����ȷ��׼ȷ��Ԫ�ظ���!
                              * ��Ϊset��ʱ�򣬿����м���ֿն�������϶�count��Ҫ�������ߵ����
                              * ����������ⲿ����ά��vector��Ԫ����ʵ����������ط���unicorn�ǲ�ͬ�ģ���Ϊ
                              * unicornû��set���������ϣ�count��Vector�����һ����ЧԪ�غ��������!!!
                              */
    unsigned int  slots; /* data�����λ��������������Ԫ�ص������� */
    unsigned int  size;  /* the size of a member��ÿ��Ԫ�صĴ�С */
    void         *data;  /* Ԫ������ */
}msd_vector_t;

typedef int (*msd_vector_cmp_t)(const void *, const void *); 
typedef int (*msd_vector_each_t)(void *, void *);

msd_vector_t *msd_vector_new    (unsigned int slots, unsigned int size);
int           msd_vector_push   (msd_vector_t *v, void *elem);
int           msd_vector_set_at (msd_vector_t *v, unsigned int index, void *data);
void         *msd_vector_get_at (msd_vector_t *v, unsigned int index);
void          msd_vector_free     (msd_vector_t *v);
void         *msd_vector_pop     (msd_vector_t *vec);
void         *msd_vector_top      (msd_vector_t *vec); 
int           msd_vector_sort    (msd_vector_t *vec, msd_vector_cmp_t cmp);
int           msd_vector_each   (msd_vector_t *vec, msd_vector_each_t func, void *data);

#endif /* __MSD_VECTOR_H_INCLUDED__ */

