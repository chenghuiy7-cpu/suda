# 2026-10-07 排版、公式交叉引用与 Related Work 修订

> ?????????????????????????????????????? [????????](evaluation-float-placement-20261007.md)?

本记录对应最新的 12 页主稿。修改前的 13 页版本保存于 `../../tmp/layout-review-20261007/before/`。本轮按作者要求检查非内容性问题，并参考作者指定的 HERA 论文调整 Related Work 的叙述节奏。

## 2.1 的实际排版

修改前逐页渲染确认：2.1 标题和首段没有错位，标题字号与其他同级标题一致。标题后首段左齐、不缩进，是 acmart 原生规则；未修改这一规则，也未修改 section/subsection 定义。

公式 (1) 后的 `Encoding maps...` 原来仍在正确性描述的同一段中，视觉上不缩进。本轮将这段说明设为正常的新段落，并为 Encoding、LWE/TFHE、Encryption 三个意群补齐与正文一致的段落边界。未改变其中的英文句子、符号或公式。

## 非内容性修复与核验

- 将三处 `Equation~\ref{...}` 改为 `Equation~\eqref{...}`，使正文中的公式号带有与陈列公式一致的括号。
- 修改前第 10、11 页普通正文两栏末行基线分别相差 3.845 pt 和 2.824 pt。原因是紧接标题前的 `\bodypar` 含 `\vskip 0pt plus 6pt`，在标题断栏时可能留下伸展的栏底胶水。本轮统一删除六处紧接 section/subsection 的 `\bodypar`，保留自然段落结束，让标题前间距由 acmart 处理；普通正文段落之间的伸展机制保留。未使用逐标题 vspace、强制分页、字号或页边距调整。
- 修改前 References [31] 的 URL 在左栏以 `https:` 收尾，剩余内容孤立在右栏顶部。本轮给 bibliography 环境增加条目内不跨栏的断行惩罚，保留原 BibTeX、BBL 和参考文献文字。新版本 [31] 及其完整 URL 均位于第 12 页右栏。
- 公式 (1)–(9) 连续编号，公式内容及标签与修改前逐字节一致；公式标点与后文衔接正常。
- 图 1–8、表 1–5 均有正文引用，编号与对象一一对应。没有重复 label、未定义引用、`??` 或失效的内部 PDF 链接。
- 31 个文献条目、31 个 BBL 条目与 31 个正文引用键对应，无未引用、重复或未定义条目。
- 最终日志无 Overfull、未定义引用或交叉引用重编译要求。剩余 Underfull 提示对应窄栏排版、长 URL 和整条文献的断栏保护，渲染检查未见裁切或重叠。

最新视觉复核：第 2–7、9、11 页普通正文两栏末行基线均为 84.653 pt。第 8 页右栏末尾为表格，第 10 页右栏末尾为 Figure 8 图注，第 12 页为 References 与附录，这些混排位置不以普通正文的基线判断齐底。

## Related Work 的组织

参考源为作者指定的本地 HERA 论文：

`acmart-primary/acmart-primary_eva/Xu чнЙ - 2026 - HERA A Bandwidth-efficient Accelerator for Fully H omomorphic E nc r yption on HBM-enabled FPG A.pdf`

已阅读其 Related Work（Section 6，纸面 p.274 / PDF 第 10 页）和硬件工作比较（Section 2.2，纸面 pp.267–268 / PDF 第 3–4 页）。借鉴的是类别组织和叙述顺序，没有复制句子，也没有将 HERA 的跨平台性能主张移植到本文。

本稿 Related Work 改为四个左齐的简短粗体主题标记：

1. Homomorphic Computation Acceleration
2. Encryption and Client Processing
3. Computational Storage
4. Storage-Coupled FHE Evaluation

每段先说明研究类别，再用具体工作的机制展开，最后在该类别的比较轴上定位 FHEStore。删除原来的泛泛比较轴开场，以及两句不增加具体信息的总结。仍保留原 Related Work 的全部 23 个引用键，结合其他章节共 31 个文献条目。MATCHA、Strix、Biscuit、Ohba、Suzuki 的引用范围均保持已核实的机制与研究对象，没有新增性能、首创或能力缺失主张。

## 保留性与最终检查

- 题目、短标题、摘要、Introduction、Conclusion 和 Reproducibility Details 与修改前逐字节一致。
- 第 3–5 章各仅一处公式引用改为 eqref；第 6 章仅删除上述六行排版命令，正文、参数、数值、表格及数据集引用保留。
- 全部九个公式、八条 includegraphics 调用及其参数与顺序保持原样；图资产存在，未编辑图表文件。
- 文献库及 BBL 逐字节保持原样，31 条文献保留。
- Figure 2 仍在第 3 页顶部；Table 1/2 仍在第 8 页，且物理位置均低于第 6 节标题。
- 稳定编译为 12 页，已逐页渲染检查，并分别复核 2.1、实验图表、Related Work 和 References。
- 原附录仍有作者待补充的 Writing guide；本轮没有擅自补写或删除附录内容。

机器核验与视觉证据见：

- [最终交叉引用和保留性核验](../../tmp/layout-review-20261007/final-cross-reference-audit.json)
- [最终逐页视觉复核](../../tmp/layout-review-20261007/visual-reaudit.md)
- [栏末基线复核](../../tmp/layout-review-20261007/baseline-reaudit.json)
- [HERA 结构学习及改写说明](../../tmp/layout-review-20261007/related-work-proposal-notes.md)

本轮确认范围为上述排版、编号、引用对应关系和改写保留性，不涉及重新验证实验或投稿政策。
