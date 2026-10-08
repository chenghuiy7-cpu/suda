# Chapter 5: storage-side ciphertext production

Draft integrated into `../FHEStore_FPGA27.tex` on 2026-10-05. The author has
restricted the paper's engineering story to ciphertext production. Chapters
1--3, the abstract, and the remaining template's scope were aligned accordingly;
the FHE correctness and decryption definitions in Background remain foundational
material. Figure 1 and Figure 2 image assets were preserved. No performance
measurement was added or changed.

Paths below are relative to `/home/yangchenghui/suda`. This evidence record is
separate from the paper-facing prose.

| Draft mechanism | Implementation evidence |
|---|---|
| SLM is the runtime's device-memory object/range abstraction | `docs/api/APIReference.md:131`; `docs/architecture/LWE加密算子原型开发与测试总结.md:193`; `device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c:3268` |
| SSD read destinations are in input SLM | `host/applications/vscode-selective-lwe-full-pipeline/vscode-selective-lwe-full-pipeline.cpp:2161`; `device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c:4299` |
| Application payload enters/leaves the same MCDMA and shared operator switch | `device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/scripts/fidus/mpsoc.tcl:2501`; `device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/scripts/accframework.tcl:479` and `:758` |
| Logical graph connects Filter to Encrypt and then output, with no intermediate memory range | `host/applications/vscode-selective-lwe-full-pipeline/vscode-selective-lwe-full-pipeline.cpp:791` and `:2290` |
| Runtime maps logical IDs to physical slots and output channel | `device/platform/software_stack/nf_spdk/lib/hlsacccompute/hlsacccompute.c:706` |
| APPLY carries contexts/connections; controllers attach next-hop TDEST | `device/platform/ips/hls/hlsacc_assscheduler/hlsacc_assscheduler.cpp:88`; `device/platform/ips/rtl/OperatorController.v:768` and `:815` |
| Input FIFO, downstream ready propagation, and default 4 KiB FIFO capacity | `device/platform/ips/rtl/OperatorController.v:728` and `:755`; `device/platform/ips/rtl/axis_fifo.v:137` and `:217` |
| Field capture, predicate evaluation at record end, scalar emission | `device/operators/hls/selective_filter/selective_filter.cpp:103`, `:174`, `:299`, `:328` |
| Encryption scalar mode receives one projected u8 per payload beat | `device/operators/hls/lwe_encrypt/lwe_encrypt.cpp:442` (prototype input width) |
| Mandatory FPGA selection discovery and manifest parsing | `host/applications/vscode-selective-lwe-full-pipeline/vscode-selective-lwe-full-pipeline.cpp:1902` and `:2206`; `host/applications/vscode-selective-lwe-full-pipeline/selection_manifest.hpp:13` |
| Generation reuses the input SLM, one SSD load and two SLM scans | Same runner `:2206`, `:2290`, `:2359` |
| CPU-order serialized ciphertext capacity and output extent | Same runner `:56`, `:2217`; per value `4*257*64=65,792` B; range `4096*ceil((M*C+64)/4096)` |
| Empty selection returns before encryption output SLM allocation | Same runner `:2249` |
| RX descriptors posted before TX begins | `device/platform/software_stack/nf_spdk/lib/hlsacccompute/hlsacccompute.c:1312` |
| Software recognizes terminal TUSER in RX completion, excludes terminal beat, reports valid payload length | `device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c:883`, `:1153`, `:1691`; host verifies payload count at runner `:2439` |
| Bounded sequential batches and source-index offsets | `host/applications/vscode-selective-lwe-full-pipeline/run_task.py:147` and `:217`; native runner `:1978` |

Figure 3 depicts the production pass's route through a single shared switch.
The first implementation-oriented drawing and its export records are retained
in `../figures/ciphertext-production-v1/`.
The discovery pass is explained in Section 5.2 and is not depicted as a second
physical datapath.

The draft states direct stream handoff between preprocessing and encryption
within the generation pass. It does not characterize the entire selective
workflow as a single scan or describe the discovery manifest as ciphertext.
The host receives the manifest, including projected plaintext values, within
the trusted domain defined in Chapter 3.

## Method-oriented Figure 3 revision

The author supplied a conceptual layout with memory above a shared stream
switch, operators attached through ports, and route binding at the side.
Revision 2 uses that organization to explain task-graph binding and stream
routing. It replaces the depicted MM2S/S2MM/FIFO implementation with memory
endpoints, one streaming DMA endpoint, operator ports, and runtime bindings.
The selected generation path remains preprocessing followed by encryption
and memory output. A generic operator endpoint and ellipsis express the pool
abstraction rather than a specific number of installed or concurrent operators.

The operator-to-operator logical destination is a pair of operator and input
port identifiers. The runtime remaps the operator identifier to a physical slot.
Logical terminal destinations use the reserved high nibble `0xf`, and are
remapped to allocated DMA RX channel tags (current channels 5/6/7). The figure
therefore labels logical destinations and a memory receive endpoint rather than
using `0xf0` as an actual on-stream DMA destination. The switch decodes the
mapped tag; the runtime supplies controller next-hop state, not a new per-task
switch decoder table. Evidence: `hlsacccompute.c:739`, `mcdma.c:1751`, and
`OperatorController.v:768` under the paths listed above.

The manuscript opening, routing paragraph, Figure 3 caption, and Description
were updated together. Revision 2 editable/export assets are in
`../figures/ciphertext-production-v2/`; existing Figures 1 and 2 are preserved.

