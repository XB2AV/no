/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
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
 *     Company :  Qh 
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
    unsigned int  count; /* vector��Ԫ�ظ�����Vector��������0��ʼ����count<=slots-1 
                          * ע��!���ֵ���������!Vector����ȷ��׼ȷ��Ԫ�ظ���!
                          */
    unsigned int  slots; /* data�����λ��������������Ԫ�ص������� */
    unsigned int  size;  /* the size of a member��ÿ��Ԫ�صĴ�С */
    void         *data;  /* Ԫ������ */
}msd_vector_t;

typedef struct msd_vector_iter
{
    unsigned int  pos;  /* �������е����������� */
    msd_vector_t *vec;  /* ��������vector */
    void         *data; /* pos����Ӧ��Ԫ�ص���ʼ��ַ */
}msd_vector_iter_t;


msd_vector_t *msd_vector_new(unsigned int slots, unsigned int size);

int msd_vector_push(msd_vector_t *v, void *data);

int msd_vector_set_at(msd_vector_t *v, unsigned int index, void *data);

void *msd_vector_get_at(msd_vector_t *v, unsigned int index);

void msd_vector_destroy(msd_vector_t *v);

void msd_vector_free(msd_vector_t *v);
 
msd_vector_iter_t *msd_vector_iter_new(msd_vector_t *vec);

int msd_vector_iter_next(msd_vector_iter_t *iter);

int msd_vector_iter_prev(msd_vector_iter_t *iter);

void msd_vector_iter_reset(msd_vector_iter_t *iter);

void msd_vector_iter_destroy(msd_vector_iter_t *iter);

void msd_vector_iter_free(msd_vector_iter_t *iter);

#endif /* __MSD_VECTOR_H_INCLUDED__ */

