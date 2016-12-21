/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * onvm_sc_mgr.c - service chain functions for manager 
 ********************************************************************/

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_debug.h>
#include <rte_malloc.h>
#include "onvm_sc_mgr.h"
#include "onvm_sc_common.h"

struct onvm_service_chain*
onvm_sc_get(void) {
	return NULL;
}

struct onvm_service_chain*
onvm_sc_create(void)
{
        struct onvm_service_chain *chain;

        chain = rte_calloc("ONVM_sercice_chain",
                        1, sizeof(struct onvm_service_chain), 0);
        if (chain == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for service chain\n");
        }
	return chain;
}


#include <stdlib.h>
typedef enum ONVM_SORT_DATA_TYPE {
        ONVM_SORT_TYPE_INT = 0,
        ONVM_SORT_TYPE_UINT32 = 1,
        ONVM_SORT_TYPE_UINT64 = 2,
        ONVM_SORT_TYPE_CUSTOM = 3,
}ONVM_SORT_DATA_TYPE_E;
typedef enum ONVM_SORT_MODE {
        SORT_ASCENDING =0,
        SORT_DESCENDING =1,
}ONVM_SORT_MODE_E;

typedef int (*comp_func)(const void *, const void *);
typedef int (*comp_func_s)(const void *, const void *, void*);

void onvm_sort_uint32( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf);

void onvm_sort_generic( void* ptr, ONVM_SORT_DATA_TYPE_E data_type, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) size_t size, __attribute__((unused)) comp_func cf);
void onvm_sort_generic_r( void* ptr, ONVM_SORT_DATA_TYPE_E data_type, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) size_t size, __attribute__((unused)) comp_func_s cf);
int onvm_cmp_func_r (const void * a, const void *b, void* data);

int onvm_cmp_uint32_r(const void * a, const void *b, void* data);
int onvm_cmp_uint32_a(const void * a, const void *b);
int onvm_cmp_uint32_d(const void * a, const void *b);

void onvm_sort_uint32_t(void);
void onvm_sort_uint64_t(void);

typedef struct onvm_callback_thunk {
        ONVM_SORT_DATA_TYPE_E type;
        ONVM_SORT_MODE_E mode;
        comp_func_s cf;
}onvm_callback_thunk_t;

int onvm_cmp_uint32_r(const void * a, const void *b, void* data) {
        uint32_t arg1 = *(const int*)a;
        uint32_t arg2 = *(const int*)b;
        int val = 0;

        if (arg1 < arg2) val = -1;
        else if (arg1 > arg2) val = 1;
        else    return 0;

        if(data) {
               switch(*((const ONVM_SORT_MODE_E*)data)) {
                       case SORT_ASCENDING:
                               return val;
                       case SORT_DESCENDING:
                               val *= (-1);
                               break;
                       default:
                               return val;
               }
        }
        return val;
}
int onvm_cmp_uint32_a(const void * a, const void *b) {
        uint32_t arg1 = *(const int*)a;
        uint32_t arg2 = *(const int*)b;
        int val = 0;

        if (arg1 < arg2) val = -1;
        else if (arg1 > arg2) val = 1;
        else    return 0;
        return val;
}
int onvm_cmp_uint32_d(const void * a, const void *b) {
        uint32_t arg1 = *(const int*)a;
        uint32_t arg2 = *(const int*)b;
        int val = 0;

        if (arg1 < arg2) val = 1;
        else if (arg1 > arg2) val = -1;
        else    return 0;
        return val;
}
void onvm_sort_uint32( void* ptr, ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) comp_func_s cf) {
        if(sort_mode == SORT_DESCENDING)
                qsort(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_d); //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_d, &sort_mode);
        else
                qsort(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_a);   //qsort_r(ptr, count, sizeof(uint32_t), onvm_cmp_uint32_a, NULL);
}


int onvm_cmp_func_r (const void * a, const void *b, void* data) {
        if(data) {
               // onvm_callback_thunk_t *onvm_thunk = (onvm_callback_thunk_t*)data;
                return 0;
        }
        if(a && b) {
                return -1;
        }
        return 1;
}
void onvm_sort_generic_r( void* ptr,  __attribute__((unused)) ONVM_SORT_DATA_TYPE_E data_type,  __attribute__((unused)) ONVM_SORT_MODE_E sort_mode, size_t count,
                __attribute__((unused)) size_t size, __attribute__((unused))  comp_func_s cf) {

        //onvm_callback_thunk_t onvm_thunk = {.type=data_type, .mode=sort_mode, .cf=cf };
        switch(data_type) {
        case ONVM_SORT_TYPE_INT:

                break;
        case ONVM_SORT_TYPE_UINT32:
                break;
        case ONVM_SORT_TYPE_UINT64:
                break;
        case ONVM_SORT_TYPE_CUSTOM:
                break;
        }
        //qsort_r( ptr, count, size, onvm_cmp_func_s, &onvm_thunk);
        qsort(ptr, count, size, onvm_cmp_uint32_a);
}
