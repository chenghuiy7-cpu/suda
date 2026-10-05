# Chapter 3 v2: author-selected overview and explicit scope

Archived on 2026-10-04. Chapter 3 has been integrated and revised in
`../../FHEStore_FPGA27.tex` to align with the main manuscript's Introduction
and Background. The full paper is `../../FHEStore_FPGA27.pdf`. These excerpt
files preserve the earlier review version; all further chapter edits and PDF
builds use the main manuscript.

This archived revision uses the unchanged author-uploaded figure
`figures/fhestore-overview-v5/fhestore-overview.png` and updates
the first System Organization paragraph, figure caption, accessible description,
and Chinese reading copy. It states that FHEStore covers the CSD and Host software
for input preparation, result recovery, task coordination, and ciphertext handoff;
the remote server supplies homomorphic evaluation through an FHE accelerator.
The uploaded figure's layout, colors, labels, and arrows are preserved. Previous
color and work-scope-bracket descriptions have been removed from the prose.
The CSD's FPGA operators are described as an operator pool, matching the figure.

The Application Task Model presents the four task components in prose, followed
by the ordered record mapping and encryption equation. At the author's request,
the task Listing, its reference, symbol definitions, and concrete selective-update
explanation have been removed. The earlier field table is not reinstated.

The trusted-domain paragraph explicitly states the Host/CSD assumption and the
actual local secret-key loading and provisioning
through execution contexts. The remote service uses corresponding evaluation
keys and receives ciphertexts. No key-vault, non-exportability, or hardware
key-isolation mechanism is claimed. Evidence: `run_task.py:95`,
`vscode-selective-lwe-full-pipeline.cpp:726`, and `hpu/remote-hpu/src/main.rs:82`,
under the directories documented in `../section3-v1/README.md`.

The ordered record mapping, operator-execution, and write-back mechanisms remain
as in v1. The code evidence and substantive scope choices are documented in
`../section3-v1/README.md`. No performance values are introduced. Runtime code
and the prior v1 draft remain unchanged.

Files: `system-overview.tex`, `system-overview-zh.md`, `preview.tex`, `preview.pdf`.
The Listing package and its preview-specific balance hook have been removed.

Historical excerpt build command, run twice from `acmart-primary`:

```bash
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=drafts/section3-v2 drafts/section3-v2/preview.tex
```

Validation: the two-page two-column preview compiles with local figure and
equation references resolved and no LaTeX warnings or overflow diagnostics.
The original 1087 x 371 pixel architecture image remains included. Both final
pages were rendered and inspected for column fit, clipping, and overlap. There
are no remaining Listing references in the section or its Chinese reading copy.
This is an archived local review excerpt; its numbering is local and its prose
predates the integrated main-manuscript revision.
