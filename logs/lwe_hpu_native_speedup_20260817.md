CPU mode: serial
Output layout: hpu-native-psi64-v80 (98304 B/u8)
等价输出加速比 = CPU(加密+HPU-native打包) / FPGA execute；大于1表示FPGA更快。

| 批量(B) | SLM读块(KB) | QD | CPU加密(ms) | CPU native打包(ms) | CPU同层合计(ms) | FPGA执行(ms) | 等价输出加速比 | SLM回读(ms) | SLM回读(MiB/s) | FPGA传输就绪(ms) | FPGA一次性传输就绪(ms) |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 128 | 1 | 0.560350 | 0.038111 | 0.598619 | 9.362500 | 0.064x | 50.296500 | 1.942 | 78.346500 | 159.420000 |
| 16 | 128 | 1 | 7.390902 | 0.521434 | 7.916410 | 9.283500 | 0.853x | 589.767500 | 2.550 | 617.266500 | 702.485000 |
| 32 | 128 | 1 | 14.781208 | 1.043563 | 15.825546 | 12.530500 | 1.263x | 1177.843500 | 2.550 | 1209.864000 | 1289.406500 |
| 128 | 128 | 1 | 59.158760 | 4.235593 | 63.398940 | 28.979000 | 2.188x | 3988.583000 | 3.010 | 4033.774000 | 4113.844000 |

P95 抖动检查：
batch=1 slm_read_chunk_bytes=131072 slm_read_queue_depth=1 cpu_encrypt_and_pack_p95_ms=1.282317 fpga_execute_p95_ms=11.607000 slm_to_host_p95_ms=58.281000 fpga_transport_ready_p95_ms=91.291000
batch=16 slm_read_chunk_bytes=131072 slm_read_queue_depth=1 cpu_encrypt_and_pack_p95_ms=9.521597 fpga_execute_p95_ms=12.161000 slm_to_host_p95_ms=1656.214000 fpga_transport_ready_p95_ms=1682.539000
batch=32 slm_read_chunk_bytes=131072 slm_read_queue_depth=1 cpu_encrypt_and_pack_p95_ms=15.829467 fpga_execute_p95_ms=14.936000 slm_to_host_p95_ms=5365.945000 fpga_transport_ready_p95_ms=5398.168000
batch=128 slm_read_chunk_bytes=131072 slm_read_queue_depth=1 cpu_encrypt_and_pack_p95_ms=63.529657 fpga_execute_p95_ms=31.581000 slm_to_host_p95_ms=9846.110000 fpga_transport_ready_p95_ms=9890.963000

说明：FPGA传输就绪 = SSD->SLM + FPGA execute + output SLM->Host，不含Host正确性验证。
CPU同层合计从Host内存中的明文开始，FPGA execute从input SLM中的明文开始；完整系统比较需另测SSD->Host CPU路径。
