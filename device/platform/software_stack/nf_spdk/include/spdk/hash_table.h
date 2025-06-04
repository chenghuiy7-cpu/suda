#include "spdk/env.h"
void* spdk_cuckoo_table_create(int socket_id,uint32_t entry_size);
int spdk_cuckoo_table_free(void* table);
int spdk_cuckoo_table_entry_delete(void*table,uint32_t key,void* entry);
int spdk_cuckoo_table_entry_add(void* table,uint32_t key,void* entry,void** entry_ptr);
int spdk_cuckoo_table_lookup(void* table,uint32_t key,void** entry);