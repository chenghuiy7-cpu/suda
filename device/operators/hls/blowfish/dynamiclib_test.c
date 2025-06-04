#include "dlfcn.h"
#include "sys/mman.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include <sys/time.h>

typedef int  (*data_in_write_func_t)(unsigned long long* buffer, unsigned int size, unsigned int port_id,int thread_id);
typedef int  (*data_out_read_func_t)(unsigned long long* buffer, unsigned int size, unsigned int port_id,int thread_id);
typedef void (*context_write_func_t)(unsigned long long* data, int size,unsigned int op_id,int thread_id);
typedef int  (*run_func_t)(int thread_id);
typedef int  (*data_last_func_t)(unsigned int port_id,int thread_id);

int main(){
    struct timeval start,end;
    printf("running\n");
    void* handle = dlopen("./libblowfish_x86.so",RTLD_NOW);
    for(int i=0;i<2;i++){
    if(handle == NULL){
        fprintf(stderr,"Failed to create handle\n");
        char* error = dlerror();
        if (error != NULL) {
            fprintf(stderr,"dlopen error: %s\n", error);
            
        }
        return 0;
    }

    data_in_write_func_t data_in_write =
        dlsym(handle, "data_in_write");
    data_out_read_func_t data_out_read =
        dlsym(handle, "data_out_read");
    context_write_func_t context_write = 
        dlsym(handle,"context_write");
    run_func_t run = dlsym(handle, "run");
    data_last_func_t last = dlsym(handle,"data_last");
    if (!data_in_write) {
        fprintf(stderr,"Failed to lookup func\n");
        return 0;
    }
    unsigned long long *tx_buffer = malloc(1024*1024);
    unsigned long long *rx_buffer = malloc(1024*1024);
    if(tx_buffer==NULL||rx_buffer==NULL){
        fprintf(stderr,"Failed to load data\n");
    }
    data_in_write(tx_buffer,1024*1024,0,0);
    data_out_read(rx_buffer,1024*1024+4096,0,0);
    context_write(tx_buffer,2048,0,0);
    int cur_size = 0;
    gettimeofday(&start,NULL);
    
    while(1){
        run(0);
        if(last(0,0)){
            printf("Finish\n");
            break;
        }   
    }
}
    return 0;

}