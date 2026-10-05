# FHEDock 生产者—消费者叙事改写与文献核验

核验与改写日期：2026-10-03。修改范围：`CipherStore_FPGA27.tex` 的题目、摘要和 Introduction，以及相应 BibTeX 条目。系统名称保持 FHEDock。题目和摘要的这次调整由本轮写作任务授权。

## 采用的主线

**FHEDock 在存储侧把持久化应用数据转化为供 FHE 加速器使用的密文。**

生产者—消费者模型适合用来界定工作的系统边界。Producer 负责读取、轻量预处理、编码、加密、缓冲和交付；consumer 负责同态执行。FHEDock 的主要研究对象是前者的硬件算子、流式组织、控制通路和应用工作流。返回路径的解密与持久化写回作为配套能力保留。

根据作者最新意见，题目、摘要和引言使用通用的 FHE accelerator 描述 consumer。HPU 的具体名称、操作和接口留在原型与评估章节，作为密文兼容性和系统集成的验证场景。引言不把下游同态计算架构作为 FHEDock 的贡献。

新题目：

> FHEDock: Producing Ciphertexts for FHE Accelerators with FPGA-Based Computational Storage

中文含义：FHEDock：利用 FPGA 计算存储为 FHE 加速器生产密文。

引言按以下论证顺序组织：

1. 定义生产者与消费者，明确持久化数据是 producer 的起点。
2. 简要说明已有同态执行加速，把 reader 的注意力转向输入生产。
3. 承认已有 BFV/CKKS 客户端加解密、完整编码和加密硬件。
4. 承认 MemFHE 的双侧 PIM 与 CHOCO 对客户端计算、通信的共同考虑，定位 FHEDock 的存储侧组织问题。
5. 说明主机生产路径中访问、预处理、加密及密文处理的联系。
6. 用计算存储及已有存储侧同态执行文献说明这种放置方式的背景。
7. 描述 FHEDock 的生产与返回路径，接三项贡献。

三项贡献仍按作者指定的顺序：HLS 加密/解密算子；流式组织、控制和应用使用；原型与评估。Contributions 没有独立的小标题。

## 摘要中文对照

加速的全同态加密（FHE）工作流由准备并交付密文的生产者，以及执行同态计算的消费者组成。当应用数据位于持久化存储时，密文生产包括数据访问、预处理、编码、加密、缓冲和交付。仅测量同态执行或加密算子的时间，不能刻画这条路径的成本。

我们提出 FHEDock，一种基于 FPGA 计算存储的存储侧密文生产系统。FHEDock 使用高层次综合（HLS）实现 TFHE 加密和解密硬件算子，并将明文编码集成到加密中。流式组织连接存储访问、轻量预处理和密文生成，控制通路协调输入准备与返回结果处理。返回路径在 FPGA 上解密结果密文，并将恢复的值写入持久化存储。

我们在支持 FPGA 的计算存储设备上实现 FHEDock 原型，并演示独立 FHE 加速器可以对其生成的密文执行同态计算。对于 unsigned 8-bit 输入和 4 KiB 明文批次，FPGA TFHE 加密引擎相对主机 CPU 加密获得 5.33× 加速，所评估的端到端输入准备相对主机基线获得 3.26× 加速。

## 文献核验与实际引用

以下检查区分“出版身份已核实”与“正文主张的内容已核实”。Crossref 用于核对 DOI、作者、年份和出版记录；架构与工作范围依据作者公开稿、正式论文或机构原始摘要。公开全文和元数据快照保存在 `../tmp/producer-consumer-revision/`。

