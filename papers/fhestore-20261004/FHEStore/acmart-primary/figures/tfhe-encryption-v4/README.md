# Figure 2 — author noise-branch revision

The author supplied an updated Figure 2 inline in the conversation.
This revision synchronizes the existing editable drawio source with that
visible revision: the noise branch turns left higher before entering the
last adder, its e_ell label is relocated, lower panels are shorter, and the
arithmetic title is raised. All 21 edge endpoint pairs are unchanged.
Input w, output W, and the manuscript caption and Description are retained.

The main manuscript includes this vector PDF as Figure 2 on page 5.
The export measures 178 × 77.30 mm and contains no raster or foreign-object
elements. The eight open edges in render-checks.json are deliberate external
interfaces, width ticks and the constant-zero input inherited from v3.

## Files

- `tfhe-encryption.drawio`: native editable source.
- `tfhe-encryption.pdf`, `.svg`, `.png`: paper and review exports.
- `author-revision-preview.png`: reference-width preview.
- `revise_drawio.py`, `export_drawio.py`: reproducible revision/export tools.
- `source-info.json`, `render-checks.json`, `qa-notes.json`: provenance and checks.

## Rebuild

```sh
python3 revise_drawio.py
python3 export_drawio.py --viewer-js /path/to/viewer-static.min.js --force
rsvg-convert -f png -w 1156 -o author-revision-preview.png tfhe-encryption.svg
```

The official diagrams.net renderer and installed Chrome are used for native
export. Earlier figure revisions are preserved. No image model was used.
