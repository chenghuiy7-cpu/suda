# FHEDock 引言第一版写作说明

本次修改范围：`CipherStore_FPGA27.tex` 的 CCS Concepts、Keywords、Introduction 及其三个贡献点；新增引用写入 `CipherStore_refs.bib`。题目、摘要和后续正文保持原稿。修改前的三个主文件保存在 `backup_before_introduction_20261002/`。

## 叙事和贡献组织

引言从 FHE 的加速需求切入，借助 HERA、FHEmem、FPT 说明算子与数据组织协同的重要性，随后转向持久化输入准备和结果落盘所需的处理。INSIDER、DONGLE 2.0 和 NVMe–TFHE 平台用于建立计算存储及接口设计的已有基础。FHEDock 的主张聚焦存储侧加解密与远端 HPU 之间的输入、返回路径。

贡献顺序遵循作者要求：

1. **HLS 加密和解密硬件算子。** 加密集成编码并产生 HPU 可消费的密文，解密恢复返回结果中的应用值。
2. **流式算子组织、控制通路和应用使用路径。** 连接存储访问、轻量预处理、加解密、远端 HPU 交换与结果回写。
3. **实现原型和评估。** 保留 unsigned 8-bit、4 KiB 条件下的 5.33× 加密与 3.26× 端到端输入准备加速，以及远端 HPU 演示。

第一项的具体微架构创新还需补充实际 HLS 设计；第二项的接口设计还需补充真实控制方式和用户调用过程。当前材料没有实现源码、算子图、接口签名或 HLS 调度结果，因此本版没有写入特定流水线深度、并行核数、FIFO/AXI 结构、自动调度、直接设备通信或可配置 API 的实现主张。返回路径描述加解密和回写功能；数值性能主张落在已有输入路径测量上。

## CCS Concepts 和 Keywords

- Hardware → Reconfigurable logic and FPGAs（500）
- Information systems → Storage architectures（300）
- Security and privacy → Cryptography（300）

Keywords：Fully homomorphic encryption; TFHE; FPGA; high-level synthesis; computational storage; streaming dataflow。

