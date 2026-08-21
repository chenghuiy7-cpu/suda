# LWE Encrypt Operator Work Log

Date: 2026-06-09

Objective:
- Add a SUDA HLS hardware operator prototype for the TFHE LWE core encryption datapath.

Files changed:
- Updated `device/operators/Makefile` and appended `lwe_encrypt` to `HWOP_TARGETS`.
- Added `device/operators/hls/lwe_encrypt/lwe_encrypt.hpp`.
- Added `device/operators/hls/lwe_encrypt/lwe_encrypt.cpp`.
- Added `device/operators/hls/lwe_encrypt/test.cpp`.
- Added `device/operators/hls/lwe_encrypt/run_hls.tcl`.

Design summary:
- The top function is `lwe_encrypt(Acc_Data &data_in, Acc_Data &data_out, ap_uint<512> context[256])`.
- The operator follows the SUDA HLS interface pattern: one AXIS input, one AXIS output, and one 512-bit BRAM context.
- The output format is the CPU/tfhe-rs LWE ciphertext layout: `[mask_0, ..., mask_n-1, body]`, packed as eight `u64` words per 512-bit packet.
- Partial output packets set AXIS `keep`/`strb` according to the number of valid 64-bit words.
- Added named constants for the fixed HPU preset in `/home/yangchenghui/hpu/tfhe-rs/mockups/tfhe-hpu-mockup/params/tuniform_64b_pfail128_psi64.toml`.
- `context[0]` stores mask dimension, request count, input mode, noise mode, delta, seed, and nonce.
- Secret key coefficients are binary and packed from `context[1]`, one coefficient per bit.
- The datapath computes `body = dot(mask, secret_key) + encoded + noise` with wrapping `u64` arithmetic.
- The input may provide an already encoded plaintext, or a clear block that is encoded as `clear * delta`.
- The input may provide the noise word directly. The internal noise generator is only a simple t-uniform-like prototype and is not a bit-exact tfhe-rs Gaussian/TUniform sampler.

Important compatibility notes:
- The HPU TOML is treated as the fixed configuration contract and was not modified.
- For direct HPU-bound ciphertexts under `tuniform_64b_pfail128_psi64.toml`, fill the context with:
  `mask_dimension = 2048`, `delta = 1 << 59`, `noise_bound_log2 = 17`, and the GLWE secret key flattened as the Big LWE key.
- The TOML field `lwe_dimension = 879` is the small/input LWE dimension. It is not the LWE size expected by the current tfhe-rs HPU boundary conversion path.
- This is a CPU-format LWE generator. To feed tfhe-rs HPU execution, host-side code still needs to wrap the output into `LweCiphertextOwned`, `shortint::Ciphertext`, and `RadixCiphertext`, then call the existing HPU conversion path.
- For cryptographic compatibility with tfhe-rs production parameters, the PRNG and noise sampler must be aligned with tfhe-rs, or the host must provide sampled noise through `LWE_ENCRYPT_NOISE_INPUT`.
- HPU native layout conversion is intentionally not implemented in this first operator. That should be a separate step after validating CPU-format LWE output.

Validation performed:
- Added a C simulation testbench that checks mask generation, binary secret-key dot product, partial-packet flushing, and final body construction for two small LWE ciphertexts.
- Ran syntax check:
  `g++ -std=c++14 -fsyntax-only -DUSING_XILINX_STREAM -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -Idevice/shared_components/hls device/operators/hls/lwe_encrypt/lwe_encrypt.cpp device/operators/hls/lwe_encrypt/test.cpp`
- Ran local testbench:
  `g++ -std=c++14 -DUSING_XILINX_STREAM -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -Idevice/shared_components/hls device/operators/hls/lwe_encrypt/lwe_encrypt.cpp device/operators/hls/lwe_encrypt/test.cpp -o /tmp/lwe_encrypt_test && /tmp/lwe_encrypt_test`
- Test output:
  `lwe_encrypt test passed. dimension=16 ciphertext_words=34`
- Ran dry-run Makefile check:
  `make -n lwe_encrypt_prj`
- Ran dry-run hardware target check:
  `make -n hwop_gen`

Update: 2026-06-10

Validation program additions:
- Reworked `device/operators/hls/lwe_encrypt/test.cpp` into a reusable C simulation testbench.
- Kept the original small 16-dimensional encoded-input check.
- Added an HPU-aligned Big-LWE check with `mask_dimension = 2048`, `delta = 1 << 59`, clear inputs `0..3`, externally supplied noise words, and a packed binary secret key spread across the BRAM context.
- The C++ test now verifies AXIS `keep`, final `last/user`, all generated mask words, and the final `body = dot(mask, secret_key) + encoded + noise`.

tfhe-rs cross-check:
- Added a unit test in `/home/yangchenghui/hpu/tfhe-rs/tfhe/src/shortint/client_key/atomic_pattern/ks32.rs`.
- The unit test builds a `KeySwitch32PBSParameters` value matching the HPU TOML preset, manually creates an HLS-style Big-LWE ciphertext, and checks that the same KS32 client key decrypts it.
- The test explicitly checks `lwe_dimension = 879` and encryption/Big-LWE dimension `2048`.

Validation performed:
- Ran:
  `g++ -std=c++14 -DUSING_XILINX_STREAM -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -Idevice/shared_components/hls device/operators/hls/lwe_encrypt/lwe_encrypt.cpp device/operators/hls/lwe_encrypt/test.cpp -o /tmp/lwe_encrypt_test && /tmp/lwe_encrypt_test`
