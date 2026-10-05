#!/usr/bin/env python3
import unittest
from benchmark_decrypt import parse_result

CPU = 'cpu_benchmark_ms runs=5 median=1.000000 min=1 max=1\n'
OK = ('lwe_decrypt FPGA execution passed\ncorrectness_checked=yes\n'
      'benchmark_stage_ms fpga_execute=4.000 fpga_pipeline=10.000\n')
PROFILE = ('hw_profile available=yes version=1 clock_hz=250000000 '
           'boundary=ap_start_to_finish_valid cycles=125000 stream_ms=0.500000 '
           'input_wait_ms=0.100000 output_wait_ms=0.050000\n')

class Measurements(unittest.TestCase):
    def test_old_firmware_has_request_metric_only(self):
        result=parse_result(CPU+OK,False)
        self.assertEqual(result['execute_request_speedup'],0.25)
        self.assertNotIn('hw_stream_ms',result)
        self.assertNotIn('operator_speedup',result)
    def test_required_profile_rejects_old_firmware(self):
        with self.assertRaisesRegex(ValueError,'hardware profile missing'):
            parse_result(CPU+OK,False,True)
    def test_distinguishes_hardware_and_request(self):
        result=parse_result(CPU+OK+PROFILE,False,True)
        self.assertEqual(result['hardware_stream_speedup'],2.0)
        self.assertEqual(result['execute_request_speedup'],0.25)
        self.assertEqual(result['hw_stream_cycles'],125000)
    def test_bad_boundary_and_clock(self):
        for profile in [PROFILE.replace('250000000','100000000'),
                        PROFILE.replace('ap_start_to_finish_valid','loop_iterations')]:
            with self.assertRaisesRegex(ValueError,'boundary or clock'):
                parse_result(CPU+OK+profile,False)
    def test_failed_correctness_cannot_be_benchmarked(self):
        with self.assertRaisesRegex(ValueError,'correctness'):
            parse_result(CPU+PROFILE,False)
    def test_cpu_only_does_not_need_device(self):
        self.assertEqual(parse_result(CPU,True,True),{'cpu_decrypt_ms':1.0})

if __name__=='__main__': unittest.main()
