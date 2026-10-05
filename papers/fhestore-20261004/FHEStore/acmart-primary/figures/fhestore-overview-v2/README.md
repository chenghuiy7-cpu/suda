# FHEStore system overview, v2

This revision makes the host's two roles explicit: submitting the operator task
to the computational storage device and relaying ciphertexts between that device
and the remote FHE service. The author requested these changes on 2026-10-04,
along with thinner lines, restrained color, and more regular alignment.

## Files

- `fhestore-overview.svg`: editable native vector source and text.
- `fhestore-overview.pdf`: paper-ready vector export at 178 x 89 mm.
- `fhestore-overview.png`: preview rendered from the vector source.
- `build_overview.py`: reproducible SVG generation and export script.
- `figure.tex`: two-column inclusion snippet, caption, and accessible description.
- `design-reference.png`: built-in ImageGen edit of the v1 preview.
- `imagegen-prompt.txt`: exact prompt for that edit.

## Meaning and evidence

The three columns are FHEStore CSD, Host, and Remote FHE Service. The host task
runtime submits an operator graph, per-operator contexts, and data ranges; the
device runtime configures execution of existing operators. This is task
submission, not FPGA bitstream upload.

The outbound path is storage -> optional preprocessing -> encoding/encryption
-> host input ciphertext relay -> remote service. The return path is remote
service -> host result ciphertext relay -> decryption/decoding -> write-back
-> storage. Both paths explicitly traverse the host. Write-back is a system
operation and can involve host-assisted record read-modify-write.

Solid arrows represent logical data flow; gray dashed arrows represent task
control. The overview omits device memory allocations, DMA, driver interfaces,
consumer-specific layouts, and HPU internals at the author's requested level
of abstraction. It contains no measured data or performance claims.

Evidence is the author-approved Chapter 3 discussion and the SUDA implementation
inspected in `host/applications/vscode-lwe-full-pipeline/`,
`host/applications/vscode-selective-lwe-full-pipeline/`, and `hpu/remote-hpu/`.
The repository HEAD used for the original source analysis was
`d679e93603f0737f8e489195d5aaf4bba7a17c56`, with existing local working-tree changes.
An independent code reviewer confirmed the task submission and both relay paths.

## Production

The built-in ImageGen tool edited the v1 preview to establish the new composition.
The final figure is reconstructed with native SVG shapes, paths, and editable
text. The reconstruction uses flat fills, controlled line weights, and aligned
ports. It corrects the reference's FPGA-only boundary around storage and
write-back to a functional storage-side data path.

The original generated reference remains at
`/home/yangchenghui/.codex/generated_images/01a104d2-16af-7740-86b4-595efd656257/exec-74048e8d-75fe-40a7-8ea0-24b8f1c8941c.png`.
The v1 figure and manuscript source are preserved. This directory contains a
candidate for review; the manuscript does not yet include it.

Regenerate the named exports after editing the source:

```bash
/usr/bin/python3 build_overview.py --force
```

The export step uses `rsvg-convert`.

## Validation

The final PNG and PDF renders were inspected at the intended 178 mm width,
including 96 and 144 dpi previews. The text, relay ports, arrow directions,
and region boundaries are legible with no overlaps. The minimum text size is
8.2 pt. The SVG has 30 editable text elements and no embedded raster image;
the PDF embeds Arimo/Arimo-Bold and contains no raster image objects.
The caption describes the functional data flow and the host-assisted
read-modify-write path where applicable.