- Test output:
  `small encoded-input LWE reference passed. dimension=16 ciphertext_words=34`
  `HPU Big-LWE clear-input reference passed. dimension=2048 ciphertext_words=8196`
- Ran:
  `cargo test -p tfhe --features shortint hls_style_big_lwe_ciphertext_decrypts_with_client_key --lib`
- Test output:
  `test shortint::client_key::atomic_pattern::ks32::hls_lwe_encrypt_verify_tests::hls_style_big_lwe_ciphertext_decrypts_with_client_key ... ok`

Update: HLS-output decryptability check
- Extended `device/operators/hls/lwe_encrypt/test.cpp` to decrypt the actual words produced by `lwe_encrypt.cpp`.
- The check computes `decrypted_plaintext = body - dot(mask, secret_key)` from the emitted `[mask..., body]` words.
- For clear-input HPU ciphertexts, it then decodes with the shortint HPU delta `1 << 59` and checks that the result is the original message modulo `message_modulus = 4`.
- Re-ran:
  `g++ -std=c++14 -DUSING_XILINX_STREAM -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -Idevice/shared_components/hls device/operators/hls/lwe_encrypt/lwe_encrypt.cpp device/operators/hls/lwe_encrypt/test.cpp -o /tmp/lwe_encrypt_test && /tmp/lwe_encrypt_test`
- Test output:
  `small encoded-input LWE reference passed. dimension=16 ciphertext_words=34 decrypt_checked=yes`
  `HPU Big-LWE clear-input reference passed. dimension=2048 ciphertext_words=8196 decrypt_checked=yes`

Update: fixed u8 radix input contract
- Added `LWE_ENCRYPT_INPUT_U8_RADIX` to `device/operators/hls/lwe_encrypt/lwe_encrypt.hpp`.
- In this mode, the Host sends one clear `u8` per input packet. The operator splits it into four 2-bit radix blocks internally and emits four Big-LWE ciphertexts in least-significant-block-first order.
- The HPU constants are fixed inside the operator for this mode: `mask_dimension = 2048` is required and `delta = 1 << 59` is used internally.
- If `LWE_ENCRYPT_NOISE_INPUT` is used, the input packet carries four `u64` noise words:
  block 0 at `[127:64]`, block 1 at `[191:128]`, block 2 at `[255:192]`, and block 3 at `[319:256]`.
- `request_count` now means number of clear `u8` inputs in this mode. Output ciphertext count is `request_count * 4`.
- The Host still needs to provide the Big-LWE/flattened GLWE secret key in `context[1...]`.
- Extended `device/operators/hls/lwe_encrypt/test.cpp` with an HPU u8-radix case that decrypts every emitted ciphertext block and reconstructs the original byte.
- Re-ran:
  `g++ -std=c++14 -DUSING_XILINX_STREAM -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -Idevice/shared_components/hls device/operators/hls/lwe_encrypt/lwe_encrypt.cpp device/operators/hls/lwe_encrypt/test.cpp -o /tmp/lwe_encrypt_test && /tmp/lwe_encrypt_test`
- Test output:
  `small encoded-input LWE reference passed. dimension=16 ciphertext_words=34 decrypt_checked=yes`
  `HPU Big-LWE clear-input reference passed. dimension=2048 ciphertext_words=8196 decrypt_checked=yes`
  `HPU u8-radix reference passed. u8_inputs=4 radix_blocks=16 ciphertext_words=32784 decrypt_checked=yes`

Update: saved psi64 HPU key flow
- Added a tfhe-rs example tool:
  `/home/yangchenghui/hpu/tfhe-rs/tfhe/examples/hpu/export_lwe_encrypt_key.rs`
- Registered it as:
  `cargo run -p tfhe --features hpu --example hpu_export_lwe_encrypt_key`
- The tool reads:
  `/home/yangchenghui/hpu/tfhe-rs/mockups/tfhe-hpu-mockup/params/tuniform_64b_pfail128_psi64.toml`
- It generated:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_shortint_ks32_client_key.bincode`
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin`
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_key_manifest.txt`
- The raw HLS key file stores the flattened GLWE/Big-LWE key as one byte per binary coefficient.
- Generated key summary:
  `lwe_dimension=879`
  `encryption_lwe_dimension=2048`
  `big_lwe_key_bytes=2048`
  `big_lwe_key_ones=1015`
- Updated `device/operators/hls/lwe_encrypt/test.cpp` so HPU Big-LWE and u8-radix cases load `psi64_big_lwe_secret_key.bin` instead of constructing a test key in C++.
- Re-ran:
  `g++ -std=c++14 -DUSING_XILINX_STREAM -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -Idevice/shared_components/hls device/operators/hls/lwe_encrypt/lwe_encrypt.cpp device/operators/hls/lwe_encrypt/test.cpp -o /tmp/lwe_encrypt_test && /tmp/lwe_encrypt_test`
- Test output:
  `small encoded-input LWE reference passed. dimension=16 ciphertext_words=34 decrypt_checked=yes`
  `HPU Big-LWE clear-input saved-key reference passed. dimension=2048 ciphertext_words=8196 decrypt_checked=yes`
  `loaded saved psi64 Big-LWE secret key. dimension=2048 ones=1015`
  `HPU u8-radix saved-key reference passed. u8_inputs=4 radix_blocks=16 ciphertext_words=32784 decrypt_checked=yes`

Update: tfhe-rs ClientKey verification for HLS output dump
- Extended `device/operators/hls/lwe_encrypt/test.cpp` so the saved-key u8-radix C simulation test writes the actual emitted ciphertext words to:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_u8_radix_hls_ciphertexts.bin`
- Dump format:
  `LWEHLS01` magic, version, mask dimension, input count, radix block count, message/carry/padding widths, delta log2, ciphertext word count, clear u8 inputs, then raw u64 ciphertext words in `[mask..., body]` order.