Revision 2 final validation: native drawio export has 32 vertices and 13 edges,
with no unresolved/unrendered cells, raster elements, or foreign objects.
Root inspected the final 178 mm preview and the actual whole-manuscript page 6
at 130 dpi. Input/output aliases are attached to their memory ranges, and their
boxes were widened to retain label padding. The whole paper compiled to 8 pages
with no fatal errors or undefined references. Figure 1 and Figure 2 hashes,
citation keys, and speedup values are unchanged.

## Draft validation

- Whole manuscript compiled with `latexmk -pdf -interaction=nonstopmode
  -halt-on-error FHEStore_FPGA27.tex`; the final PDF has 8 pages, with Chapter 5
  across pages 4--6 and Figure 3 on page 6.
- Final log has no undefined references or fatal errors. The existing
  Background inline-math overfull box (1.97366 pt, lines 82--83) remains.
- Compared with the pre-edit source: all citation keys and every occurrence
  of the two measured speedup values, 5.33 and 3.26, were preserved.
- Root inspected rendered whole-manuscript pages 4--6 at 130 dpi, including
  the actual two-column Figure 3 placement, label readability, and output
  allocation equation. New Chapter 5 has no writing-guide placeholders.
- Figure 3's official drawio renderer reports 26 native vertices, 16 connected
  edges, no unresolved or unrendered cells, and no raster or foreign-object
  elements in the SVG. Figure export and an independent agent's topology
  review agree with the source-code route.

## Single-column Figure 3 revision

Revision 3 responds to the author’s request for a simpler, more refined
single-column figure. It retains input/output device memory, one shared
stream switch, preprocessing, integrated encoding/encryption and dashed
task-graph bindings. It removes separate DMA components, generic endpoints,
port labels, destination tuples, the runtime panel and legend. The omitted
adapter and routing details remain in Section 5.1. The figure Description
identifies optional preprocessing and ciphertext output.

The main manuscript uses `figure` and `\columnwidth` with the v3 PDF asset.
The export is 85 × 57.87 mm. The official renderer reports 10 vertices and
8 edges, with no unresolved/unrendered cells, raster elements or foreign
objects. The embedded-font vector export, compact preview and actual
single-column manuscript placement were inspected. An independent
architecture agent confirmed the shared-switch topology. Figure 3 now
appears on page 5 of the 8-page whole manuscript; Figures 1 and 2 retain
their earlier checksums. This revision changes only the Figure 3 inclusion,
caption and Description in the manuscript.

## Author-supplied Figure 3, revision 4

The author supplied a new operator-interconnect diagram and requested its
insertion at single-column width while discussing Chapter 5's structure.
The original 686 x 523 PNG was recovered from the current conversation's image
record and retained byte-for-byte in `../figures/ciphertext-production-v4/`.
The main manuscript includes `author-upload.png` at `\columnwidth`; its caption
identifies the depicted path as an example stream route and the dashed blue
arrows as data flow. The existing Chapter 5 prose remains unchanged.

The whole manuscript compiled successfully to seven pages. Figure 3 appears
at the upper right of page 5. Root inspected the actual PDF page render:
labels and arrows are legible and no clipping or overlap is visible. Source
outside the Figure 3 environment is identical to the before-edit snapshot.
Figure 1 and Figure 2 asset hashes, citation commands, and the speedup values
are unchanged.

For the subsequent prose discussion, distinguish the diagram's illustrative
ports and terminal endpoint from the deployed route. Current operator
controllers use `FIFO_NUM=1`, and the production graph uses port 0. Logical
terminal destinations `{F,p}` are remapped to the allocated RX tag before
stream execution. If the diagram retains physical Slot ID and TDEST labels,
the terminal edge should use the mapped tag `r`. The scheduler supplies
controller next-hop state; it does not rewrite the shell's static switch
decoder. DMA descriptors supply memory addresses and lengths.

## Concise Chapter 5 rewrite aligned to the author's figure

Date: 2026-10-05. The author requested a complete Chapter 5 revision with
logical, non-repetitive prose and a light connection to preprocessing in the
preceding sections. The author also confirmed that Figure 3 is the method-level
explanatory abstraction; its route notation governs this presentation rather
than a detailed account of physical remapping. This supersedes the preceding
editorial suggestion to change the figure's terminal label.

The revised chapter keeps two subsections:

- Section 5.1 explains SSD-to-SLM loading, DMA memory endpoints, destination-tagged
  routing and scheduler/controller configuration, then uses selection and
  projection under the established `Q` as the composition example. The direct
  stream handoff claim explicitly applies to ciphertext generation. FIFO and
  backpressure are described without listing controller parameters.
- Section 5.2 derives the output receive extent from `M`, `C`, allocation
  alignment and the terminal beat. It connects selection discovery to allocation
  and generation, then explains valid-length reporting and sequential bounded
  batches. The ciphertext-size derivation remains in Section 4.

The figure's `{3,1}` and `{F,2}` are explicitly introduced as example route
notation. The manuscript states that the scheduler configures source next-hop
destinations and that controllers attach tags; it does not claim per-task
switch-decoder reprogramming. Hardware remapping evidence remains available in
this note without becoming a repeated implementation caveat in the prose.

Read-only reviews by `code_workflow` and `architecture_outline` checked the
implementation facts and cross-section coherence. The latter's scope refinement
was applied to distinguish generation from discovery.

Validation: the whole manuscript compiled successfully to seven pages. Chapter 5
and the single-column Figure 3 appear on page 5. Root inspected the actual page
render for figure legibility, equation layout, paragraph flow and clipping.
Source outside Chapter 5, all labels and citation commands, the reported speedup
values, and hashes of all three active figures are preserved. No new figures,
tables, results, or citations were added. The only overfull box remains the
pre-existing Background box at source lines 82--83.