这组分类覆盖 FPGA 硬件、计算存储和密码计算三个主题。采用 Cryptography 可避免在尚未明确 TFHE 加密模式时把工作限定为公钥加密。当前提供的是可在 acmart 中排版的 `\ccsdesc`；投稿元数据中的 CCSXML 仍应由 [ACM CCS 工具](https://dl.acm.org/ccs/ccs.cfm) 生成。此次访问 ACM 分类工具返回 403，未把推测的 concept_id 写入稿件。

## 参考论文与引用依据

检索与核对日期：2026-10-02。以下定位对应本次引言中的概括；未将不同方案、参数和测量范围的加速比直接比较。

| BibTeX key | 来源与版本 | 读取定位 | 在引言中的用途 |
|---|---|---|---|
| `xu2026hera` | [作者提供的 HERA PDF](<Xu 等 - 2026 - HERA A Bandwidth-efficient Accelerator for Fully H omomorphic E nc r yption on HBM-enabled FPG A.pdf>)；FPGA '26，DOI 10.1145/3748173.3779201；[会议官方程序](https://isfpga.org/past/fpga2026/program/) | pp. 265–266，Abstract、Introduction 和贡献列表 | CKKS 算子分解、HBM 数据布局与内存优化协同；学习从问题到机制再到评估的组织方式 |
| `zhou2023fhemem` | [作者提供的 FHEmem PDF](<Zhou 等 - 2023 - FHEmem A Processing In-Memory Accelerator for Fully Homomorphic Encryption.pdf>)；[arXiv:2311.16293v1](https://arxiv.org/html/2311.16293v1)，2023-11-27 | pp. 1–2，Introduction 和贡献列表；§IV-F | near-mat 计算、应用映射和 load-save pipeline；学习硬件、映射、评估的三个贡献层次 |
| `vanbeirendonck2023fpt` | [FPT 作者版本](https://eprint.iacr.org/2022/1635.pdf)；2023-10-18 修订；[作者项目及 CCS '23 元数据](https://github.com/KULeuven-COSIC/fpt-demo) | pp. 1–2，Abstract、Introduction；§3 | TFHE bootstrapping 的流式算子组合和吞吐匹配 |
| `ruan2019insider` | [USENIX ATC '19 官方论文及元数据](https://www.usenix.org/conference/atc19/presentation/ruan) | Abstract；§3.3 Host Programming Interface | FPGA 存储计算与文件式用户抽象；说明使用路径也是系统设计的一部分 |
| `wong2024dongle` | [DONGLE 2.0 出版方论文](https://doi.org/10.1145/3650038)，ACM TRETS 17(3)，Article 45，2024 | Abstract；§4.1 Programming Model；§4.2 Architectural Overview | HLS 计算与存储访问的统一接口。引用读取到的期刊扩展版，不混用 FPGA '23 原版的元数据 |
| `ohba2025nvme` | [NVMe–TFHE 作者论文](https://eprint.iacr.org/2024/744.pdf)；[IACR 元数据](https://eprint.iacr.org/2024/744)，最新修订 2025-04-16；[Crossref 注册元数据](https://api.crossref.org/works/10.1109/ACCESS.2025.3561728)，IEEE Access 13，69980–69997，DOI 10.1109/ACCESS.2025.3561728 | Abstract；§III-A Basic Architecture，pp. 2–3 | 已有 NVMe、TFHE 评估引擎和主机中间件集成；据此明确 FHEDock 的输入准备、返回结果处理和远端 HPU 定位 |
| `chillotti2020tfhe` | 原稿已有 TFHE 论文，Journal of Cryptology 33，34–91，DOI 10.1007/s00145-019-09319-x | 用于标识 TFHE 方案；本次没有新增算法或性能概括 | 保留原引用，并使引用明确指向 TFHE 方案名称 |

## 后续补充最有价值的材料

- 算子层：TFHE 加密/解密的具体形式、关键循环或算术结构、HLS 优化、资源与时序结果。
- 系统层：真实算子连接图、控制职责、批次/缓冲管理、用户调用示例、数据与结果的完成条件。
- 评估层：对应两个加速比的绝对时间与基线配置；解密和结果回写的功能验证及独立测量。

这些材料可以使第一、第二个贡献从已知的功能组织进一步落实到可复现的设计机制。

## 本版检查

已完成 BibTeX 和两轮最终 LaTeX 编译，并逐页查看最终 5 页 PDF。7 个引用键均能解析；BibTeX 无警告；最终 LaTeX 日志没有未定义引用或 Overfull 报告。原稿的 `printacmref=false` 提示仍在，未在本次章节写作中调整投稿元数据设置。自动比对确认题目、摘要和 Background 之后的正文未改动。

审阅范围是本次三个组成部分及引用关系。后续章节原有的 Writing guide 和实验细节待补充项仍按原稿保留。

## 2026-10-03：FHEStore 引言目标与贡献对应修订

本次仅修订 Introduction 及前两个贡献项的表述。当前作者确认的题目、摘要、两个加速比及其测量条件保持不变；贡献引导句、局部列表间距与缩进也保持不变。后续正文与文献库未改动。

### 术语与目标

`homomorphic evaluation` 是已有术语，指对密文执行函数或电路计算，与 encryption、decryption 区分。[TFHE 作者维护的原始项目页面](https://tfhe.github.io/tfhe/) Description 和用户操作列表明确使用该术语；[Microsoft Research 的 CHET 原始出版记录](https://www.microsoft.com/en-us/research/publication/chet-compiler-and-runtime-for-homomorphic-evaluation-of-tensor-programs/)也在题目中使用它。为与摘要一致，当前引言采用 `homomorphic computation`。

新增明确目标：加速 TFHE 加密与解密，并减少持久化存储到 FHE 加速器路径中的主机 CPU 工作、主机内存缓冲及不必要的中间数据搬移。前者对应 HLS 密码算子；后者对应存储侧流式通路、控制和应用使用路径。第三项用原型与已测量的输入加速结果提供证据。

CPU、缓冲和搬移收益是设计目标；正文只陈述已经描述的 FPGA 密码操作卸载与存储侧组织机制，没有新增资源占用或字节数减少的测量结论。消费者连接仍可能由主机软件中转，未声称直接设备通信或消除主机缓冲。加解密目标也不等于已获得解密或完整输出路径的性能结果。

### 分组引用与依据

| 文献类别与引用键 | 对应主张 | 原始来源、版本与定位 |
|---|---|---|
| 同态计算加速，3 篇：`xu2026hera,zhou2023fhemem,vanbeirendonck2023fpt` | 算术、内存访问与密文数据移动共同构成消费端加速设计。 | 作者提供 HERA PDF，pp. 265–266；作者提供 FHEmem 2023 arXiv v1，pp. 1–2；[FPT 2023-10-18 作者稿](https://eprint.iacr.org/2022/1635.pdf)，pp. 1–2，Abstract、§1。消费端内部流量不作为本文主机存储路径的瓶颈测量。 |
| 客户端加解密硬件，3 篇：`mert2020bfv,lee2023ckks,krieger2024aloha` | 明密文转换有计算和内存需求，已有专用数据通路及编码/解码支持。 | BFV 沿用上述机构原始摘要核验；[CKKS 原始全文](https://pmc.ncbi.nlm.nih.gov/articles/PMC10490559/)，§1、§4.2、Fig. 4、§5；[Aloha-HE 作者机构全文](https://tugraz.elsevierpure.com/ws/portalfiles/portal/71961571/AlohaHe_ClientSideFHE.pdf)，p. 1/§I、p. 5/§III-E/Fig. 5、p. 6/§IV-B。编码/解码支持是该组包含的能力，未声称每篇均实现全部阶段。 |
| 密码处理与资源组织，3 篇：`vanderhagen2022choco,gupta2024memfhe,yune2025abcfhe` | 加速与数据表示、内存访问或客户端通信共同设计。 | [CHOCO 正式作者副本](https://brandonlucia.com/pubs/choco-taco-asplos22.pdf)，pp. 683–684/§1、p. 686/§2.3、pp. 688–689/§4；[MemFHE 正式版 NSF 存档](https://par.nsf.gov/servlets/purl/10534250)，pp. 28:1–28:3；[ABC-FHE 作者 v1](https://arxiv.org/html/2506.08461v1)，§I、§III、§IV-B、§V-A。CHOCO 为 client-aided BFV/CKKS，ABC-FHE 内存流量主要是密码核访存；这些结果不移植为本文 TFHE 的量化瓶颈。 |
| 计算存储与 FPGA 存储耦合，4 篇：`ruan2019insider,wong2024dongle,suzuki2023isc,ohba2025nvme` | 研究数据搬移成本，以及存储连接加速处理的接口。 | [INSIDER 官方全文](https://www.usenix.org/system/files/atc19-ruan_0.pdf)，§1、§2.1、§3.5；DONGLE 2.0 正式期刊版 DOI 10.1145/3650038，§3、§4.1–4.3、§4.6（ACM 搜索返回正文，直接页面访问 403）；Suzuki 沿用[作者大学原始摘要](https://waseda.elsevierpure.com/en/publications/designing-in-storage-computing-for-low-latency-and-high-throughpu/)，全文未取得；NVMe–TFHE 沿用 [2025-04-16 作者稿](https://eprint.iacr.org/2024/744.pdf)，§I–III、§V。该组不能共同证明存储侧输入加密、主机绕过或本文实际传输量下降。 |

以上原始来源在 2026-10-03 重新检索或结合既有全文副本核对。Introduction 按类别概述，没有逐篇点名介绍；TFHE 方案定义引用单独保留。5.33× 仍对应 uint8、4 KiB 批次的 FPGA 加密相对 CPU 加密；3.26× 仍对应同一条件下的端到端输入生产相对主机基线。HPU 仅在第三项贡献中作为可消费生成密文的原型验证场景。

本次完成 BibTeX 与 LaTeX 编译，最终 LaTeX 日志无未定义引用、Overfull 或交叉引用重编译要求；PDF 共 5 页，已逐页渲染并检查。自动比对确认题目、摘要、头部元数据、Background 起的正文、第三项贡献及文献库保持不变。核验结果与渲染图保存在 `../tmp/pdfs/introduction-goals/`。本次自审限于引言目标、贡献对应、引用和排版；后续章节原有待补充项保留。

## 2026-10-03：三类 FHE 文献与持久化数据路径的引言重构

本节记录本次最新版。修改范围为 Introduction 的六个正文段落，以及文献库新增的四条记录。题目、摘要、CCS、Keywords、ACM 元数据、贡献列表全文及其局部排版、Background 起的后续正文保持上一版。新增文献后，模板在第二栏启动末页均衡而产生 `balance` 警告；在参考文献前补入已有的 `\balance` 命令，使末页从第一栏启动均衡，未更改字号、栏宽或页边距。修改前的主稿、文献库与 PDF 保存在 `../tmp/pdfs/introduction-v4/before.*`。

### 论证链条

1. 小背景直接定义应用的输入与返回路径：FHE 加速器消费密文；持久化应用数据需转换为输入密文，返回密文需恢复并回写。
2. **加解密硬件，4 篇。** 肯定专用算术和并行通路对明密文转换的加速；由核执行成本转到算子与准备、交付、恢复阶段的连接及放置问题。
3. **内存与数据搬移感知的 FHE 计算，5 篇。** 肯定对密文、评估密钥工作集以及内存访问、复用和流式调度的优化；明确其同态计算边界与应用输入生产、结果恢复路径的区别。
4. **客户端转换与资源协同，4 篇。** 肯定编码、内存组织、流式处理、缓冲或通信的协同；把这一协调问题扩展到持久化访问、应用预处理和最终结果回写。
5. 用主机基线路径说明两个设计对象为何相互关联：密码算子的执行效率，以及其周围阶段的放置与协调。CPU 工作、内存缓冲和接口数据量均有对应来源，随后给出明确目标。
6. 用 HLS TFHE 算子、存储侧输入和返回流式通路、控制与应用使用路径回应目标，再接原有三个贡献项。

引言只按研究类别概述；具体论文名称、机制和逐项差异留给 Related Work。当前分类用于组织论证，不要求工作之间严格互斥。计算存储及存储耦合加速文献从引言综述移出，原有 BibTeX 记录保留；FHEStore 方案段仍明确介绍 FPGA 计算存储的放置。

### 分组与新增依据

| 类别 | 引用键 | 概括依据与边界 |
|---|---|---|
| 加解密硬件 | `mert2020bfv,lee2023ckks,feryputri2025lwe,syafalni2026multicore` | 专用明密文转换硬件。BFV 与 CKKS 沿用上文的原始来源核验；Feryputri 全文与 Syafalni 机构摘要及官方程序见下表。不称这些工作为“没有接口的裸核”，也不由仅摘要来源推断缺失持久化功能。 |
| 内存与数据搬移感知的同态计算 | `xu2026hera,zhou2023fhemem,vanbeirendonck2023fpt,kim2022bts,kim2022ark` | 算术与密文/评估密钥工作集、复用、访存及流式调度共同设计。HERA、FHEmem、FPT 沿用提供的原始版本与定位；BTS、ARK 新增定位见下表。本文生产的是应用输入密文，不把评估密钥或整个计算工作集称为从应用数据生产。 |
| 客户端转换与资源协同 | `krieger2024aloha,yune2025abcfhe,vanderhagen2022choco,gupta2024memfhe` | Aloha-HE p. 5/§III-E、p. 6/§IV-B 有主机控制、DMA 与传输计时；ABC-FHE §III、§V-A 有外存 scratchpad、主机消息和服务器密文；CHOCO §IV.3、§V.6 有主机内存输出及通信评估；MemFHE 正式版 §§4、7、9.5 有客户端与服务器设计。概括限定于其客户端部分，持久化路径是本文增加的应用处理边界。 |

| 新增引用 | 已核实元数据 | 原始来源、版本及内容定位 |
|---|---|---|
| `feryputri2025lwe` | Najmi Azzahra Feryputri 等六位作者；IEEE SOCC 2025；pp. 1–6；DOI `10.1109/SOCC66126.2025.11235379` | [作者提供的六页 PDF](<Feryputri 等 - 2025 - FPGA-Based Hardware Accelerator for LWE Encryption and Decryption with TFHE Scheme.pdf>)，p. 1 标题、作者、DOI 与摘要；§III-B 加解密硬件。p. 2/§III-A、Algorithm 1 已含 Python 预处理/控制、缓冲分配及 DMA，不能写成未考虑任何数据处理。名字按 PDF 的 `Najmi Azzahra` 记载。 |
| `syafalni2026multicore` | Infall Syafalni 等七位作者；IEEE COOL CHIPS 2026；pp. 1–3；DOI `10.1109/COOLCHIPS68842.2026.11556998` | [ITB 原始机构摘要](https://scholar.itb.ac.id/paper/multicore-encryption-decryption-hardware-accelerat_2-s2.0-105043405047)支持多核 FPGA torus/RLWE 加解密硬件；[会议官方程序](https://www.coolchips.org/archive/2026/2026/index.html@p=386.html)，2026-04-17 Session XIII 核实标题与七位作者。[IEEE 提交的 Crossref 元数据](https://api.crossref.org/works/10.1109/COOLCHIPS68842.2026.11556998)的 `message.page` 为 `1-3`，作者名与官方程序一致；响应副本保存在 `../tmp/pdfs/introduction-v4/syafalni2026-crossref.json`。未取得全文，不用于具体接口或持久化范围的否定判断。 |
| `kim2022bts` | ISCA '22；pp. 711–725；DOI `10.1145/3470496.3527415`；作者顺序按正式 Crossref 记录 | [arXiv:2112.15479v2](https://arxiv.org/abs/2112.15479v2)，2022-04-29；PDF p. 1 摘要、p. 4/§3.1、p. 5/§3.3、p. 7/§4.3，支持 bootstrapping 工作集、评估密钥外存带宽与数据复用；正式元数据副本为 `../tmp/producer-consumer-revision/bts-crossref.json`。 |
| `kim2022ark` | MICRO '22；pp. 1237–1254；DOI `10.1109/MICRO56248.2022.00086`；作者顺序按正式 Crossref 记录 | [arXiv:2205.00922v3](https://arxiv.org/abs/2205.00922v3)，2022-10-30；PDF p. 1、p. 5/§III-A、p. 6/§III-B 与 §IV-A，支持密文膨胀、密钥工作集、访存压力与密钥复用。其 runtime data generation 指同态计算内部数据，不是生产应用输入密文；元数据副本为 `../tmp/producer-consumer-revision/ark-crossref.json`。 |

### 主张与核验范围

减少主机 CPU 工作、缓冲及中间数据搬移是设计目标；算子卸载和存储侧通路是相应机制。当前没有将输入时间加速比解释为 CPU 利用率、峰值内存或搬移字节数的测量结果。5.33× 与 3.26× 及 unsigned 8-bit、4 KiB 条件按作者摘要保留，分别对应加密引擎与端到端输入生产；HPU 仅作密文消费者兼容性演示。没有新增解密、结果回写或完整读—计算—写路径的性能数值。

编译、源码保持性比对、引用键检查与最终渲染记录保存在 `../tmp/pdfs/introduction-v4/`；机器核验见 `verification.json`。自审针对本次引言及引用，不扩展为尚含 Writing guide 后续章节的投稿就绪检查。

最终完成 `pdflatex → bibtex → pdflatex → pdflatex`，四步均成功。三组引用数量为 4、5、4，另保留一条 TFHE 方案引用；18 条文献库记录中，本稿共引用 14 条，未定义引用与重复键均为零。最终 PDF 为 5 页，已逐页查看最新渲染图，正文、贡献列表、首页参考格式与会议脚注均正常，末页双栏已均衡，无裁切、重叠或 Overfull。日志剩余非致命提示为模板的 Underfull vbox，以及五条 IEEE 会议文献未填出版社地址的 BibTeX 警告；没有把会议地点冒充出版社地址，也没有缺失页码警告。

## 2026-10-03：贡献列表压缩

按作者反馈，本次仅缩短三个贡献项的文字和短标题。前两项各一句，第三项两句；按同一英文词数口径（含短标题），三项由 50、43、67 词缩至 24、30、55 词，总计减少约 32%。编码与加密集成、加解密主机卸载、双向流式组织、控制和应用使用、FPGA 计算存储原型均保留。第三项保留 unsigned 8-bit、4 KiB 明文批次、5.33× 对应主机 CPU 加密、3.26× 对应主机输入生产，以及生成密文在远端 HPU 上执行计算的演示。

自动比对确认列表以外的主稿文字及局部间距设置完全不变。已重新编译并逐页检查最新五页 PDF；列表无裁切或重叠，末页保持均衡。日志有非致命的 1.17801 pt 末页均衡 vbox 提示，经渲染检查未出现文字裁切或页脚重叠。备份、编译记录、渲染和核验文件位于 `../tmp/pdfs/contributions-concise/`。
