#!/usr/bin/env python3
"""Check the real runtime finish helpers across every footer/IOV split."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[3]
source = (repo / 'device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c').read_text()
start = source.index('  static int\n  mcdma_copy_rx_finish_beat(')
end = source.index('  int tx_rx_channel_poller(', start)
helpers = source[start:end]
program = r'''
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>
#include "lwe_hw_profile.h"
struct spdk_axi_dma_io { struct iovec *iovs; int iovcnt; };
'''+helpers+r'''
int main(void) {
    uint8_t finish[64] = {0}, copied[64];
    const uint64_t fields[6] = {LWE_HW_PROFILE_MAGIC,1000,10,20,100,10};
    for(int j=0;j<6;++j) for(int b=0;b<8;++b) finish[16+j*8+b]=fields[j]>>(b*8);
    memset(finish,0x5a,16);
    struct lwe_hw_profile profile;
    assert(lwe_profile_decode(finish,64,&profile));
    assert(profile.cycles==1000 && profile.input_beats==100 && profile.output_beats==10);
    assert(!lwe_profile_decode(finish,63,&profile));
    for(int prefix=0;prefix<=70;++prefix) for(int split=0;split<=64;++split) {
        for(int preserve=0;preserve<=1;++preserve) {
            uint8_t a[160],b[96],c[32];
            memset(a,0xa5,sizeof(a));memset(b,0xa5,sizeof(b));memset(c,0xa5,sizeof(c));
            struct iovec iov[3]={{a,(size_t)(prefix+split)},{b,(size_t)(64-split)},{c,sizeof(c)}};
            struct spdk_axi_dma_io io={iov,3};
            memcpy(a+prefix,finish,split);memcpy(b,finish+split,64-split);
            assert(mcdma_copy_rx_finish_beat(&io,prefix+64,64,copied)==0);
            assert(memcmp(copied,finish,64)==0);
            assert(mcdma_clear_rx_finish_beat(&io,prefix+64,64,preserve?finish:NULL)==0);
            assert(mcdma_copy_rx_finish_beat(&io,prefix+64,64,copied)==0);
            for(int i=0;i<64;++i) assert(copied[i]==(preserve && i>=16?finish[i]:0));
            for(int i=0;i<prefix;++i) assert(a[i]==0xa5);
            for(unsigned i=0;i<sizeof(c);++i) assert(c[i]==0xa5);
        }
    }
    finish[24]=0;finish[25]=0;
    assert(!lwe_profile_decode(finish,64,&profile));
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='lwe_profile_test_') as directory:
    directory=Path(directory)
    (directory/'test.c').write_text(program)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',
                    '-I'+str(repo/'device/shared_components'),str(directory/'test.c'),
                    '-o',str(directory/'test')],check=True)
    subprocess.run([str(directory/'test')],check=True)
print('PASS: profile decode and actual ARM finish helpers (9230 split/clear cases)')
