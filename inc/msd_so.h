/**
 *  __  __  ___  ____ ____    _    ____  
 * |  \/  |/ _ \/ ___/ ___|  / \  |  _ \ 
 * | |\/| | | | \___ \___ \ / _ \ | | | |
 * | |  | | |_| |___) |__) / ___ \| |_| |
 * |_|  |_|\___/|____/____/_/   \_\____/ 
 *
 *     Filename:  Msd_so.h 
 * 
 *  Description:  A dynamic library implemention. 
 * 
 *      Created:  May 14, 2012 
 *      Version:  0.0.1 
 * 
 *       Author:  HQ 
 *      Company:  Qihoo 
 *
 **/
#ifndef __MSD_DLL_H_INCLUDED__ 
#define __MSD_DLL_H_INCLUDED__

/*
#define MSD_OK  0
#define MSD_ERR -1
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
*/
 
typedef struct msd_symbol_struct 
{
    char    *sym_name; /* ��̬����ĳ������������ */
    void    **sym_ptr; /* ��̬����ĳ�������ĵ�ַ */
    int     no_error;  /* ���Ϊ1���򲻹���dlsym�Ƿ�ʧ�ܣ���Ϊ0����dlsymʧ�ܺ���ͷž�� */
} msd_symbol_t;

int msd_load_so(void **phandle, msd_symbol_t *syms, const char *filename);
void msd_unload_so(void **phandle);

#endif /* __MSD_DLL_H_INCLUDED__ */
