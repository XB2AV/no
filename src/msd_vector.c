/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  /_\  |  _ \ 
 * | |\/| | | | \___ \___ \ //_\\ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *    Filename :  Msd_vector.c
 * 
 * Description :  Msd_vector, a vector based on array. Support any type.
 *                ��һ��Ƭ(size*slots)�������ڴ��У�ģ����������Ϊ��
 * 
 *     Version :  0.0.1 
 * 
 *      Author :  HQ 
 *
 **/

#include "msd_core.h"

/**
 * ����: init vector
 * ����: @vec, @slots, @size
 * ����:
 *      1. ��һ��Ƭ�������ڴ��У�ģ����������Ϊ
 * ����: �ɹ� 0�� ʧ�ܣ�-x
 **/ 
static int msd_vector_init(msd_vector_t *vec, unsigned int slots, unsigned int size) 
{
    if (!slots)
    {
        slots = 16;
    }    
    vec->slots = slots;
    vec->size = size;
    vec->count = 0;
    
    vec->data = calloc(1, vec->slots * vec->size);    
    if (!vec->data) 
    {
        return MSD_ERR;
    }
    
    return MSD_OK;
}

/**
 * ����: Create a vector, and init it
 * ����: @slots, vectorԪ�ظ���
 *       @size, ÿ��Ԫ�صĴ�С
 * ����:
 *      1. 
 * ����: �ɹ���vector�ṹ��ַ��ʧ�ܣ�NULL
 **/
msd_vector_t *msd_vector_new(unsigned int slots, unsigned int size) 
{
    msd_vector_t *vec = (msd_vector_t*)calloc(1, sizeof(*vec));
    if (!vec) 
        return NULL;
        
    if (msd_vector_init(vec, slots, size) < 0) 
    {
        free(vec);
        return NULL;
    }
    
    return vec;
}

/**
 * ����: double the number of the slots available to a vector
 * ����: @vec
 * ����:
 *      1. realloc������Ҫ�ͷ�ԭ���ĵ�ַ
 * ����: �ɹ� 0 ʧ�� -x
 **/ 
static int msd_vector_resize(msd_vector_t *vec) 
{
    void *temp;
    temp = realloc(vec->data, vec->slots * 2 * vec->size);
                
    if (!temp) 
    {
        return MSD_ERR;
    }
    
    vec->data = temp;
    vec->slots *= 2;
    return MSD_OK;
}

/**
 * ����: vector set at
 * ����: @vec, @index, @data
 * ע��:
 *       ��������count�Ĵ���ʱ��������ģ�set_at�п����������!
 * ����: �ɹ� 0 ʧ�� -x
 **/
int msd_vector_set_at(msd_vector_t *vec, unsigned int index, void *data) 
{
    while (index >= vec->slots)
    {
        //TODO vector shoud have a MAX_LENGTH
        /* resize until our table is big enough. */
        if (msd_vector_resize(vec) != MSD_OK) 
        {
            return MSD_ERR;
        }
    }
    
    memcpy((char *)vec->data + (index * vec->size), data, vec->size);
    ++vec->count; /* ����! */
    return MSD_OK;
} 
 
/**
 * ����: ��vectorβ�����һ��Ԫ��
 * ����: @
 *       @
 * ����:
 *      1. vector������0��ʼ
 *      2. �������������!����Ϊ������count!
 * ����: �ɹ� 0 ʧ�� -x
 **/  
int msd_vector_push(msd_vector_t *vec, void *data) 
{
    if (vec->count == (vec->slots - 1)) 
    {
        if (msd_vector_resize(vec) != MSD_OK) 
        {
            return MSD_ERR;
        }
    }
    
    return msd_vector_set_at(vec, vec->count, data);
}

 /**
 * ����: get a value random
 * ����: @vec , @index
 **/
void *msd_vector_get_at(msd_vector_t *vec, unsigned int index) 
{
    if (index >= vec->slots) 
        return NULL;

    return (char *)vec->data + (index * vec->size);
}

/**
 * ����: destroy the vector
 * ����: @vec
 * ����:
 *      1. ���ͷ�data��vec����ṹ���ͷ�
 **/
void msd_vector_destroy(msd_vector_t *vec) 
{
    if (vec->data) 
        free(vec->data);
    
    vec->slots = 0;
    vec->count = 0;
    vec->size = 0;
    vec->data = NULL;
}

/**
 * ����: destroy the vector
 * ����: @vec
 * ����:
 *      1. �ͷ�data��Ȼ���ͷ�vec����
 **/
