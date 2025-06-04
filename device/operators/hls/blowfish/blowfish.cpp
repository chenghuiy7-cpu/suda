#include "blowfish.hpp"
#include <iostream>
#define N 16


/* Function that merge the encrypt res of lower 64b and upper 64b */
inline ap_uint<128> blowfish_encrypt_128(ap_uint<128> target)
{
    return (((ap_uint<128>)blowfish_encrypt((unsigned long long)(target >> 64))) << 64) | (ap_uint<128>)(blowfish_encrypt(target & 0xffffffffffffffff) & 0xffffffffffffffff);
}

inline ap_uint<512> blowfish_encrypt_512(ap_uint<512> target)
{

    static ap_uint<128> t1, t2, t3, t4, r1, r2, r3, r4;
    /*
    unsigned long long* in_data = (unsigned long long*)&target;
    static ap_uint<512> out_data_raw;
    unsigned long long* out_data = (unsigned long long*)&(out_data_raw);
    for(int i=0;i<8;i++)
    out_data[i] = blowfish_encrypt(in_data[i]);
    return out_data_raw;*/
    
    t1 = target(511, 384);
    t2 = target(383, 256);
    t3 = target(255, 128);
    t4 = target(127, 0);

    r1 = blowfish_encrypt_128(t1);
    r2 = blowfish_encrypt_128(t2);
    r3 = blowfish_encrypt_128(t3);
    r4 = blowfish_encrypt_128(t4);
    ap_uint<512> res = (r1, r2, r3, r4);
    return res;
    
}



/*** TOP FUNCTION of encrypt ***/
// A - input; B - op output; e - info; e_m - pass
void blowfish(Acc_Data &stream_in, Acc_Data &stream_out)
{
// #pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS INTERFACE ap_none port = return
#pragma HLS INTERFACE axis port = stream_in
#pragma HLS INTERFACE axis port = stream_out
#pragma HLS PIPELINE II = 1
    if (!stream_in.empty())
    {
        while (1)
        {
            
            if (!stream_in.empty())
            {
            
                static Acc_Data_Pkt t;
	        t = stream_in.read();
                if (t.user(7, 4) != 0)
                {
                    stream_out.write(t);
                    return;
                }
                else if (t.last)
                {
                    t.data = blowfish_encrypt_512(t.data);
               
                    stream_out.write(t);
                    return;
                }
                else
                {
                    t.data = blowfish_encrypt_512(t.data);
		            stream_out.write(t);
		        }
            }else{
	        return;
	    }
        }
	
    }
}
