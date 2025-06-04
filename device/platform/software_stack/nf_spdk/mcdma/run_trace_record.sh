APP=nvmf_tgt

set -x

../build/bin/spdk_trace_record -s nvmf -p $(pidof $APP) -f $1 -q
