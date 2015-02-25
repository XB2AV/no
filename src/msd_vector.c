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
 *     Version :  1.0.0
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
                
    if (!temp) {
        return MSD_ERR;
    }
    
    vec->data = temp;
    vec->slots *= 2;
    return MSD_OK;
}

/**
 * ����: vector set at
 * ����: @vec, @index
 *       @data, ������Ԫ�ص�ַ
 * ע��:
 *       ��������count�Ĵ������п��ܴ�������ģ�set_at�п����������!
 *       ����Vector�е�countֻ����һ���ο�ֵ���������һ����ЧԪ�غ��������
 * ����: �ɹ� 0 ʧ�� -x
 **/
int msd_vector_set_at(msd_vector_t *vec, unsigned int index, void *elem) 
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
    
    memcpy((char *)vec->data + (index * vec->size), elem, vec->size);

    if(index >= vec->count)
        vec->count= index+1; 
    return MSD_OK;
} 
 
/**
 * ����: ��vectorβ�����һ��Ԫ��
 * ����: @vec
 *       @elem��������Ԫ�ص�ַ
 * ����:
 *      1. vector������0��ʼ
 * ����: �ɹ� 0 ʧ�� -x
 **/  
int msd_vector_push(msd_vector_t *vec, void *elem) 
{
    if (vec->count == (vec->slots)) 
    {
        if (msd_vector_resize(vec) != MSD_OK){
            return MSD_ERR;
        }
    }
    
    return msd_vector_set_at(vec, vec->count, elem);
}

 /**
  * ����: pop a element
  * ����: @vec
  * ����:
  *      1. ������0��ʼ��ע��߽�����
  * ����: �ɹ���element��ַ��ʧ�ܣ�NULL
  **/
void *msd_vector_pop(msd_vector_t *vec)
{
    void *elem;

    if(vec->count <= 0){
        return NULL;
    }
 
    vec->count--;
    elem = (uint8_t *)vec->data + vec->size * vec->count;
 
    return elem;
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
 * ����: ���ջ��Ԫ�أ����ǲ�pop
 * ����: @vec
 * ����: �ɹ���element��ַ��ʧ�ܣ�NULL
 **/
void *msd_vector_top(msd_vector_t *vec)
{
    return msd_vector_get_at(vec, vec->count - 1);
}

/**
 * ����: ���ո����ȽϺ������������Ԫ�ؽ�����������
 * ����: @vec, @cmp
 * ����:
 *       void qsort(void *base, size_t nmemb, size_t size, int(*compar)(const void *, const void *));
 *
 * DESCRIPTION:
 *       The qsort() function sorts an array with nmemb elements of size size.  The base argument points to the start of the array.
 *       The contents of the array are sorted in ascending order according to a comparison function pointed to by compar, which is called with two arguments that point to the objects being compared.
 *       The  comparison function must return an integer less than, equal to, or greater than zero if the first argument is considered to be respectively less than, equal to, or greater than the second.  If two mem-
 *       bers compare as equal, their order in the sorted array is undefined.
 * ����: �ɹ���0��ʧ�ܣ�-1
 **/
int msd_vector_sort(msd_vector_t *vec, msd_vector_cmp_t cmp)
{
    if(vec->count == 0){
        return MSD_ERR;
    }
    qsort(vec->data, vec->count, vec->size, cmp);
    return MSD_OK;
}

/**
 * ����: �������ÿ��Ԫ�أ�ִ���ض�����
 * ����: @vec, @func, 
 *       @data������func���������
 * ����:
 *      1. ���ĳ��Ԫ��ִ��ʧ�ܣ���ֱ�ӷ��ش���
 * ����: �ɹ���0��ʧ�ܣ�NULL
 **/
int msd_vector_each(msd_vector_t *vec, msd_vector_each_t func, void *data)
{
    uint32_t i;
    int  rc;
    void *elem;

    if(vec->count == 0 || func==NULL){
        return MSD_ERR;
    }
    
    for (i = 0; i < vec->count; i++) 
    {
        elem = msd_vector_get_at(vec, i);

        rc = func(elem, data);
        if (rc != MSD_OK){
            return rc;
        }
    }

    return MSD_OK;
}

#ifdef __MSD_VECTOR_TEST_MAIN__
#define COUNT       20
typedef struct _test{
    int a;
    int b;
}test_t;

int my_cmp(const void* val1, const void* val2)
{
     test_t *p1, *p2;
     p1 = (test_t*)val1;
     p2 = (test_t*)val2;

     return (p1->b - p2->b); 
}

int my_print(void* elem, void* data)
{
    test_t        *p1;
    int            idx;
    msd_vector_t  *vec;
    
    p1  = (test_t*)elem; 
    vec = (msd_vector_t*)data;
    idx = msd_vector_idx(vec, elem);
    
    printf("idx=%d, value.a=%d, value.b=%d\n", idx, p1->a, p1->b);
    return MSD_OK;
}

int main(int argc, char **argv) 
{
    int i = 0;
    test_t t ,*value;
    
    msd_vector_t *vec = msd_vector_new(32, sizeof(test_t));
    for (i = 0; i < COUNT; ++i) 
    {
        t.a = i;
        t.b = 100-i;
        msd_vector_push(vec, &t);
    }

    //�����ظ�ֵ
    t.a = 3;t.b = 97;
    msd_vector_push(vec, &t);
    msd_vector_push(vec, &t);
    /* 
     // DUMP
     for (i = 0; i < vec->count; ++i) 
     {
        value = (test_t *)msd_vector_get_at(vec, i);
        printf("idx=%d, value.a=%d, value.b=%d\n",i, value->a, value->b);
     }
     */

    //ָ����set
    t.a = 3;t.b = 97;
    msd_vector_set_at(vec, 5, &t);   
    
    msd_vector_each(vec, my_print, vec);
    printf("slots:%d\n", vec->slots);
    printf("count:%d\n\n\n", vec->count);

    //����
    msd_vector_sort(vec, my_cmp);
    msd_vector_each(vec, my_print, vec);
    printf("slots:%d\n", vec->slots);
    printf("count:%d\n\n\n", vec->count);

    value = msd_vector_pop(vec);
    printf("value->a:%d\n", value->a);
    printf("value->b:%d\n\n\n", value->b);
    
    msd_vector_free(vec);
    return 0;
}
#endif /* _MSD_VECTOR_TEST_MAIN__ */