- Added a tfhe-rs verification example:
  `/home/yangchenghui/hpu/tfhe-rs/tfhe/examples/hpu/verify_lwe_encrypt_output.rs`
- Registered it in:
  `/home/yangchenghui/hpu/tfhe-rs/tfhe/Cargo.toml`
  as `hpu_verify_lwe_encrypt_output`.
- The verifier reads:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_shortint_ks32_client_key.bincode`
  and the HLS ciphertext dump above.
- It wraps each dumped Big-LWE ciphertext as a `shortint::Ciphertext` and calls `client_key.decrypt(&ct)` for every 2-bit radix block, then reconstructs the original u8 inputs.
- Re-ran:
  `/tmp/lwe_encrypt_test`
- Test output:
  `small encoded-input LWE reference passed. dimension=16 ciphertext_words=34 decrypt_checked=yes`
  `HPU Big-LWE clear-input saved-key reference passed. dimension=2048 ciphertext_words=8196 decrypt_checked=yes`
  `loaded saved psi64 Big-LWE secret key. dimension=2048 ones=1015`
  `HPU u8-radix saved-key reference passed. u8_inputs=4 radix_blocks=16 ciphertext_words=32784 decrypt_checked=yes dump=/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_u8_radix_hls_ciphertexts.bin`
- Dump size:
  `257K`
- Ran:
  `cargo run -p tfhe --features hpu --example hpu_verify_lwe_encrypt_output`
- Verification output:
  `client_key_file=/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_shortint_ks32_client_key.bincode`
  `ciphertext_dump=/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_u8_radix_hls_ciphertexts.bin`
  `mask_dimension=2048`
  `u8_inputs=4`
  `radix_blocks=16`
  `tfhe_client_key_decrypt_checked=yes`

Update: split HLS smoke testbench from deep verification
- Preserved the previous long C++ verification testbench as:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test_deep.cpp`
- Replaced:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test.cpp`
  with a shorter HLS CSIM/COSIM-oriented smoke testbench.
- The new smoke test keeps the production-size parameters:
  `mask_dimension=2048`
  `input_mode=LWE_ENCRYPT_INPUT_U8_RADIX`
  `u8_inputs=1`
  `radix_blocks=4`
  `delta=1 << 59`
- It avoids external key files and ciphertext dumps. Instead, it uses a deterministic in-test binary key pattern, emits one u8 request, verifies every output mask/body word, checks AXIS final `last/user`, and reconstructs the original u8 plaintext from the decrypted radix blocks.
- Re-ran:
  `g++ -std=c++14 -DUSING_XILINX_STREAM -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -Idevice/shared_components/hls device/operators/hls/lwe_encrypt/lwe_encrypt.cpp device/operators/hls/lwe_encrypt/test.cpp -o /tmp/lwe_encrypt_hls_smoke_test`
- Test output:
  `lwe_encrypt HLS smoke test passed. mask_dimension=2048 u8_inputs=1 radix_blocks=4 decrypt_checked=yes`

Update: Vitis HLS CSIM/CSYNTH/COSIM attempt
- Updated:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/run_hls.tcl`
  to add explicit `csim`, `csynth`, and `cosim` modes, and to make direct `vitis_hls -f run_hls.tcl -tclargs lwe_encrypt <mode>` invocation work.
- Added HLS 2020.2 compatibility settings in the same script:
  - explicit Vitis include path
  - local compatibility include symlink:
    `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/compat/x86_64_linux_gnu -> /usr/include/x86_64-linux-gnu`
  - `DEBUG` environment cleanup, because `DEBUG=release` was being passed to g++ as a stray source-file argument during cosim wrapper compilation
  - Vivado runtime environment hints for XSIM (`RDI_*`, `HDI_APPROOT`, `LD_LIBRARY_PATH`)
- Updated:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt.hpp`
  to define `NO_CTOR` before including Xilinx AXIS headers, avoiding an HLS 2020.2 synthesis parsing issue in `ap_axi_sdata.h`.
- Updated:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt.cpp`
  to use a literal tripcount value in the HLS pragma and older-style HLS interface pragmas compatible with Vitis HLS 2020.2.
- Ran:
  `/opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls -f run_hls.tcl -tclargs lwe_encrypt csim`
- CSIM result:
  `lwe_encrypt HLS smoke test passed. mask_dimension=2048 u8_inputs=1 radix_blocks=4 decrypt_checked=yes`
  `CSim done with 0 errors.`
- Ran:
  `/opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls -f run_hls.tcl -tclargs lwe_encrypt cosim`
- COSIM path result:
  - `csynth_design` completed successfully.
  - RTL Verilog/VHDL was generated.
  - HLS reported estimated Fmax:
    `329.14 MHz`
  - C wrapper compilation completed after clearing `DEBUG=release`.
  - C TB inside cosim passed with the same smoke-test output.
  - RTL testbench generation and XSIM elaboration completed, including:
    `Built simulation snapshot lwe_encrypt`
  - Final XSIM run failed with:
    `ERROR: unknown error occurred`
    while executing:
    `xsim {lwe_encrypt} -autoloadwcfg -tclbatch {lwe_encrypt.tcl}`