void msd_vector_free(msd_vector_t *vec) 
{
    msd_vector_destroy(vec);
    free(vec);
}
 
/**
 * ����: init vector
 * ����: @iter
 *       @vec
 * ����:
 *      1. ��ʼ����ɺ󣬽�iterָ����ʼԪ��
 * ����: �ɹ� 0 ʧ�� -x
 **/  
static int msd_vector_iter_init(msd_vector_iter_t *iter, msd_vector_t *vec) 
{
    iter->pos = 0;
    iter->vec = vec;
    iter->data = NULL;
    /* ָ����ʼλ�� */
    return msd_vector_iter_next(iter);
}

/**
 * ����: new vector
 * ����: @vec
 * ����: �ɹ� iterָ�� ʧ�� NULL
 **/ 
msd_vector_iter_t *msd_vector_iter_new(msd_vector_t *vec) 
{
    msd_vector_iter_t *iter;
    
    iter = (msd_vector_iter_t *)calloc(1, sizeof(*iter));
    if (!iter) 
    {
        return NULL;
    }
    
    if (msd_vector_iter_init(iter, vec) != MSD_OK) 
    {
        free(iter);
        return NULL;
    }
    
    return iter;
}

/**
 * ����: iter move next
 * ����: @iter
 * ����:
 *      1. ָ����ƣ�pos������data�ƶ�����һ��Ԫ�صĿ�ʼλ��
 *      2. �������������!����Ϊ������count!
 * ����: �ɹ� 0 ʧ�� -x
 **/
int msd_vector_iter_next(msd_vector_iter_t *iter) 
{        
    if (iter->pos == (iter->vec->count - 1)) 
    {
        /* �ﵽĩβ */
        return MSD_ERR;
    }

    if (!iter->data && !iter->pos) 
    {
        /* run for the first time */
        iter->data = iter->vec->data;
        return MSD_OK;
    } 
    else 
    {
        ++iter->pos;
        iter->data = (char *)iter->vec->data + (iter->pos * iter->vec->size);
        return MSD_OK;
    }
}

/**
 * ����: iter move next
 * ����: @iter
 * ����:
 *      1. ָ��ǰ�ƣ�pos�Լ���data�ƶ���ǰһ��Ԫ�صĿ�ʼλ��
 *      2. �������������!����Ϊ������count!
 * ����: �ɹ� 0 ʧ�� -x
 **/
int msd_vector_iter_prev(msd_vector_iter_t *iter) 
{
    if ((!iter->pos) ? 1 : 0) 
    {
        /* ���ڿ�ͷ */
        return MSD_ERR;
    }
    
    --iter->pos;
    iter->data = (char *)iter->vec->data + (iter->pos * iter->vec->size);
    return 0;
}

/**
 * ����: destroy iter
 **/
void msd_vector_iter_destroy(msd_vector_iter_t *iter) 
{
    iter->pos = 0;
    iter->data = NULL;
    iter->vec = NULL;
}

/**
 * ����: free
 **/
void msd_vector_iter_free(msd_vector_iter_t *iter) 
{
    msd_vector_iter_destroy(iter);
    free(iter);
}

/**
 * ����: free
 **/
void msd_vector_iter_reset(msd_vector_iter_t *iter) 
{
    msd_vector_t *tmp = iter->vec;
    msd_vector_iter_destroy(iter);
    msd_vector_iter_init(iter, tmp);
}
 
#ifdef __MSD_VECTOR_TEST_MAIN__
#define COUNT       100
typedef struct _test{
    int a;
    int b;
}test_t;

int main(int argc, char **argv) 
{
    int i = 0;
    test_t *value;
    test_t t;
    
    msd_vector_t *vec = msd_vector_new(32, sizeof(test_t));
    for (i = 0; i < COUNT; ++i) 
    {
        t.a = i;
        t.b = i+100;
        msd_vector_push(vec, &t);
    }
    
    for (i = 0; i < COUNT; ++i) 
    {
        value = (test_t *)msd_vector_get_at(vec, i);
        printf("idx=%d, value.a=%d, value.b=%d\n",i, value->a, value->b);
    }
    printf("slots:%d\n", vec->slots);
    printf("count:%d\n", vec->count);

    msd_vector_iter_t *iter = msd_vector_iter_new(vec);
    
    do {
        value = (test_t*)(iter->data);
        printf("%p, value.a=%d, value.b=%d \n", value, value->a, value->b);
    } while (msd_vector_iter_next(iter) == 0);
    
    msd_vector_iter_free(iter);
    msd_vector_free(vec);

    return 0;
}
#endif /* _MSD_VECTOR_TEST_MAIN__ */

