# Author's input-w / output-W revision

This version preserves the editable monochrome diagram and applies the author's
input-width notation from the supplied revised image: `w` at the radix encoder
input and `W` at the ciphertext output. The input denotes an application value
delivered by the stream adapter; the output denotes a packed ciphertext beat.

The manuscript uses `tfhe-encryption.pdf`. Native editing uses
`tfhe-encryption.drawio`, with editable modules, grouped labels, and connectors.
`tfhe-encryption.svg` and `.png` are exports of that drawio source.

Run `python revise_drawio.py` to reproduce the notation revision from the v2
native diagram. Export it with:

```bash
python export_drawio.py --viewer-js /path/to/viewer-static.min.js --force
```

The viewer is the official diagrams.net renderer from
`https://viewer.diagrams.net/js/viewer-static.min.js`; local export requires
Chrome and `rsvg-convert`. Source and rendering records are retained in
`source-info.json` and `render-checks.json`. Eight free-end lines are deliberate:
five bus-width marks and three external input/output or zero-input lines.

Figure caption: “HLS architecture for integrated TFHE encoding and encryption.”