- Current conclusion:
  CSIM and CSYNTH are verified on this machine. Full C/RTL COSIM is blocked at the local Vivado/XSIM runtime stage after snapshot build, not at C testbench compilation, HLS synthesis, or RTL generation.

Update: `make lwe_encrypt_hwop` RTL/IP generation
- User ran:
  `make lwe_encrypt_hwop`
  from:
  `/home/yangchenghui/suda/device/operators`
- Initial failure was caused by:
  `/home/yangchenghui/suda/device/operators/scripts/toolset.mk`
  sourcing the 2022.2 Xilinx settings script. On this machine the 2022.2 script points to missing files under `/mnt/vivado/Xilinx_2022.2/...`, so the Vitis environment setup failed before HLS could run.
- Updated:
  `/home/yangchenghui/suda/device/operators/scripts/toolset.mk`
  to use the installed 2020.2 Vitis HLS/Vivado paths by default:
  `/opt/Xilinx_2020.2/Vitis_HLS/2020.2`
  and
  `/opt/Xilinx_2020.2/Vivado/2020.2`.
- A second issue appeared during `export_design`: Vitis/Vivado 2020.2 generated a date-based `core_revision` value such as `2606161433`, which is too large for the packager path and caused a `bad lexical cast` failure.
- Updated:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/run_hls.tcl`
  with a fallback that patches the generated `run_ippack.tcl` revision to `1`, reruns `pack.sh`, and copies the packed IP zip to the expected output name.
- Re-ran:
  `make lwe_encrypt_hwop`
- Final result:
  `HLS processing for lwe_encrypt completed successfully.`
- Generated IP package:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt_ip.zip`
- Zip size:
  `86K`
- Zip contents include:
  `component.xml`,
  `hdl/verilog/lwe_encrypt.v`,
  `hdl/verilog/lwe_encrypt_encrypt_encoded_lwe.v`,
  `hdl/verilog/lwe_encrypt_encrypt_one_lwe.v`,
  `hdl/verilog/lwe_encrypt_encrypt_u8_radix.v`,
  and matching VHDL files.

