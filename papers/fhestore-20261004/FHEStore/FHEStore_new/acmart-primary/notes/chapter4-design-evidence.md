# Chapter 4 design evidence and reference decisions

Revision date: 2026-10-05. Current author-requested scope: remove decryption IP
content from Chapter 4 of `FHEStore_FPGA27.tex`. The author reports that current
FPGA decryption does not outperform the CPU baseline. This edit introduces no
timing values or new performance claims. Earlier chapters and the evaluation
template are unchanged, pending a separate story revision. The chapter retains
CPU natural coefficient order. No new hardware experiments were run.

## Chapter organization

Title: **HLS Design of TFHE Encryption**. Two subsections cover
representation/interfaces and integrated encoding/encryption. The parallel
decryption/decoding subsection, its equations, and its boundary-reduction figure
have been removed from the manuscript. Figure 2 now shows the encryption IP
alone, structurally reusing the previous figure's upper panel. Operator graph
composition and filter/projection optimizations belong to the later pipeline
design discussion.

Following the author's parameterization request, Figure 2 and the radix formulas
now use symbolic widths and counts: W (stream), q (torus coefficient), w
(application value), p (message bits per digit), K (radix blocks), and L=W/q
(coefficients packed per stream beat), with w=Kp. The evaluated prototype instantiates W=512,
q=64, L=8, w=8, p=2, K=4, n=2048, and Delta=2^59. These are architecture
notation and an explicit prototype instance, not a claim that the implemented
radix path supports arbitrary run-time parameter changes. The earlier
decryption figure and implementation evidence below remain archived for
traceability; they are no longer part of Chapter 4.

The old implementation-environment placeholder table, `tab:implementation`,
has its content relocated intact to the existing experimental-setup subsection,
with its float placement adjusted to keep it after the Section 5 heading. Its
unfinished fields are not replaced by mixed-layout synthesis estimates.
Introduction, Background, and System Overview retain their source text.

## Sources and claim boundaries

Code paths below are relative to `/home/yangchenghui/suda`.
Inspected repository HEAD: `e3d5b52e5862a93290e94efbe7c768e110516674`.
Source hashes and the pre-edit manuscript snapshot are retained in
`/tmp/fhestore-chapter4-20261005/source-snapshot.json`.

| Manuscript content | Primary implementation evidence |
|---|---|
| 512-bit streams and packed context | `device/shared_components/hls/hlsacc_types.hpp:9–12,33–38`; encrypt `.cpp:367–383`; decrypt `.cpp:612–623` |
| Integer torus and binary-key conditional terms | encrypt `.cpp:238–279`; decrypt `.cpp:388–393,424–432,469–489,511–514` |
| Four 2-bit radix blocks, n=2048, Delta=2^59 | encrypt `.hpp:31–39,65–66`, `.cpp:317–364`; decrypt `.hpp:12–20` |
| Packed and one-value-per-beat input | encrypt `.cpp:442–487` |
| Sequential encryption and incremental serialization | encrypt `.cpp:238–279,317–364`; no explicit CPU-path UNROLL or DATAFLOW |
| Per-block output padding | encrypt `.cpp:44–65,124–139,279`; `host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp:46–53` |
| Compact and padded CPU decryption framing | decrypt `.hpp:35–41,59–66`, `.cpp:253–254,317–327` |
| Two key-group reads and eight selectors | decrypt `.cpp:106–130,348–365,388–395,642–643` |
| Balanced prefix/suffix trees and accumulator boundary | decrypt `.cpp:424–489` |
| Short recurrence paths, DATAFLOW, shallow SRL FIFOs | decrypt `.cpp:243–244,435–436,559–609` |
| Rounded phase, two-bit extraction, radix reconstruction | decrypt `.cpp:494–520` |
| Partial plaintext packet and separate completion | decrypt `.cpp:63–84,524–555`; encrypt `.cpp:97–121,425–435` |

Here `encrypt .cpp/.hpp` means
`device/operators/hls/lwe_encrypt/lwe_encrypt.cpp/.hpp`, and `decrypt` means
`device/operators/hls/lwe_decrypt/lwe_decrypt.cpp/.hpp`.

Logical size is 2049 u64 = 16392 bytes per LWE. Beat-padded size is 2056 u64 =
16448 bytes per LWE. A u8 uses four blocks: 65568 logical bytes or 65792 padded
bytes. These are serialization sizes, not measured transfer rates.

The decryption II=1 statement is supported by the coefficient-stage reports in
`device/operators/hls/lwe_decrypt/lwe_decrypt_parallel_final/solution1/syn/report/`.
The statement is about steady-state coefficient beats with ready streams,
not one LWE or one byte per cycle. The current full-IP resource estimates include
other layout branches and are not used as CPU-only resource results.

The CPU encryption path has no explicit PIPELINE, UNROLL, or DATAFLOW directive.
The retained September synthesis schedule reports coefficient-loop II=3;
neither an II=1 encryption claim nor a replicated encryption datapath is made.

## Sampling and validation evidence

