#include "blowfish.hpp"
#include "sw_wrapper.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <thread>
#include <time.h>
#include <vector>
#include <atomic>
#include <iostream>

#include <sys/time.h>
#include <unistd.h>
int main()
{
    struct timeval start,end;
    int size = 1024*1024/64;
    Acc_Data in,out;
    Acc_Data_Pkt p;
    p.data = 0;
    p.last = 0;
    p.keep = -1;
    p.user = 0;
    #ifdef USING_XILINX_STREAM
        gettimeofday(&start,nullptr);
        int cur_size = 0;
        
        while(1)
        {
            
            if(cur_size<size){
                int write_size = (size-cur_size)<(4096/64)?(size-cur_size):(4096/64);
                DTRACE_PROBE(test,data_begin_move);
                for(int i=0;i<write_size;i++)
                {
                    if((cur_size%(4096/64))==0)
                    {
                        p.last = 1;
                    }else{
                        p.last = 0;
                    }
                    in.write(p);
                    
                    cur_size++;
                }
                DTRACE_PROBE(test,data_end_move);
            }else if(cur_size == size){
                cur_size++;
                DTRACE_PROBE(test,data_begin_move);
                p.last = 1;
                p.user = 255;
                in.write(p);
                DTRACE_PROBE(test,data_end_move);
	    }
            DTRACE_PROBE(test,data_begin_cacl);
            blowfish(in,out);
            DTRACE_PROBE(test,data_end_cacl);
            if(!out.empty()){
                DTRACE_PROBE(test,data_begin_move);
                out.read(p);
                if(p.user==255){
                    break;
                }
                DTRACE_PROBE(test,data_end_move);
            }
           
        }  
        gettimeofday(&end,nullptr);
        fprintf(stdout,"Finish blowfish encryption,time used %lf s\n",end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);
    #else
        struct AccContext context;
        void* data_base = malloc(size*64+4096);
        gettimeofday(&start,nullptr);
        
        DTRACE_PROBE(test,data_begin_move);
        context_write((unsigned long long *)&(context),sizeof(struct AccContext),0,0);
        data_in_write((unsigned long long *)data_base,size*64,0,0);
        data_out_read((unsigned long long *)data_base,size*64+4096,0,0);
        DTRACE_PROBE(test,data_end_move);
        
        while(1)
        {
            run(0);
            if(data_last(0,0)){
                break;
            }
        } 
        DTRACE_PROBE(test,data_begin_cacl);
        //for(int i=0;i<size;i++){
        //    blowfish_encrypt_512mini((unsigned long long*)data_base,(unsigned long long*)data_base);
        //}   
        DTRACE_PROBE(test,data_end_cacl);
        gettimeofday(&end,nullptr);
        fprintf(stdout,"Finish blowfish encryption,time used %lf s\n",end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);
    #endif
    return 0;
}