Update: copy generated Verilog into SUDA platform source tree
- Created destination directory:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt`
- Extracted only the Verilog files from:
  `/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt_ip.zip`
  using the zip path:
  `hdl/verilog/*.v`
- Copied/generated files in the destination directory:
  `lwe_encrypt.v`,
  `lwe_encrypt_encrypt_encoded_lwe.v`,
  `lwe_encrypt_encrypt_one_lwe.v`,
  `lwe_encrypt_encrypt_u8_radix.v`,
  `lwe_encrypt_flush_output_packet.v`,
  `lwe_encrypt_mul_64s_64s_64_5_1.v`,
  `lwe_encrypt_regslice_both.v`,
  `lwe_encrypt_write_output_word.v`.
- Updated:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/scripts/prj_setup.tcl`
  to add:
  `add_files -fileset sources_1 [glob -nocomplain ${design_dir}/../fpga/sources/hlsaccframework/lwe_encrypt/*.v]`

Update: accframework Vivado project/DCP generation check
- User ran from:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/work_farm`
  the command:
  `make PRJ=shell:virt_one_drive:accframework FPGA_BD=fidus FPGA_ACT=dcp_gen vivado_prj`
- Checked output DCP:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/work_farm/fpga/vivado_out/shell_virt_one_drive_accframework_fidus/dcp/accframework.dcp`
- The DCP was regenerated on:
  `2026-06-16 16:46`
- Checked log:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/work_farm/fpga/vivado_out/shell_virt_one_drive_accframework_fidus/run_log/dcp_gen.log`
- Vivado reported:
  `synth_design completed successfully`
  and:
  `0 Errors encountered`
- The generated project file includes the copied lwe_encrypt Verilog sources:
  `hlsaccframework/lwe_encrypt/*.v`
- Important limitation:
  this only confirms the `lwe_encrypt` source files are part of the Vivado project. The Block Design script still does not instantiate/connect a `lwe_encrypt_0` module with its own `OperatorController`, context BRAM, op_id, and AXIS switch routes, so the operator is not yet callable from SUDA runtime.

Update: add `lwe_encrypt` into the SUDA accframework operator pool
- Updated:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/scripts/accframework.tcl`
- Added `lwe_encrypt` to the module existence check list.
- Added a third operator slot:
  - `OperatorController_2`
  - `lwe_encrypt_0`
  - `static_var_bram2`
  - `xlconstant_6`, with `CONST_VAL = 2` and `CONST_WIDTH = 4`, connected to `OperatorController_2/op_id`
- Expanded control/context/data switches:
  - `axis_switch_0`: `NUM_MI` from 2 to 3 for control request routing
  - `axis_switch_1`: `NUM_SI` from 2 to 3 for control response routing
  - `axis_switch_2`: `NUM_MI` from 2 to 3 for context recovery routing
  - `axis_switch_4`: `NUM_MI/NUM_SI` from 3 to 4 for data routing
  - assigned the new data destination range:
    `M03_AXIS_BASETDEST = 0x20`,
    `M03_AXIS_HIGHTDEST = 0x2f`
- Connected the new slot:
  - `OperatorController_2/m_axis_inside` -> `lwe_encrypt_0/data_in`
  - `lwe_encrypt_0/data_out` -> `OperatorController_2/s_axis_inside`
  - `OperatorController_2/m_axis_outside` -> `axis_switch_4/S03_AXIS`
  - `axis_switch_4/M03_AXIS` -> `OperatorController_2/s_axis_outside`
  - `axis_switch_0/M02_AXIS` -> `OperatorController_2/ctrl_req_from_ctrl`
  - `OperatorController_2/ctrl_rsp_to_ctrl` -> `axis_switch_1/S02_AXIS`
  - `axis_switch_2/M02_AXIS` -> `OperatorController_2/recovery_context_from_ctrl`
  - `lwe_encrypt_0/context_*` -> `OperatorController_2` BRAM input side
  - `OperatorController_2` BRAM output side -> `static_var_bram2`
  - `static_var_bram2/douta` -> `OperatorController_2/bram_q_out`
  - `OperatorController_2/ap_start/ap_rst_n` -> `lwe_encrypt_0`
  - `lwe_encrypt_0/ap_done/ap_idle/ap_ready` -> `OperatorController_2`
- Re-ran from:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/work_farm`
  the command:
  `make PRJ=shell:virt_one_drive:accframework FPGA_BD=fidus FPGA_ACT=dcp_gen vivado_prj`
- Vivado result:
  `synth_design completed successfully`
  `0 Errors encountered`
  checkpoint generated at:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/work_farm/fpga/vivado_out/shell_virt_one_drive_accframework_fidus/dcp/accframework.dcp`
- Generated DCP:
  `21M`, timestamp `2026-06-16 17:11`
- Confirmed generated BD/synthesis outputs contain:
  `lwe_encrypt_0`,
  `OperatorController_2`,
  `static_var_bram2`,
  `M03_AXIS`,
  `S03_AXIS`.
- Remaining note:
  Vivado still reports many clock-association and width-mismatch critical warnings, similar to the existing accframework style. The generated DCP succeeds, but runtime validation from host is still required.

Update: pre-bitstream-generation backup
- Current generated accframework DCP with `lwe_encrypt`:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/work_farm/fpga/vivado_out/shell_virt_one_drive_accframework_fidus/dcp/accframework.dcp`
  size `21M`, timestamp `2026-06-16 17:11`.
- Current final bitstream in the SUDA build tree is still the old one:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/work_farm/hw_plat/shell_virt_one_drive_fidus/system.bit`
  size `35M`, timestamp `2025-07-15`.
- Current downloadable/boot image is also old:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/ready_for_download/fidus/BOOT.bin`
  size `36M`, timestamp `2025-07-16`.
- Created backup directory:
  `/home/yangchenghui/suda/backups/pre_lwe_encrypt_bitgen_2026-06-16`
- Backed up:
  - `system.bit`
  - `BOOT.bin`
- Conclusion:
  the `lwe_encrypt` operator is in the new accframework DCP, but it is not yet present in the old final `system.bit`/`BOOT.bin`. A full bitstream/boot image regeneration is still required for FPGA runtime use.

Update: expanded old-shell recovery backup
- Reviewed:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/build_bd.sh`
- The script generates more than the final `system.bit` and `BOOT.bin`; it also depends on/intermediately produces accframework and pcie_ep DCPs, final shell synth/place/route DCPs, hardware platform files, device tree, FSBL/PMUFW/ATF/U-Boot artifacts, and ready-for-download files.
- Created compressed recovery archive:
  `/home/yangchenghui/suda/backups/pre_lwe_encrypt_bitgen_2026-06-16/old_shell_recovery_outputs_2025.tar.gz`
- Archive size:
  `833M`
- Included old output/project paths relative to:
  `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd`
  - `work_farm/fpga/vivado_out/shell_virt_one_drive_fidus`
  - `work_farm/fpga/vivado_out/shell_virt_one_drive_pcie_ep_fidus`
  - `work_farm/fpga/vivado_prj/shell_virt_one_drive_fidus`
  - `work_farm/fpga/vivado_prj/shell_virt_one_drive_pcie_ep_fidus`
  - `work_farm/hw_plat/shell_virt_one_drive_fidus`
  - `work_farm/bootstrap`
  - `work_farm/software/arm-atf`
  - `work_farm/software/arm-uboot`
  - `shell/virt_one_drive/ready_for_download/fidus`
- Spot-checked archive entries:
  - `work_farm/fpga/vivado_out/shell_virt_one_drive_fidus/dcp/route.dcp`
  - `work_farm/hw_plat/shell_virt_one_drive_fidus/system.bit`
  - `work_farm/bootstrap/pmufw/pmufw.elf`
  - `work_farm/software/arm-atf/bl31.elf`
  - `work_farm/software/arm-uboot/u-boot.elf`
  - `shell/virt_one_drive/ready_for_download/fidus/BOOT.bin`
- Added backup README:
  `/home/yangchenghui/suda/backups/pre_lwe_encrypt_bitgen_2026-06-16/README.md`
- Note:
  `system.bit` and `BOOT.bin` are enough for direct rollback, but the archive is what protects old build/debug/recovery artifacts before the next full bitstream generation overwrites them.

Update: SUDA Host application for FPGA runtime validation
- Added:
  `/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload`
- The application follows the SUDA NVMe CS flow:
  - creates input/output SLM namespaces
  - creates a two-range Memory Range Set
  - loads and activates a one-operator hardware program
  - requests `lwe_encrypt` by `operator_type_id = 2`
  - sends one clear `u8` input
  - initializes one 4KB operator context page
  - receives four 2048-dimensional radix Big-LWE ciphertexts
- The context builder matches the HLS contract:
  - `mask_dimension = 2048`
  - `request_count = 1`
  - `input_mode = u8 radix`
  - `noise_bound_log2 = 17`
  - `delta = 1 << 59`
  - fresh seed and nonce by default
  - the 2048-byte binary Big-LWE key is packed one coefficient per bit from byte 64
- Added Host-side ciphertext verification using:
  `body - dot(mask, secret_key)`.
- The application recognizes both compact output and the current
  `OperatorController` 64-byte-padded layout. The current controller forces
  AXIS `TKEEP` to all ones, so the expected deployed layout is padded.
- The output is written in the existing `LWEHLS01` format for optional
  tfhe-rs `ClientKey` verification.
- Registered the application in:
  `/home/yangchenghui/suda/host/applications/Makefile`
- Added `lwe_encrypt` to the board software-stack operator table:
  `/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/config.json`
  with:
  `operator_type_id = 2`, `slot_id = 2`.
- Local build validation:
  `make -C /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload`
  completed successfully.
- CLI smoke validation:
  `vscode-lwe-encrypt-offload --help`
  completed successfully.
- JSON validation:sud
  `python3 -m json.tool device/platform/software_stack/nf_spdk/config.json`
  completed successfully.

Update: SSD-backed Host input path
- Changed the Host application input flow from direct Host-to-SLM writing to:
  `board SSD -> nvme_slm_copy -> input SLM -> lwe_encrypt`.
- Added command-line selection of the physical source location:
  `--ssd-nsid` (default `1`) and required `--ssd-lba`.
- The offload application only reads existing SSD data and does not write or
  generate plaintext.
- The clear `u8` is read from byte 0, and only the first 64-byte beat is
  exposed to the FPGA operator.
- `--ssd-lba` is mandatory so the application cannot silently overwrite LBA 0.
- Added a separate Host application:
  `/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-data-gen`.
- The data writer loads one fixed 4KB `u8` plaintext file, writes it to a
  selected SSD namespace/LBA, and verifies the complete page by NVMe
  readback. It prints the first `u8`, the first 16 bytes, and an FNV-1a
  checksum for the follow-up encryption test.
- Generated and saved one fixed 4096-byte random `u8` plaintext:
  `/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-data-gen/testdata/plaintext_u8_4k.bin`.
- Fixed plaintext SHA-256:
  `1d26e5894d74eb78e0e176732f567602a8cb7161d7cd16a49764123bc3c889b8`.
- Updated the data writer to load this file by default, ensuring subsequent
  SSD/offload tests reuse identical plaintext instead of regenerating data.

Update: on-board execution hang diagnosis and stream-start fix (2026-06-24)
- The first FPGA runtime attempt stalled inside
  `nvme_execute_hlsacc_program`.
- ARM initially reported:
  `mcdma.c: tx_rx_channel_poller: ERROR! DATA USED IS BIGGER THAN NEEDED!`
- Root cause 1 was in the ARM MCDMA TX descriptor construction: a 64-byte
  virtual object was submitted as a 4096-byte descriptor. Updated
  `device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c` so the final
  descriptor uses the actual remaining byte count.
- Rebuilt the ARM `nvmf_tgt` successfully. The rebuilt AArch64 binary SHA-256
  was:
  `d4b11e173e25532209cb696e4006672b8e6d701030acc241e57bee14dd65d436`.
- After fixing the MCDMA length, execution still stalled. Inspection of the
  SUDA runtime showed that the operator APPLY command is issued before the TX
  channel starts sending input data.
- Root cause 2 was the HLS top-level `data_in.empty()` check. The deployed RTL
  could observe an empty stream immediately after `ap_start`, skip the read,
  assert completion, and leave the runtime waiting for an output stream that
  would never arrive.
- Changed `lwe_encrypt.cpp` to use a blocking `data_in.read()`. The synthesized
  state machine now remains in its input wait state while `TVALID=0`.
- Validation:
  - HLS CSIM passed with the 2048-dimensional, four-radix-block decrypt check.
  - C synthesis and RTL generation passed.
  - XSIM elaboration completed, but COSIM was interrupted at simulator launch
    by Vivado 2020.2 reporting `ERROR: unknown error occurred`; no RTL mismatch
    was reported.
- Exported the updated `lwe_encrypt_ip.zip`, SHA-256:
  `b071b2ad664522204b65ca7b9453d72a429de6c5df6fcd57cca441c6763f8680`.
- Copied the regenerated Verilog modules into:
  `device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt`.
- A full accframework/shell bitstream and `BOOT.bin` regeneration is required
  before this stream-start fix can be tested on the board.

## 2026-07-15：连续 u8 批量加密支持

### 修改目标

将 u8 radix 模式从“每个 512-bit 输入包只读取 byte 0”改为“连续读取输入流中的
每个有效 u8”，并允许 Host 通过 `--plaintext-bytes N` 精确指定本次加密的字节数。
SSD 和 SLM 仍按 4KB 粒度搬运，但 HLS 只加密 context 中声明的前 N 个字节。

### HLS 修改

- context 配置字 `[63:32]` 在 u8 radix 模式下改为 `plaintext_bytes`。
- 每个 512-bit AXIS beat 最多连续处理 64 个 u8；支持跨 beat 和非 64B 对齐长度。
- HLS 在处理完 N 个字节后继续排空页对齐输入，直到收到独立的 SUDA
  `TUSER=0xff` done 包，再转发 done 包结束任务。
- 若 done 包到达时有效输入不足 N 字节，输出错误码 4，避免静默生成不完整密文。
- 新增 `LWE_ENCRYPT_NOISE_ZERO=2`。连续 packed-u8 模式不再把同一个输入包的
  其余字段解释为四组外部噪声；支持内部原型噪声或显式零噪声。
- byte lane 读取采用“固定取低 8 bit，再右移 8 bit”的顺序扫描，避免生成大型
  可变选择网络。该优化将 HLS LUT 估算从 36,733 降至 31,045。

### Host 修改

- 新增 `--plaintext-bytes N`；旧 `--encrypt-count` 暂时保留为兼容别名，但新语义
  同样是连续明文字节数。
- 未指定 `--input-lbas` 时，自动使用 `ceil(N / 4096)` 个输入 LBA。
- input SLM 大小为 `input_lbas * 4096`。
- 每个 u8 的物理密文 payload 为 65,792B，output SLM 计算为：
  `align_4KB(N * 65792 + 64)`。
- 128B 明文需要 8,425,472B output SLM；256B 明文需要 16,846,848B。
- 增加 signed 32-bit runtime SLM 长度检查；当前单任务最大允许 32,640B 明文，
  实际推荐先从 128B、256B 分批测试。
- program 的预计执行时间和最大响应时间按明文字节数线性放大。
- 修复 `LWEHLS01` 批量 dump header：旧代码误把 `clear_count` 写入 version 字段，
  单字节时二者都等于 1，因而未暴露；现在正确写为
  `version=1, mask_dimension, input_count`，可被 HPU mockup 批量导入。

### 验证结果

- Host 应用 `make clean all` 编译通过；仅有 libnvme 既有头文件告警。
- HLS CSIM 使用 65B 连续明文，跨越两个 AXIS beat，第二个 beat 的 TKEEP 仍为
  全有效，以验证 `plaintext_bytes` 能精确停在 byte 65。
- CSIM 对 65 个 u8、260 个 radix Big-LWE 的 mask、body、内部噪声和解密结果逐项
  核对，结果为 `CSim done with 0 errors`。
- HLS C synthesis 通过：目标 250MHz，Estimated Fmax 302.82MHz，LUT 31,045，
  FF 14,201，DSP 8；mask 输出循环 II=3 的既有警告仍存在。
- deep test 通过 C++ 语法检查，并已同步 packed-u8/zero-noise 输入格式。

独立日志：

- `device/operators/hls/lwe_encrypt/vitis_hls.csim_batch_u8_20260715.log`
  SHA-256：`3515b8acd0e0482255371d651a76d672fde66f8825d3a3c92a05d60a6edb595c`
- `device/operators/hls/lwe_encrypt/vitis_hls.csynth_batch_u8_20260715.log`
  SHA-256：`62917e673e1d539ab2d0f2e941ff77b72e0770b9e83c0e21b493471e13aed22e`
- `device/operators/hls/lwe_encrypt/vitis_hls.cosim_batch_u8_20260715.log`
  SHA-256：`9a49a9ecbdf328e6517bddb67fd1e5740c7f32a8e9272dd83a42f4459b2997c1`

### 尚未完成

- 本版 RTL COSIM 已尝试：C testbench、RTL testbench 生成、Verilog 编译和 snapshot
  elaboration 均完成，但启动 `xsim` 时 Vivado 2020.2 再次报
  `ERROR: unknown error occurred`，因此没有 clean pass，也没有出现 C/RTL 数据不一致报告。
- 尚未运行完整 Vivado implementation timing 和板上 128B/256B 测试；当前
  CSIM/CSYNTH 通过不等于新逻辑已经进入 FPGA。

## 2026-07-15：批量版 RTL 导出与算子池更新

### 执行过程

- 从 `device/operators` 执行 `make lwe_encrypt_hwop`，使用仓库固定的
  Vitis HLS/Vivado 2020.2 流程重新综合并导出批量版 IP。
- `export_design` 首次仍受 Vivado 2020.2 日期型 `core_revision` 溢出问题影响，
  随后 `run_hls.tcl` 的既有 fallback 将 revision 固定为 1 并成功完成 IP 打包。
- 新 IP 包为：
  `device/operators/hls/lwe_encrypt/lwe_encrypt_ip.batch_u8_20260715.zip`。
- 新 IP SHA-256：
  `6ae644f10627d53d0a3dc2f141af2351f279a0cbde0f7a1628d8f931fce81d11`。
- 独立保存本次 RTL 导出日志：
  `device/operators/hls/lwe_encrypt/vitis_hls.rtl_gen_batch_u8_20260715.log`。
- 导出日志 SHA-256：
  `6154a88c23d9920453089395f24014bf0f9790793dd72238c8740b8b50a10b51`。

### RTL 更新与核验

- 更新前完整备份旧算子池 RTL 到：
  `backups/lwe_encrypt_pool_before_batch_u8_20260715`。
- 旧版顶层 `lwe_encrypt.v` SHA-256：
  `7a96261e7e8a12a7c8f2080d6d16cb80902f180f8e5d5d59ac2b2dd02e1c0d4e`。
- 将新 IP 中 `hdl/verilog/*.v` 的 11 个 Verilog 模块更新到算子池目录：
  `device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt`。
- 新版顶层 `lwe_encrypt.v` SHA-256：
  `c50a2600c1fb70a01ca537aceaf5a2464ce88978343aaf1c2b5de4f8fc56d5f4`。
- 逐文件按字节比对，确认算子池内 11 个 Verilog 文件与新 IP ZIP 中的内容完全一致。
- 在新版顶层 RTL 中确认存在 `input_count`、`processed_count`、`packed_data` 和
  `packed_keep` 等批量连续 u8 处理状态，不是 7 月 10 日的旧单字节 RTL。
- `prj_setup.tcl` 已通过 `lwe_encrypt/*.v` glob 引入该目录，无需再次修改工程脚本。

### 后续步骤

- 在 `tmux` 中运行完整 `build_bd.sh`，生成新的整板 `BOOT.bin`。
- 必须检查 implementation timing；确认 bitstream 生成和 timing 状态后再上板。
- 建议按 1B、64B、65B、128B、256B 的顺序进行板上回归测试。

## 2026-07-16：批量版整板构建日志检查与失败原因

### 检查结论

- `build_bd_20260715_batch_u8.log` 最后显示 `Bootimage generated successfully`，并在
  `ready_for_download/fidus` 下生成了 `BOOT.bin` 和 `zynqmp.dtb`，但这只代表最后的
  Bootgen 打包步骤成功，不代表前面的 Vivado 工程生成成功。
- 算子池 DCP 阶段报告：`The following module(s) are not found in the project:
  lwe_encrypt`，生成的 `accframework.dcp` 只有 10,792B，是缺少正常接口的空壳 DCP。
- shell `prj_gen` 随后报告 `Netlist 29-77`：新 `accframework_wrapper` 与 shell 中
  `accframework` 占位单元接口不匹配，缺少 4,040 个端口，Vivado 在此处退出。
- 当时的 `build_bd.sh` 没有启用失败即停，后续流程继续打开
  `2026-07-10 18:22:38` 的旧 `synth.dcp`，最终实现和 Bootgen 因此仍能完成。
- 由此生成的 `BOOT.bin` SHA-256 为
  `1511caa05f4a23c82d98caaef556575c4b6e68575e0bf8cb222754fb1f1edbfa`，
  不能作为批量 u8 RTL 已进入 FPGA 的证据，不用于本次上板测试。
- 旧 `synth.dcp` 的 post-route timing 仍未 clean：WNS=-0.009ns、TNS=-0.009ns，
  仅有 1 个 setup failing endpoint；布线本身完成，routing error 为 0。

### 根因与修正

- 根因是旧 RTL 备份曾放在递归导入的 `hlsaccframework` 源码树内。Vivado 同时读入
  新旧两套同名 `lwe_encrypt` 模块，产生重复定义并使 BD Tcl 无法解析模块引用。
- 已将备份移出源码树，保存到：
  `backups/lwe_encrypt_pool_before_batch_u8_20260715`。源码树内现在只保留当前版本的
  `hlsaccframework/lwe_encrypt` 目录。
- 已在 `build_bd.sh` 开头增加 `set -euo pipefail`，后续任一 make/Vivado 阶段失败时
  立即停止，不再使用旧 DCP 继续生成一个表面成功的 `BOOT.bin`。

### 重新构建前的验证要求

- 重新生成 `accframework.dcp` 时不得出现 `module(s) are not found: lwe_encrypt`、
  重复模块定义或 `Netlist 29-77`。
- 新 `synth.dcp` 的时间戳必须晚于本次构建开始时间，不能继续使用 7 月 10 日版本。
- 完整实现后再次检查 post-route WNS/TNS、route status 和最终 `BOOT.bin` 哈希。
# 2026-07-16：批量测试第二次加载返回 138

## 现象

单字节测试成功后，使用同一个 `--program-id 11` 运行 128B 连续明文测试，程序在 `nvme_load_hlsacc_program` 阶段返回 `138`。此时 FPGA 尚未开始执行，故该错误与 128B 密文输出大小以及 HLS 加密逻辑无关。

## 原因

`138` 的十六进制值为 `0x8a`，在 SUDA/SPDK 中对应 `SPDK_NVME_SC_INSUFF_PROGRAM_RESOURCES`。ARM runtime 的 `spdk_hlsacccompute_add_program_with_id()` 要求目标 `program_list[program_id]` 为空；原 host 程序退出时仅调用 `nvme_deactivate_program()`，没有调用 `nvme_unload_hlsacc_program()`，导致上一次成功测试留下的 program 11 持续占用槽位。下一次向同一槽位加载程序时因此返回 138。

## 修复

- 加载程序前先调用 `nvme_unload_hlsacc_program()`，清除上一次异常退出或旧程序遗留的同 ID 槽位。
- 使用独立的 `program_activated` 状态记录程序是否已经激活。
- cleanup 阶段按 `deactivate -> unload` 顺序释放程序，并打印清理失败告警。

该修改只涉及 host 程序的 program 生命周期管理，不需要重新生成 HLS RTL 或 BOOT.bin。

Host 程序已执行 `make clean all` 并编译通过；新二进制 SHA-256 为
`4949d443828c83a0124df03592b560d836360c62986abfaf4cb2d011c52abacb`。
