# FHEStore Background 写作说明

本次按作者确认的三部分结构重写 `CipherStore_FPGA27.tex` 的 Background and Motivation，并补充基础引用。题目、摘要、Introduction、贡献点及已有测量数值保持原文。

## 三节的职责与衔接

1. **FHE and TFHE Fundamentals**：定义加密、同态计算、解密及正确性关系；以通用 secret-key torus-LWE 为基础示例，说明编码、掩码、误差、密文和解密 phase。随后联系密码运算、密文大小与数据接口。
2. **Existing FHE Producer–Consumer Workflows**：介绍数据所有者、输入生产者、同态计算消费者和结果恢复的角色；说明客户端硬件与消费端的内存接口、参数与密文表示兼容，以及客户端辅助执行中的中间交互；最后限定到本文关注的持久化数据场景。
3. **Computational Storage and Application Integration**：依次解释计算存储概念、代表性应用、可编程性与应用接口，以及与 FHE 输入生产和结果写回的结合机会。没有展开设备或架构分类。

正文解释已有机制与接口；具体工作的优劣、实现差异和逐篇比较留在 Related Work。“用户友好”具体化为任务参数、输入输出位置、数据访问和完成语义，而不是直接宣称计算存储天然易用。

## 主要文献与支持范围

| 文献 | 已核实的原文位置 | 在 Background 中支持的内容 |
|---|---|---|
| [Gentry, STOC 2009](https://www.cs.cmu.edu/~odonnell/hits09/gentry-homomorphic-encryption.pdf) | §1.1，印刷页 169 | KeyGen、Encrypt、Decrypt、Evaluate 及同态正确性接口；正式元数据 DOI `10.1145/1536414.1536440` |
| [TFHE 原论文全文](https://eprint.iacr.org/2018/421.pdf) | §2 的 torus 定义、§2.2 torus distance、§2.3 Definition 2.5 及后续加解密说明 | torus-LWE 密文、phase、离散消息空间与舍入；对应已有 `chillotti2020tfhe` 的 JoC 2020 版本 |
| [Aloha-HE](https://eprint.iacr.org/2023/1736.pdf) | §I；§III.E；§IV.B | 客户端编码加密—服务端计算—客户端解密解码，以及软件控制、DMA 和本地存储接口 |
| [ABC-FHE 作者版本](https://arxiv.org/html/2506.08461v1) | §II-D、§III、§V-A | 客户端转换流程、片内缓冲与外部内存输入输出接口 |
| [MemFHE 正式全文](https://par.nsf.gov/servlets/purl/10534250) | §§4、7、9.5 | 客户端转换与服务端同态计算的不同角色 |
| HERA、FHEmem、FPT | 作者提供的 HERA/FHEmem PDF；[FPT 作者版本](https://eprint.iacr.org/2022/1635.pdf) | 消费端的密文、计算密钥和内存调度；不把其内部 load/save 解释为应用持久化生产路径 |
| [CHOCO 正式论文作者版](https://brandonlucia.com/pubs/choco-taco-asplos22.pdf) | §§2.1–2.2 | 客户端辅助执行中的中间解密、处理、重新加密与通信 |
| [SNIA Computational Storage Architecture and Programming Model v1.1](https://www.snia.org/sites/default/files/technical-work/computational/release/SNIA-Computational-Storage-Architecture-and-Programming-Model-1.1.pdf) | 2025-05-31 发布；§1，p12；§3.1.3，p14 | 计算与存储耦合，以及主机卸载或减少数据移动的定义；采用固定版本而非动态网页年份 |
| [INSIDER](https://www.usenix.org/system/files/atc19-ruan_0.pdf) | §3.3，印刷页 383–384；§3.4，p385；§5.2/Table 3，p387 | 虚拟文件、C++ 输入/输出/参数 FIFO，以及过滤、统计、特征选择和解压应用 |
| [DONGLE 2.0](https://doi.org/10.1145/3650038) | §4.1 Programming Model、§4.2 Architectural Overview；出版社缓存全文 | 统一 HLS 存储与内存接口、处理与传输协调；不推导出所有模式均绕过主机 |

新增 BibTeX 条目为 `gentry2009fhe` 和 `snia2025csmodel`，其他引用沿用现有文献库。

## 公式与实现边界

- 抽象正确性关系针对精确语义的 FHE，明确有效参数、支持的计算及 overwhelming probability；不把近似 FHE 的误差语义写成精确等式。
- `k_enc`、`k_dec`、`k_eval` 表示方案所需的密钥材料，不预设所有 FHE 使用公钥加密，也不要求三者都是独立密钥。
- torus-LWE 对称加密公式是通用背景示例，没有将其声明为已确认的 FHEStore 加密模式。实际原型的密钥分布、维数、精度、噪声、安全参数和编码方式仍由实现章节说明。
- 最近消息按 torus distance 恢复；成功条件针对 phase 是否仍在编码消息的判决区域。
- unsigned 8-bit 与 4 KiB 是已有原型评估条件，没有引入通用公式，也没有由此推断每字节对应多少密文。
- producer–consumer 是本文解释已有客户端/服务端流程的视角；客户端中间交互作为已有执行变体介绍。
- 计算存储提供放置与接口能力；没有据此宣称消除全部主机缓冲或必需密文传输，也没有增加未测量的 CPU、内存、流量或输出路径收益。

## 修改核验

写作后执行数学与论证复核，修正了正确性概率条件、phase 判决区域和未确认的独立结果格式化阶段。编译入口、ACM 双栏匿名设置、首页会议脚注及 ACM Reference Format 保留。为适配新增正文后的分页，已有 `\balance` 在参考文献开始处按当前栏位置启用；若该处在第二栏，则延迟至下一页第一栏，避免改变含浮动表格的前一页。后续章节正文未改动。

原稿备份、编译输出、页面渲染及确定性核验结果保存在 `tmp/pdfs/background-v1/`。这是 Background 的修订记录，不是全篇论文完成或投稿就绪判断。

最终 PDF 共 6 页，Background 位于第 2–3 页。完成 BibTeX 及多轮 LaTeX 编译后，未出现未定义引用、Overfull 或 balance 警告；有两条 Underfull vbox 提示。已逐页检查最终六页渲染，公式、引用、页眉页脚和双栏内容没有截断或重叠。
