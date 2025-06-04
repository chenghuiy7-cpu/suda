#include "spdk/hash_table.h"
#include <rte_mbuf.h>
#include <rte_table_hash_cuckoo.h>
#include "spdk/env.h"
#include "spdk/log.h"
#include <rte_jhash.h>
#include "stdatomic.h"



#define MBUF_SIZE 2



void* spdk_cuckoo_table_create(int socket_id,uint32_t entry_size){
    static char flag = 'A';
    static uint8_t key_mask[4];
    memset(key_mask,0xFF,sizeof(char)*4);
    struct rte_table_hash_cuckoo_params *p_ptr = spdk_malloc(sizeof(struct rte_table_hash_cuckoo_params),2,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_SHARE);
    atomic_char af = atomic_fetch_add(&flag,1);
    p_ptr->name = "devmemtable"+flag;
    printf("Create Table Flag%d core%d\n",flag,spdk_env_get_current_core());
    p_ptr->key_size = sizeof(uint32_t);
    p_ptr->key_offset = 0;//sizeof(uint32_t);
    p_ptr->key_mask = key_mask;
    p_ptr->n_keys = 512;
    p_ptr->n_buckets = 512;
    p_ptr->f_hash = rte_jhash;
    p_ptr->seed = rand();
    return rte_table_hash_cuckoo_ops.f_create(p_ptr,rte_socket_id(),entry_size);
}

int spdk_cuckoo_table_free(void* table){
    return rte_table_hash_cuckoo_ops.f_free(table);
}

int spdk_cuckoo_table_entry_delete(void*table,uint32_t key,void* entry){
    int key_found = 0;
    
    return rte_table_hash_cuckoo_ops.f_delete(table,&key,&key_found,entry);
}

//?? How to use???
int spdk_cuckoo_table_entry_add(void* table,uint32_t key,void* entry,void** entry_ptr){
    int key_found = 0;
    return rte_table_hash_cuckoo_ops.f_add(table,&key,entry,&key_found,entry_ptr);
}

int spdk_cuckoo_table_lookup(void* table,uint32_t key,void** entry){
    uint64_t lookup_hit_mask = 0;
    
    void* key_ptr = &key;
    return rte_table_hash_cuckoo_ops.f_lookup(table,(struct rte_mbuf**)&key_ptr,1,&lookup_hit_mask,entry);
}



