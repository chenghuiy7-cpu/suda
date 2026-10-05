# FHEStore framework overview, v1

This figure follows the author's agreed Chapter 3 abstraction: host task control,
storage-side input preparation and result recovery, and an external HPU service
shown as a cloud. It is a functional overview, not a detailed physical wiring map.

## Files

- `fhestore-overview.svg`: editable vector source, including editable text.
- `fhestore-overview.pdf`: vector export, 178 x 89 mm, for two-column placement.
- `fhestore-overview.png`: 3200 x 1600 preview of the reconstructed SVG.
- `build_overview.py`: standard-library Python source that builds and exports the SVG.
- `figure.tex`: ACM-compatible inclusion snippet, caption, and description.
- `design-reference.png`: selected visual design produced by the built-in ImageGen tool.
- `imagegen-prompt.txt`: the exact generation prompt.

## Supported content

Solid arrows denote logical data flow; the dashed arrow denotes host task control.
The input path is storage -> optional preprocessing -> FPGA encoding/encryption ->
external service. The return path is external service -> FPGA decryption/decoding ->
write-back -> persistent storage. Both ciphertext handoffs are host-mediated.
Write-back is a system operation and may involve host-assisted record merging.
The overview deliberately omits SLM objects, DMA internals, specific consumer
layouts, and HPU microarchitecture, as requested by the author.

The source context is the author-approved discussion on 2026-10-04 and the SUDA
workflow inspected in `host/applications/vscode-lwe-full-pipeline/`,
`host/applications/vscode-selective-lwe-full-pipeline/`, and `hpu/remote-hpu/`.
The repository HEAD at creation was `d679e93603f0737f8e489195d5aaf4bba7a17c56`;
the inspected working tree also contained existing local changes. This is a
conceptual artifact with no experimental measurements.

## Production and checks

The built-in ImageGen tool produced the visual design. The SVG was reconstructed
with native paths, shapes, and text; it contains no embedded bitmap. The
reconstruction preserves the selected composition and icon silhouettes, uses
flat fills, increases label margins, and wraps the transfer note onto two lines.

The image and caption received independent semantic review. The reconstructed
PNG and PDF renders were visually inspected, including at the intended 178 mm
width. Arrow directions, boundaries, labels, and text spacing were checked.
Minimum text size is approximately 8.2 pt at that width. The PDF embeds Arimo
and Arimo-Bold; its dimensions and absence of raster content were checked.
The manuscript source has not been modified to include the figure.

To regenerate the three named exports after editing the generator:

```bash
/usr/bin/python3 build_overview.py --force
```

The export step requires `rsvg-convert`, already available in this workspace.
