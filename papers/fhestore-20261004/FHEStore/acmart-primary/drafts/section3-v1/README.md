# Section 3 first draft

Author request: draft Chapter 3 from the supplied CSD--Host--Remote FHE Service
architecture and the current SUDA implementation. Chapters 4 and 5 retain the
agreed hardware and pipeline scopes; their template prose is not used as evidence.

## Deliverables

- `system-overview.tex`: English manuscript section, preserving `sec:architecture`
  and `fig:pipeline` for eventual replacement of the main manuscript's template.
- `system-overview-zh.md`: Chinese reading copy for discussion.
- `preview.tex` / `preview.pdf`: ACM two-column standalone section preview.

The main manuscript and the older `CipherStore/samples/system_overview.tex` were
read for context but are preserved. This version is independently reviewable.
The preview uses the previously verified v3 architecture export; the supplied
image in this turn uses a rectangular remote-service endpoint, while v3 uses a
cloud. Both express the same actors, host relay, and interconnect boundaries.
Replacing the preview artwork with the author's final export requires only the
includegraphics path; this task does not redraw that figure.

## Source evidence

Repository HEAD: `d679e93603f0737f8e489195d5aaf4bba7a17c56` (existing modified
working tree inspected on 2026-10-04). No performance numbers are added here.

| Draft claim | Source |
|---|---|
| Task fields and separate runtime profile | `host/applications/vscode-selective-lwe-full-pipeline/run_task.py:69`; task examples in `tasks/` |
| Per-operator contexts and connected filter/encryption graph | `vscode-selective-lwe-full-pipeline.cpp:726` and `:791` |
| Memory range binding and program load/activate/execute | `vscode-selective-lwe-full-pipeline.cpp:2289` |
| Logical-to-physical operator and edge mapping | `device/platform/software_stack/nf_spdk/lib/hlsacccompute/hlsacccompute.c:719` and `:1019` |
| Device-derived selection and one buffered source image | selective application README; `vscode-selective-lwe-full-pipeline.cpp:2204` |
| Request identifiers, shape metadata, host TCP relay | `host/applications/vscode-lwe-encrypt-remote-offload/lwe_remote_protocol.cpp`; selective application `:2598`; `hpu/remote-hpu/src/protocol.rs` |
| Evaluator loads evaluation material without ClientKey | `hpu/remote-hpu/src/main.rs:23` and `:82` |
| Contiguous decrypted buffer copied to SSD | `host/applications/vscode-lwe-full-pipeline/vscode-lwe-full-pipeline.cpp`, `run_decrypt_and_store`; application README |
| Selected-record Host RMW and read-back check | `vscode-selective-lwe-full-pipeline.cpp:2706` |

Application filenames without a directory in the table refer to
`host/applications/vscode-selective-lwe-full-pipeline/`.

## Scope and presentation choices

The added task table and ordered record mapping explain relationships that the
architecture picture alone does not show. Device buffers are described without
introducing SLM objects, allocation-page formats, DMA descriptors, or driver names
into the manuscript. The selected count explains output-range sizing; the two-pass
discovery protocol and its costs belong in Chapter 5. The text distinguishes
contiguous device write-back from host-assisted record updates.

The ciphertext interface is described through agreement on parameters, encoding,
and serialization. HPU-native coefficient arrangements are not expanded into a
core design topic, consistent with the author's Chapter 4 decision.

The trusted domain includes both Host and CSD. The draft does not claim that all
plaintext is absent from Host, that the relay is removed, that output persistence
is transactional or power-failure durable, or that arbitrary operator graphs and
remote query transformations are supported. Prototype CPU reference checks are
correctness instrumentation and are not characterized here as eliminated work.

## Build

From the `acmart-primary` directory:

```bash
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=drafts/section3-v1 drafts/section3-v1/preview.tex
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=drafts/section3-v1 drafts/section3-v1/preview.tex
```

This is a section draft and local preview, not a submission-readiness assessment.

## Validation

The two-column excerpt was compiled with pdfLaTeX and its local references
resolved. The final two-page PDF was rendered and inspected: the equation and
task table fit their columns, and the architecture figure and caption are
readable without clipped labels. There are no undefined references or horizontal
overflow warnings. The log retains a 1.624 pt output-vbox warning; the rendered
pages show no clipping or overlap. Equation and figure numbering are local to
this excerpt. The main manuscript source has not been edited.