| 引用键 | 引言使用的主张 | 内容来源和定位 | 核验结果 |
|---|---|---|---|
| `mert2020bfv` | FPGA 加速 BFV 加密与解密 | [作者所属机构原始记录](https://research.sabanciuniv.edu/id/eprint/40087/)，Abstract；[DOI](https://doi.org/10.1109/TVLSI.2019.2943127) | TVLSI 28(2), 353–362, 2020。机构摘要明确 ENC/DEC、SEAL 与 PCIe；未获得原论文全文，不据此断言它完全排除数据处理或传输。 |
| `lee2023ckks` | FPGA 客户端 CKKS 加解密 | [出版社全文](https://mdpi-res.com/d_attachment/sensors/sensors-23-07389/article_deploy/sensors-23-07389.pdf)，pp. 1–2，Abstract、§1、Fig. 1 | Sensors 23(17), 7389, 2023；[DOI](https://doi.org/10.3390/s23177389)。客户端加解密范围与图示相符。 |
| `krieger2024aloha` | 硬件统一支持编码/加密与解码/解密 | [作者公开稿](https://eprint.iacr.org/2023/1736.pdf)，p. 1，Abstract、§I；p. 4，§III.E、Fig. 5；[作者代码仓库](https://github.com/flokrieger/Aloha-HE) | 正式发表为 DATE 2024, pp. 1–6；[DOI](https://doi.org/10.23919/DATE58400.2024.10546608)。公开稿描述 CPU/内存、AXI4 和 DMA，不能把它简化成“没有数据通路的单算子”。 |
| `yune2025abcfhe` | 流式客户端 CKKS 运算，支持 bootstrappable 参数 | [作者 DAC 公开稿 v1](https://arxiv.org/abs/2506.08461v1)，pp. 1–3，Abstract、§II.D、§III；p. 5，§V.A | **已核实正式发表为 DAC 2025, pp. 1–7**，不是只有预印本身份；[正式 DOI](https://doi.org/10.1109/DAC63849.2025.11132592)。本次内容核验使用作者公开稿，正式元数据另由 Crossref 核实，BibTeX 注释保留这个版本区分。 |
| `gupta2024memfhe` | 客户端与服务器两侧 PIM 加速 | [正式出版 PDF（NSF 存档）](https://par.nsf.gov/servlets/purl/10534250)，pp. 28:1–28:3，Abstract、§1 | ACM TECS 23(2), Article 28, 23 pages, 2024；[DOI](https://doi.org/10.1145/3569955)。采用正式版作者 Tajana Šimunić，不混用预印本的作者字段。双侧工作必须主动承认。 |
| `vanderhagen2022choco` | 同时考虑客户端计算、通信与 ENC/DEC 硬件 | [作者保存的正式论文](https://brandonlucia.com/pubs/choco-taco-asplos22.pdf)，p. 683，Abstract、§1 | ASPLOS 2022, pp. 683–696；[DOI](https://doi.org/10.1145/3503222.3507737)。这是本次检索补充的相关工作。正式题目为 *Client-Optimized Algorithms and Acceleration for Encrypted Compute Offloading*；不混用早期预印本题目。 |
| `suzuki2023isc` | CSD 与同态执行结合已经有研究 | [作者所属机构原始记录](https://waseda.elsevierpure.com/en/publications/designing-in-storage-computing-for-low-latency-and-high-throughpu/)，Abstract；[DOI](https://doi.org/10.1109/ICBDA57405.2023.10104970) | ICBDA 2023, pp. 127–136。元数据与主题已核实；未获得全文。引言使用“CSD-coupled model for encrypted computation”，不对其明文准备、缓冲或传输的缺失作排他判断。 |

已有文献键 `xu2026hera`、`zhou2023fhemem`、`vanbeirendonck2023fpt`、`ruan2019insider`、`wong2024dongle`、`ohba2025nvme` 和 `chillotti2020tfhe` 保留。沿用此前依据原论文建立的核验记录，见 `FHEDock_Introduction_写作说明.md`。FHEmem 继续引用作者提供的 2023 年 arXiv v1；DONGLE 2.0 继续使用 2024 年期刊扩展版。

## 对所给文档其他候选文献的核对

这些文献支持背景或应用讨论，但不需要全部塞入 Introduction。

| 文档中的候选 | 核对依据 | 本次处理 |
|---|---|---|
| F1，MICRO 2021 | [作者论文](https://people.csail.mit.edu/sanchez/papers/2021.f1.micro.pdf)，Abstract、§1、Fig. 1；[DOI](https://doi.org/10.1145/3466752.3480070) 与 Crossref | 可支持消费侧 FHE 程序及数据移动背景。作者 PDF 与注册元数据的作者顺序不完全一致；本次没有新增它的 BibTeX。 |
| BTS，ISCA 2022 | [作者公开稿](https://arxiv.org/abs/2112.15479)，Abstract；[DOI](https://doi.org/10.1145/3470496.3527415) 与 Crossref | 可支持 bootstrapping 与计算/存储带宽设计；不把它用作 FHEDock 输入准备瓶颈的直接测量证据。 |
| CraterLake，ISCA 2022 | [作者正式论文](https://people.csail.mit.edu/sanchez/papers/2022.craterlake.isca.pdf)，Abstract、§1；[DOI](https://doi.org/10.1145/3470496.3527393) | 可支持深度 FHE 程序的执行加速。PDF 与 Crossref 的作者字段存在差异，本次未新增引用条目。 |
| ARK，MICRO 2022 | [作者公开稿](https://arxiv.org/abs/2205.00922)，Abstract；[DOI](https://doi.org/10.1109/MICRO56248.2022.00086) 与 Crossref | 可支持同态执行中的运行时数据生成、密钥复用与数据移动；不是客户端明文加密的证据。 |
| Garimella et al.，ASPLOS 2023 | [作者公开稿 v2](https://arxiv.org/abs/2207.07177v2)，p. 1 Abstract；[DOI](https://doi.org/10.1145/3582016.3582065) | 系统范围包括 HE、SS、GC、OT，分析计算、通信、存储及请求到达；可作为整体系统测量的方法背景，不写成纯 FHE 加速器对比。 |
| BlindFL，2025 | [作者公开稿 v1](https://arxiv.org/abs/2501.11659v1)，p. 1 Abstract、§1 | 确认是 Gronberg 等的 *Segmented Federated Learning with Fully Homomorphic Encryption*，避免与 Fu 等 2022 年同名 VFL 工作混淆。本次未核实后续正式发表记录；模型更新也不必然来自持久化存储，因此未作为本文放置动机。 |
| ArcEDB，CCS 2024 | [作者公开全文](https://eprint.iacr.org/2024/1064.pdf)，§1.1；[DOI](https://doi.org/10.1145/3658644.3670384) 与 Crossref | 可支持 FHE 加密数据库查询的应用背景，不能据此声称 FHEDock 已实现 SQL/TPC-H 工作负载。本次未把它写成实验案例。 |

## 主张与已有证据的对应

| 正文主张 | 本轮保留的证据边界 |
|---|---|
| FPGA TFHE 加密加速 | 5.33×，相对主机 CPU 加密；unsigned 8-bit，4 KiB 明文批次。 |
| 端到端输入准备加速 | 3.26×，相对主机基线；同一输入与批次条件。没有改写成网络交付、同态执行或完整读—算—写加速。 |
| 加速器能够消费生成密文 | 已有独立 FHE 加速器集成演示；用于说明 producer 输出可用于所演示的计算。 |
| FPGA 解密与持久化写回 | 保留已有系统功能描述；不新增输出路径性能数值，也不把演示描述成完成了全部输出路径验证。 |
| 流式与控制组织 | 保留已有实现层面的描述。具体算子布局、流接口、缓冲容量、控制协议和用户 API 的证据需要在方法章节补齐。 |

生产者—消费者是本轮采用的分析与叙事视角，不能由这个视角本身推出“consumer 被饿死”“已匹配 producer/consumer 吞吐”“已消除主机传输”等实验结论。正文没有提出这些主张，也没有声称首次实现客户端加密或首次将 HE/FHE 用于计算存储。

## 写作与核验流程

本轮使用项目 `paper-writing`（引言组织与引用整合）、`anti-defensive-writing`（围绕最强系统贡献组织叙事）和 `paper-review`（检查论点、证据与修订闭合）。PDF 编译与渲染按 `pdf` Skill 执行。

修改前的 `.tex`、`.bib` 和 `.pdf` 备份位于 `backup_before_producer_consumer_20261003/`。作者提供的 Word 叙事文档及原题目摘要文档未改动。

其余正文仍含既有 Writing guide 待补充项；本轮完成题目、摘要、引言的修订，不构成整篇论文的投稿就绪判断。

### 编译与排版核验

- 使用 `pdflatex → bibtex → pdflatex → pdflatex` 构建；再运行一次 `pdflatex` 消除标签更新提示，最后一轮退出码为 0。
- 最终 PDF 为 5 页，逐页渲染为 PNG 并检查。题目、摘要、贡献列表、引用和参考文献均正常显示；未见重叠、裁切或超出版心。
- 最后一次 LaTeX 日志没有 undefined citation、undefined reference、Overfull 或要求重新生成交叉引用的提示。
- 文献库共 14 个唯一引用键，正文使用的 14 个键全部能够解析。已有 7 个条目保留，新增 7 个核实后的条目。
- 摘要约 189 个英文词，引言（含贡献列表）约 734 个英文词。题目、摘要和引言中的 HPU 出现次数均为 0；具体原型仍在评估章节描述。
- 比较修改前备份，`Background and Motivation` 起的正文内容未变；标签、公式、匿名 `sigconf` 设置和原编译入口保留。
- BibTeX 对三个 IEEE 会议条目提示缺少可选出版地址，未为消除提示而补猜测地址。最终 LaTeX 仍有 ACM reference format 设置及 `balance` 调用位置的样式提示；这是当前草稿的排版设置，逐页检查未发现由此产生的显示缺陷。
- 本轮自审完成：已承认相关客户端和双侧系统工作；consumer 不作为本文设计贡献；两个加速比没有扩大测量边界；未新增未测量性能或首创性主张。
