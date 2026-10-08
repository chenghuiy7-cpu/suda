# 2026-10-07 全文引用核验与扩充记录

本记录保存的是引用扩充完成时的 13 页版本。后续排版与 Related Work 修订已生成 12 页版本，31 条文献保持不变；最新检查见 [排版与 Related Work 修订记录](layout-and-related-work-review-20261007.md)。下文的页码和 SHA 对应当时的版本。

已通读主稿 PDF。本任务按“References 的每一条是否在参考文献之前的论文正文中有引用”核验；引用可位于 Introduction、Background、System Overview、Experimental Evaluation 或 Related Work，未将所有来源强行集中到引言。

修改前实际 References 为 19 条，19 条全部有正文引用。原 `.bib` 共 21 条，其中 Ohba 2025 和 Suzuki 2023 尚未引用，因此未出现在旧 PDF References。本轮在 Related Work 引入这两条，另新增 10 条，最终 References 为 31 条：26 篇研究论文，以及 5 项标准、数据集或软件来源。

新增的研究论文为 F1、CraterLake、MATCHA、Strix、Ibex、Biscuit 和 Regev 的 LWE 论文；另外新增 NVM Express SLM 规范、TPC-H 规范和 Intel Lab 数据集来源。

## 正文修改位置

- Introduction：基础 FHE 定义补 Gentry 引用；同态计算硬件类别补 F1、CraterLake（该组共 7 篇）；计算存储类别补 Ibex。
- Background：LWE 的含噪模线性方程概念补 Regev 来源；TFHE 的 torus-LWE 表示继续由原 TFHE 论文支持。
- System Overview：首次出现 Subsystem Local Memory 时补标准来源；没有据此声称原型通过规范符合性测试或使用某一规范版本。
- Experimental Evaluation：TPC-H dbgen 和 Intel Lab 首次出现处补数据来源；没有改动实验配置、数值、统计定义或测量边界。
- Related Work：将模板提示替换为五段比较，分别讨论通用同态计算架构、TFHE bootstrapping、输入加密与客户端系统、计算存储编程接口、存储耦合的同态计算。

比较始终区分密文消费端的 encrypted evaluation 和本文从持久明文到 Host 内存密文就绪的 production 路径。没有跨方案、跨参数或跨测量边界对比论文中的加速比。

## 新引用与启用引用的依据

访问日期：2026-10-07。下表的定位对应实际阅读版本；论文的正式出版身份通过发表页、作者原始版本或出版注册元数据核对。