`lwe_encrypt.cpp:3–30,251,270–274` implements seeded 64-bit xorshift and a
symmetric bounded-noise sampler. `lwe_encrypt.hpp:83–85` explicitly calls the
noise source a toy prototype. Following the author's prose revision, the chapter
names the implemented generator and sampler in the datapath description;
the separate functional-prototype and secure-deployment disclaimer is removed.
It does not assert a CSPRNG, exact TFHE noise sampling, or deployment security.
Host OS seed generation does not change the coefficient generator into a CSPRNG.
Encryption performance comparisons will need to state and reconcile sampling
work at both measured boundaries when the experimental chapter is completed.

Historical board evidence for the CPU encryption arithmetic/interface exists
in `host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_cpu_layout_dense_u8_128b_256k.log`.
Old encryption cosim logs end in FAIL even though the C test prints success;
the chapter does not claim an encryption C/RTL co-simulation pass.
The October decryption cosim report records eight passing transactions,
including compact and CPU-padded cases, but its cycle counts are simulation
measurements and are not introduced as board or CPU-comparison results.

## Reference-paper presentation decisions

All references are supplied under `HLS figure paper/`. The original figure pages
were rendered and inspected; no figure or distinctive prose is copied.

| Source and locator | Reusable decision | Application and exclusions |
|---|---|---|
| James Victor Howe, *Practical Lattice-Based Cryptography in Hardware* (2018), `thesis.pdf`, PDF p148 / printed p126, Fig.4.1 | Group keys, sampling, arithmetic, and output separately | Encryption IP grouping; do not copy its public-key matrix LWE, Gaussian sampler, DSP, or mod-q datapath |
| Timo Zijlstra, *Secure hardware accelerators for post-quantum cryptography* (2020), `2020theseZilstraT.pdf`, PDF p69 / printed p64, Fig.5.2; PDF p78 / printed p73, Fig.5.9 | Show memory supply, actual parallel lanes, and reduction/feedback | Decryption eight selectors and dual reductions; binary-key selection replaces generic multipliers |
| Changdao Du and Yoshiki Yamaguchi, *High-Level Synthesis Design for Stencil Computations on FPGA with High Bandwidth Memory* (2020), `electronics-09-01275-v2.pdf`, PDF p12, Fig.10 | Expand wide interfaces into narrow lanes and show packing | 512-bit/8×64-bit decryption and output packers; no HBM controller, stencil window, or PE replication is imported |
| Zhaoxiong Yang, Shuihai Hu, and Kai Chen, *FPGA-Based Hardware Accelerator of Homomorphic Encryption for Efficient Federated Learning* (2020), `2007.10560v1.pdf`, PDF p5, Figs.3–4 | Connect scheduling to internal arithmetic units | General diagram reading order only; Paillier/Montgomery and overlapped encryption scheduling do not match this code |

## Figures and verification

The active Figure 2 is `figures/tfhe-encryption-v3/tfhe-encryption.pdf`, exported
from a native drawio source, with SVG and PNG versions in the same directory. Its canvas is
178 by 80.52 mm and contains only the encryption IP. At the author's request,
the monochrome revision follows Howe's Fig. 4.1: aligned gray functional
regions, white modules, a trapezoid selector, circular additions, black wiring,
and adjacent width labels. It reorganizes the existing encryption topology,
without copying the reference's DSP or modular-reduction architecture. The
caption is a single identifying sentence; coefficient order and the detailed
mechanism remain in the body. The author's revised input label is $w$, denoting
the value delivered by the input adapter to the radix encoder; the ciphertext
output label is $W$. Section 4.1 defines those widths by their datapath roles,
and Section 4.2 describes value extraction in packed and scalar modes. The
physical device streams still use $W$-bit AXI4-Stream ports. Figure 1 is unchanged. The earlier
`figures/tfhe-hls-v1/` and `figures/tfhe-hls-v2/` retain the complete historical
drafts, including the removed decryption figures. The native
renderer follows the author's prior preference for precise academic lines and
the current request to show real HLS operators. It is not an image-model design
or a reconstruction of another paper's figure.

The earlier color revision used the reference papers' component vocabulary:
memory banks and registers, MUX trapezoids, circular arithmetic nodes, wide-word
slots and FIFO cells. Dark outlines and headers establish the region hierarchy;
gray identifies storage/state, blue arithmetic, and warm yellow sampling sources.
These colors were chosen for the redraw, rather than claimed as measured copies
of the source-paper palettes. The archived Figure 3 gives separate lane entries
and explicit prefix/suffix vectors to clarify the balanced reductions. Following the author's
scope correction, Figure 1 retains the author-selected v6 PNG unchanged; the v7
system-diagram draft is unused. Previous versions remain untouched. The current
encryption-only revision changes Chapter 4's title, opening, interface prose,
Figure 2, and caption, and removes the decryption subsection and Figure 3.

Manuscript verification consists of source-bound claim checks, independent
read-only encryption/decryption reviews, full-paper compilation, and inspection
of the rendered figure/section pages. It does not establish submission readiness
or substitute for new hardware measurements.

The encryption-only revision is checked through full-paper compilation and
render inspection of the remaining IP diagram and Chapter 4. Source comparisons
check that Sections 1--3 and Section 5 onward are unchanged. HLS sources and
historical figures are unchanged. The existing small Background line overflow
is outside this revision.
