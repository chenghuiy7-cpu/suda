# Chapter 3 and Figure 1: input production revision

Date: 2026-10-05

## Scope

The author's revised scope is storage-side input ciphertext production. Figure 1 and Chapter 3 follow that scope. The revised source is integrated into `FHEStore_FPGA27.tex`; no standalone chapter document is produced.

## Narrative organization

- **System Organization:** identifies the CSD and host components, PCIe connection, host ciphertext transfer over the network, and local key context. The remote server appears in one sentence to position the external consumer.
- **Application Task Model:** defines `T=(D,Q,E,O)`, the production transformation, source order, and the association between ciphertext bundles and selected records.
- **Operator Graphs and Device Execution:** explains lowering the task to an operator program, contexts, and memory ranges; device configuration; valid output byte counts; and application completion. Section 5 retains the detailed routing and output management explanation.

Network delivery is explained in System Organization. The other two subsections describe task semantics and execution without repeating that architecture paragraph. No task table or code listing is added.

## Figure revision

Figure 1 uses `figures/fhestore-overview-v8/fhestore-overview.pdf` and its editable native drawio source. It preserves the author's shallow-color component layout. Configuration and completion control arrows terminate at Operator Pool rather than the storage-side grouping boundary. The host function is labeled Ciphertext Transfer. The three blue data arrows form the forward production path: persistent data to Operator Pool, Pool to host, and host to external consumer. Returned ciphertext and persistent-data write-back arrows are removed.

## Verification

- The whole manuscript compiled with `latexmk -pdf -interaction=nonstopmode -halt-on-error FHEStore_FPGA27.tex`; exit status 0, seven PDF pages.
- Chapter 3 occupies page 3, and Figure 1 appears at the top of page 4. Actual page renders 3--5 were inspected: figure labels, arrows, caption, equation, and adjacent sections are legible, with no clipping or overlap.
- Source outside Chapter 3 is unchanged relative to the before-edit snapshot. Citation commands, Chapter 3 labels, and the existing speedup values are preserved.
- Figure 2 and Figure 3 asset hashes are unchanged.
- Chapter 3 contains no decryption, returned-ciphertext, relay, or write-back narrative.
- Native drawio rendering checks and asset provenance are recorded in the Figure 1 directory. The raster attachment is a visual reference; the revised asset is an editable vector reconstruction from the existing author-approved source, rather than a claim of bitwise raster identity.

The build retains a pre-existing 1.97 pt overfull box in Background at source lines 82--83. It has no bearing on this revision.