| 引用键 / 当前编号 | 一级来源 | 阅读定位 | 支持的正文表述 |
|---|---|---|---|
| `samardzic2021f1` / [18] | [F1: A Fast and Programmable Accelerator for Fully Homomorphic Encryption](https://people.csail.mit.edu/devadas/pubs/micro21_fhe.pdf) | Sec. 2.2, PDF p.3: client performs encryption/decryption; F1 need not accelerate these; Secs. 3–4, PDF pp.5–8, especially Sec.4 pp.6–7: three compiler phases and decoupled transfer scheduling | F1 is a programmable homomorphic-computation accelerator with specialized vector units and an explicitly managed memory hierarchy; its compiler separately schedules off-chip transfers and cycle-level computation to reduce data movement. |
| `samardzic2022craterlake` / [19] | [CraterLake: A Hardware Accelerator for Efficient Unbounded Computation on Encrypted Data](https://people.csail.mit.edu/devadas/pubs/craterlake.pdf) | Secs. 3–4, PDF pp.4–6: boosted keyswitching and wide-vector architecture; Sec.5.4, PDF pp.8–9: direct functional-unit chaining; Sec.6, PDF p.9: compiler minimizes off-chip movement and decouples memory accesses from computation | CraterLake targets deep homomorphic computation with boosted keyswitching, a wide-vector architecture, compiler-managed transfers, and vector chaining that consumes functional-unit outputs without register-file round trips. |
| `jiang2022matcha` / [7] | [MATCHA: A Fast and Energy-Efficient Accelerator for Fully Homomorphic Encryption over the Torus](https://arxiv.org/pdf/2202.08814v1) | Sec.4.1, PDF p.3: approximate integer FFT/IFFT; Sec.4.2, PDF p.4: TGSW cluster and external-product pipeline; Sec.4.3, PDF p.5: HBM/scratchpad organization and synthesized ASIC model | MATCHA accelerates TFHE homomorphic gates by combining approximate multiplication-less integer FFT/IFFT kernels with a pipelined datapath supporting bootstrapping-key unrolling. |
| `putra2023strix` / [15] | [Strix: An End-to-End Streaming Architecture with Two-Level Ciphertext Batching for Fully Homomorphic Encryption with Programmable Bootstrapping](https://arxiv.org/pdf/2305.11423v1) | Sec.I, PDF p.2: device/core batching and spatial/temporal key reuse; Sec.IV.A–B, PDF pp.6–7: parallelism, HSC, six-stage PBS flow, three-stage keyswitch pipeline; Sec.IV.B, PDF pp.7–8: global/local scratchpads and key distribution | Strix accelerates TFHE programmable bootstrapping and keyswitching using device-level and core-level ciphertext batching plus Homomorphic Streaming Cores with pipelined functional units and scratchpads holding ciphertexts and evaluation keys. |
| `woods2014ibex` / [27] | [Ibex---An Intelligent Storage Engine with Support for Advanced SQL Off-loading](https://vldb.org/pvldb/vol7/p963-woods.pdf) | pp. 964--965, Section 3.1, Figure 1; p. 963, abstract; pp. 971--972, Section 6.3 | Ibex places the FPGA in the data path between an SSD and MySQL and processes data as a stream.; Its offload engine supports selection, projection, and GROUP BY aggregation, with a software fallback for unsupported tasks. |
| `gu2016biscuit` / [5] | [Biscuit: A Framework for Near-Data Processing of Big Data Workloads](https://class.ece.iastate.edu/tyagi/cpre581/papers/ISCA16Biscuit.pdf) | p. 153, abstract; p. 155, Section III.A; pp. 155--156, Sections III.A--III.B | Biscuit provides a flow-based programming model for composing host and SSD tasks through typed, ordered data ports.; The model separates computation within a task from coordination of task creation, execution and producer-consumer connections. |
| `regev2009lwe` / [16] | [regev2009lwe](https://cims.nyu.edu/~regev/papers/qcrypto.pdf) | Sec. 1, author PDF p. 2; publication list | LWE noisy linear equations modulo an integer |
| `nvme2025slm` / [13] | [nvme2025slm](https://nvmexpress.org/wp-content/uploads/NVM-Express-Subsystem-Local-Memory-Command-Set-Specification-Revision-1.2-2025.08.01-Ratified.pdf) | p. 7 Sec. 1.4.3.1; p. 8 Sec. 2.1 | Subsystem Local Memory standard terminology |
| `tpch2022spec` / [23] | [tpch2022spec](https://www.tpc.org/TPC_Documents_Current_Versions/pdf/TPC-H_v3.0.1.pdf) | history p. 5; Sec. 4.2.1 p. 80 | TPC-H DBGEN dataset generator provenance |
| `bodik2004intellab` / [1] | [bodik2004intellab](https://db.csail.mit.edu/labdata/labdata.html) | dataset introduction, schema, collector credits | Public Intel Lab sensor dataset provenance |
| `ohba2025nvme` / [14] | [An NVMe-Based Secure Computing Platform with FPGA-Based TFHE Accelerator](https://eaglys.co.jp/hubfs/An%20NVMe-based%20Secure%20Computing%20Platform%20with%20FPGA-based%20TFHE%20Accelerator.pdf) | pp. 69980–69981, abstract and introduction | NVMe conveys ciphertexts, keys, and homomorphic programs; encrypted evaluation scope. |
| `suzuki2023isc` / [21] | [Designing In-Storage Computing for Low Latency and High Throughput Homomorphic Encrypted Execution](https://waseda.elsevierpure.com/en/publications/designing-in-storage-computing-for-low-latency-and-high-throughpu/) | Official author-institution abstract | CSD-coupled model for homomorphic operations, evaluated by encrypted CNN simulations. No unsupported implementation detail. |

版本说明：F1 作者稿中两位共同第一作者的顺序与正式出版注册记录不同；BibTeX 按正式出版记录排列，未混入 arXiv 扩展版的九作者记录。MATCHA 的内容依据为六页 DAC 格式作者版；Strix 的内容依据为 arXiv v1，正式出版信息另行核对，只引用所核实的架构主张，未移植版本敏感的性能数值。二者没有被写成 FPGA 实现。

Suzuki 仅取得作者机构摘要，正文仅引用其研究对象和 CNN 仿真评估范围。原有 FHEmem 保持已读的 2023 arXiv v1；已确认其另有 2025 TETC 正式出版，但本轮没有取得正式全文，因此未混用作者列表或全文定位。

TPC-H 引用用于说明 dbgen 的来源，没有声称作者完成了规范要求的完整 TPC-H benchmark，也没有推定实验实际使用的 dbgen 版本。Intel Lab 条目的作者按官方网站所列采集者记录。SLM 使用固定 Revision 1.2 来源用于术语定义，未将其写成当前最新版。

## 全部 31 条的正文引用核验

| References 编号 | 引用键 | 正文章节 | PDF 正文页码 |
|---|---|---|---|
| [1] | `bodik2004intellab` | Experimental Evaluation | 8 |
| [2] | `chillotti2020tfhe` | Introduction; Background | 2 |
| [3] | `feryputri2025lwe` | Introduction; Related Work | 1, 11 |
| [4] | `gentry2009fhe` | Introduction; Background | 1, 2 |
| [5] | `gu2016biscuit` | Related Work | 11 |
| [6] | `gupta2024memfhe` | Introduction; Background; Related Work | 1, 2, 11 |
| [7] | `jiang2022matcha` | Related Work | 11 |
| [8] | `kim2022ark` | Introduction; Related Work | 1, 11 |
| [9] | `kim2022bts` | Introduction; Related Work | 1, 11 |
| [10] | `krieger2024aloha` | Introduction; Background; Related Work | 1, 2, 11 |
| [11] | `lee2023ckks` | Introduction; Related Work | 1, 11 |
| [12] | `mert2020bfv` | Introduction; Related Work | 1, 11 |
| [13] | `nvme2025slm` | System Overview | 3 |
| [14] | `ohba2025nvme` | Related Work | 12 |
| [15] | `putra2023strix` | Related Work | 11 |
| [16] | `regev2009lwe` | Background | 2 |
| [17] | `ruan2019insider` | Introduction; Background; Related Work | 2, 3, 11 |
| [18] | `samardzic2021f1` | Introduction; Related Work | 1, 11 |
| [19] | `samardzic2022craterlake` | Introduction; Related Work | 1, 11 |
| [20] | `snia2025csmodel` | Introduction; Background | 2 |
| [21] | `suzuki2023isc` | Related Work | 12 |
| [22] | `syafalni2026multicore` | Introduction; Related Work | 1, 11 |
| [23] | `tpch2022spec` | Experimental Evaluation | 8 |
| [24] | `vanbeirendonck2023fpt` | Introduction; Background; Related Work | 1, 2, 11 |
| [25] | `vanderhagen2022choco` | Introduction; Background; Related Work | 1, 2, 11 |
| [26] | `wong2024dongle` | Introduction; Background; Related Work | 2, 3, 11 |
| [27] | `woods2014ibex` | Introduction; Related Work | 2, 11 |
| [28] | `xu2026hera` | Introduction; Background; Related Work | 1, 2, 11 |
| [29] | `yune2025abcfhe` | Introduction; Background; Related Work | 1, 2, 11 |
| [30] | `zama2025hpu` | Introduction | 1 |
| [31] | `zhou2023fhemem` | Introduction; Background; Related Work | 1, 2, 11 |

结果：31 个 BibTeX 条目 = 31 个 BBL 条目 = 31 个正文引用键。未定义键、重复键、未引用条目、仅列出而未引用的 References 条目、`nocite` 均为 0。最终 PDF 的 31 个引用目标均存在，内部链接无失效目标。

## 编译、排版和保留性

- 完整执行 pdflatex → bibtex → pdflatex → pdflatex，并在必要的小排版修订后重编译；最后日志无未定义引用、交叉引用重编译提示或 Overfull。BibTeX 对 7 条 IEEE 会议文献给出出版社地址为空的非致命提示，未凭空补地址。
- 最终 PDF 共 13 页，References 从第 12 页开始。全篇逐页渲染检查，尾部与 Table 1/2 页面另用高分辨率检查，没有文字裁切或重叠。
- Figure 2 系统架构图仍位于第 3 页顶部。
- Table 1/2 均位于第 8 页且物理位置低于第 6 节标题。Table 1 保持 `[!htbp]`；Table 2 改为 `[!hbp]`，避免引文重排后重新浮到本页右栏顶部。
- 题目、摘要、前导元数据、第四至第五章及结论逐字节保持原样；第六章仅两项数据来源引用及 Table 2 浮动参数发生变化。主稿和文献库保留原始 LF 换行。
- 原图、ACM 双栏版式、字号和页边距均保留。原附录 Reproducibility Details 的待补充模板不属于本轮引用任务，未修改。

本轮核验范围为引用覆盖、增补来源、与增补来源对应的论述和排版，不等同于重新核验论文全部实验或投稿就绪检查。

## 记录与备份

- 修改前主稿、文献库、PDF 和 BBL：[before 备份目录](../../tmp/reference-expansion-20261007/before/)。
- 引用级来源、版本和支持范围：[integrated-source-evidence.json](../../tmp/reference-expansion-20261007/integrated-source-evidence.json)。
- 每条引用的句子、源码行、PDF 页码及保留性：[final-reference-audit.json](../../tmp/reference-expansion-20261007/final-reference-audit.json)。
- 候选文献和未采用的备选保存在同一临时目录，不计入最终 References。

最终 PDF SHA-256：`a5c4b1f02b1bad16c08fb7c581feb2a18c0bcda9842b5b4066195aee2185e10b`。


## 2026-10-08：全部参考文献的题名、作者与链接身份复核

本轮依据作者要求重新逐条核验当前 31 条 References，未沿用此前覆盖
检查作为身份结论。24 篇带 DOI 的文献逐项取得 Crossref 注册原始 JSON，
与出版商页面、正式 PDF、作者原始版本或官方机构记录交叉核对；其余
7 项按软件官方仓库、固定版本标准、预印本和数据集官方页面核对。
完整题名、完整作者名单及顺序、年份与版本、DOI、已有 URL 均已检查。
结果：未发现指向不同作品的题名／作者／链接错配，没有必须修改的
BibTeX 身份字段。保留文献库、主文稿与 PDF，不为了造成改动而替换
正确署名。

同时检查 BibTeX → BBL → PDF：31 个不同条目、31 个正文引用键、31 个
BBL 条目对应一致，无重复键、缺失键、未引用条目或 nocite。逐条确认
PDF 实际点击目标为该条的 DOI 或官方 URL，共检查 43 个外部链接注释；
31 条预期入口全部存在。带 DOI 的条目在 PDF 中使用正式 DOI 入口，
作者稿的补充 URL 不会替代它。

需保留的版本差异如下：

- F1：正式注册作者顺序为 Nikola Samardzic、Axel Feldmann 等；作者
  PDF 的两个共同第一作者排序与姓名简称有所不同。当前 BibTeX 符合
  正式注册记录，PDF 点击链接指向该正式 DOI，无需按作者稿改序。
- BTS / ARK：arXiv 摘要网页存在 John Kim、Minsoo Rhu 顺序差异，但
  实际作者 PDF 首页和正式注册记录与现 BibTeX 相同，保持正式顺序。
- MemFHE：正式 2024 期刊版与 Crossref 均署名 Tajana Šimunić；不因
  其他版本或个人主页而补成 Rosing。FHEmem 保持所引用的 2023 arXiv v1，
  不把后来的正式出版年份及作者信息混入该条。
- HERA：正式论文、官方会议节目与注册记录均用 Viktor Prasanna，
  不据 DBLP 作者身份规范化名称补 K。HERA 标题的下划线／换行是
  排版标记。DONGLE 的 Jing (Jane) Li 与官方落地页 Jing Li 为同一作者。
- Strix 的 Prasetiyo 是单名；F1 的 Christopher Peikert 与 CraterLake
  的 Chris Peikert 均按各自正式记录保留。Mert 论文的姓名重音正确。
- TFHE、Mert 等论文的在线先行年份与卷期年份不同，当前采用的正式
  卷期年份正确。TPC-H PDF 本次直取超时，但官方索引、PDF 搜索缓存
  的封面／修订历史和规范下载页核实了同一链接与 Revision 3.0.1，
  2022-04-28；未把抓取超时当成文献不存在或死链接。

下表列出全部条目的当前题名及引用入口。完整有序作者、逐字段检查、
版本差异与多个一级证据 URL 见同目录
[reference-metadata-audit-20261008.json](reference-metadata-audit-20261008.json)。

| 引用键 | 题名 | 核验入口 | 结果 |
|---|---|---|---|
| `chillotti2020tfhe` | TFHE: Fast Fully Homomorphic Encryption Over the Torus | [正式 DOI](https://doi.org/10.1007/s00145-019-09319-x) | 匹配，保留 |
| `zama2025hpu` | A systemVerilog implementation of the Homomorphic Processing Unit (HPU) targeting AMD Alveo V80 FPGA board | [官方来源](https://github.com/zama-ai/hpu_fpga) | 匹配，保留 |
| `xu2026hera` | HERA: A Bandwidth-efficient Accelerator for Fully Homomorphic Encryption on HBM-enabled FPGA | [正式 DOI](https://doi.org/10.1145/3748173.3779201) | 匹配，保留 |
| `zhou2023fhemem` | FHEmem: A Processing In-Memory Accelerator for Fully Homomorphic Encryption | [官方来源](https://arxiv.org/abs/2311.16293v1) | 匹配，保留 |
| `vanbeirendonck2023fpt` | FPT: A Fixed-Point Accelerator for Torus Fully Homomorphic Encryption | [正式 DOI](https://doi.org/10.1145/3576915.3623159) | 匹配，保留 |
| `ruan2019insider` | INSIDER: Designing In-Storage Computing System for Emerging High-Performance Drive | [官方来源](https://www.usenix.org/conference/atc19/presentation/ruan) | 匹配，保留 |
| `wong2024dongle` | DONGLE 2.0: Direct FPGA-Orchestrated NVMe Storage for HLS | [正式 DOI](https://doi.org/10.1145/3650038) | 匹配，保留 |
| `ohba2025nvme` | An NVMe-Based Secure Computing Platform With FPGA-Based TFHE Accelerator | [正式 DOI](https://doi.org/10.1109/ACCESS.2025.3561728) | 匹配，保留 |
| `mert2020bfv` | Design and Implementation of Encryption/Decryption Architectures for BFV Homomorphic Encryption Scheme | [正式 DOI](https://doi.org/10.1109/TVLSI.2019.2943127) | 匹配，保留 |
| `lee2023ckks` | Configurable Encryption and Decryption Architectures for CKKS-Based Homomorphic Encryption | [正式 DOI](https://doi.org/10.3390/s23177389) | 匹配，保留 |
| `krieger2024aloha` | Aloha-HE: A Low-Area Hardware Accelerator for Client-Side Operations in Homomorphic Encryption | [正式 DOI](https://doi.org/10.23919/DATE58400.2024.10546608) | 匹配，保留 |
| `yune2025abcfhe` | ABC-FHE: A Resource-Efficient Accelerator Enabling Bootstrappable Parameters for Client-Side Fully Homomorphic Encryption | [正式 DOI](https://doi.org/10.1109/DAC63849.2025.11132592) | 匹配，保留 |
| `gupta2024memfhe` | MemFHE: End-to-end Computing with Fully Homomorphic Encryption in Memory | [正式 DOI](https://doi.org/10.1145/3569955) | 匹配，保留 |
| `vanderhagen2022choco` | Client-Optimized Algorithms and Acceleration for Encrypted Compute Offloading | [正式 DOI](https://doi.org/10.1145/3503222.3507737) | 匹配，保留 |
| `suzuki2023isc` | Designing In-Storage Computing for Low Latency and High Throughput Homomorphic Encrypted Execution | [正式 DOI](https://doi.org/10.1109/ICBDA57405.2023.10104970) | 匹配，保留 |
| `feryputri2025lwe` | FPGA-Based Hardware Accelerator for LWE Encryption and Decryption with TFHE Scheme | [正式 DOI](https://doi.org/10.1109/SOCC66126.2025.11235379) | 匹配，保留 |
| `syafalni2026multicore` | Multicore Encryption-Decryption Hardware Accelerator for Torus-Based RLWE FHE | [正式 DOI](https://doi.org/10.1109/COOLCHIPS68842.2026.11556998) | 匹配，保留 |
| `kim2022bts` | BTS: An Accelerator for Bootstrappable Fully Homomorphic Encryption | [正式 DOI](https://doi.org/10.1145/3470496.3527415) | 匹配，保留 |
| `kim2022ark` | ARK: Fully Homomorphic Encryption Accelerator with Runtime Data Generation and Inter-Operation Key Reuse | [正式 DOI](https://doi.org/10.1109/MICRO56248.2022.00086) | 匹配，保留 |
| `gentry2009fhe` | Fully Homomorphic Encryption Using Ideal Lattices | [正式 DOI](https://doi.org/10.1145/1536414.1536440) | 匹配，保留 |
| `snia2025csmodel` | Computational Storage Architecture and Programming Model | [官方来源](https://www.snia.org/sites/default/files/technical-work/computational/release/SNIA-Computational-Storage-Architecture-and-Programming-Model-1.1.pdf) | 匹配，保留 |
| `samardzic2021f1` | F1: A Fast and Programmable Accelerator for Fully Homomorphic Encryption | [正式 DOI](https://doi.org/10.1145/3466752.3480070) | 匹配，保留 |
| `samardzic2022craterlake` | CraterLake: A Hardware Accelerator for Efficient Unbounded Computation on Encrypted Data | [正式 DOI](https://doi.org/10.1145/3470496.3527393) | 匹配，保留 |
| `jiang2022matcha` | MATCHA: A Fast and Energy-Efficient Accelerator for Fully Homomorphic Encryption over the Torus | [正式 DOI](https://doi.org/10.1145/3489517.3530435) | 匹配，保留 |
| `putra2023strix` | Strix: An End-to-End Streaming Architecture with Two-Level Ciphertext Batching for Fully Homomorphic Encryption with Programmable Bootstrapping | [正式 DOI](https://doi.org/10.1145/3613424.3614264) | 匹配，保留 |
| `woods2014ibex` | Ibex---An Intelligent Storage Engine with Support for Advanced SQL Off-loading | [正式 DOI](https://doi.org/10.14778/2732967.2732972) | 匹配，保留 |
| `gu2016biscuit` | Biscuit: A Framework for Near-Data Processing of Big Data Workloads | [正式 DOI](https://doi.org/10.1109/ISCA.2016.23) | 匹配，保留 |
| `regev2009lwe` | On Lattices, Learning with Errors, Random Linear Codes, and Cryptography | [正式 DOI](https://doi.org/10.1145/1568318.1568324) | 匹配，保留 |
| `nvme2025slm` | NVM Express Subsystem Local Memory Command Set Specification | [官方来源](https://nvmexpress.org/wp-content/uploads/NVM-Express-Subsystem-Local-Memory-Command-Set-Specification-Revision-1.2-2025.08.01-Ratified.pdf) | 匹配，保留 |
| `tpch2022spec` | TPC Benchmark H Standard Specification | [官方来源](https://www.tpc.org/TPC_Documents_Current_Versions/pdf/TPC-H_v3.0.1.pdf) | 匹配，保留 |
| `bodik2004intellab` | Intel Lab Data | [官方来源](https://db.csail.mit.edu/labdata/labdata.html) | 匹配，保留 |

本轮原始 Crossref 响应、四组证据、修改前备份和 PDF 链接提取记录位于
/tmp/fhestore-reference-audit-20261008。正文仍十页，含参考文献十一页；
由于书目和排版无须改动，未重新生成 PDF，未提交或 push。
