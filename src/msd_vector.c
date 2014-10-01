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
 *    Modified :  ȥ���˴���bug��iter
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
        
    if (msd_vector_init(vec, slots, size) != MSD_OK) 
    {
        free(vec);
        return NULL;
    }
    
    return vec;
}

/**
 * ����: destroy the vector
 * ����: @vec
 * ����:
 *      1. ���ͷ�data��vec����ṹ���ͷ�
 **/
static void msd_vector_destroy(msd_vector_t *vec) 
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
 * ����: ����һ��Ԫ�صĵ�ַ����ô�Ԫ����vector�е�����
 * ����: @vec, @elemԪ�ص�ַ
 * ����: �ɹ���������ʧ�ܣ�-1
 **/
uint32_t msd_vector_idx(msd_vector_t *vec, void *elem)
{
    uint8_t   *p, *q;    //�ڴ水�ֽڱ�ַ
#if __WORDSIZE == 32
    uint32_t   off, idx;
#else
    uint64_t   off, idx;
#endif
    
    if(elem < vec->data){
        return MSD_ERR;
    }

    p = vec->data;
    q = elem;
    
#if __WORDSIZE == 32
    off = (uint32_t)(q - p);
    if(off % (uint32_t)vec->size != 0){
        return MSD_ERR;
    }
    idx = off / (uint32_t)vec->size;
#else
    off = (uint64_t)(q - p);
    if(off % (uint64_t)vec->size != 0){
        return MSD_ERR;
    }
    idx = off / (uint64_t)vec->size;
#endif

    return idx;
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
    if (vec->count == (vec->slots)) 
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

    msd_vector_free(vec);

    return 0;
}
#endif /* _MSD_VECTOR_TEST_MAIN__ */

