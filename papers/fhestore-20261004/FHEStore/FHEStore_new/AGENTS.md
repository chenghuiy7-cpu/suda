# FHEStore 工作区

## 论文文件与依据

- 主文稿：`acmart-primary/FHEStore_FPGA27.tex`；编译产物为同目录下的 `FHEStore_FPGA27.pdf`。
- 文献库：`acmart-primary/FHEStore_refs.bib`。
- 题目和摘要以作者在会话中最新确认的版本为准；题目采用 2026-10-08 确认的 `FHEStore: An FPGA-based Computational Storage Framework for Input Preparation in Fully Homomorphic Encryption`，短标题为 `FHEStore: A Framework for FHE Input Preparation`。摘要采用本日确认的三段版本：producer–consumer 分工与持久化数据准备路径；HLS 集成编码加密算子、考虑密文膨胀的算子流组合方法（值数量与密文表示确定缓冲容量，流控协调算子执行）；1 KiB 明文输入下算子 5.00×、input preparation 2.45×，以及较低的 cumulative host CPU time 和 peak host memory。摘要不列动机占比或远端 HPU 演示句。原始参考文档为 `acmart-primary/FHEDock_题目&摘要.docx`，老师批注见 `acmart-primary/FHEDock_题目&摘要-comments.docx`。保持作者提供的题目、摘要和数值；修改这些内容需要对应的写作任务。
- 2026-10-08 作者确认整体阶段统一采用 FHE input preparation（后文简称 input preparation），framework 表示整体组织，producer–consumer 保留为动机角色，算子内部的实际生成动作仍使用 ciphertext generation 或 incremental ciphertext output。2026-10-08 已按作者要求完成正文迁移，Introduction、Background、System Overview、第五章、实验、Related Work 和 Conclusion 与上述题目摘要对齐。
- 系统名称使用 FHEStore，主文稿和文献库使用上述 FHEStore 文件名。
- 后续章节直接在主文稿中写作和修改，并编译整篇论文；不再创建独立章节摘录。Introduction 和 Background 已有作者完成版本，后续章节的叙事、术语与符号应与它们对齐。
- 第二章标题固定为 Background，不添加 Motivation。
- 作者于 2026-10-08 确认标题调整：第四章 HLS Design of TFHE Encoding and Encryption；5.1 Operator Stream Composition；6.2 Encryption Operator Performance；6.3 Input Preparation Performance。6.5 保持 Application-Directed Input Preparation，因测量涵盖数据访问、可选预处理、编码加密和 Host 内存就绪的完整准备阶段；不以 Ciphertext Generation 替代这一阶段名称。
- 首页 CCS Concepts 和 Keywords 沿用作者提供的 HERA FPGA ’26 论文样式：栏目名首字母大写并加粗，CCS 条目加粗，关键词正文使用普通字体和首字母大写。当前 CCS 为 Hardware accelerators、Storage architectures、Cryptography。作者于 2026-10-06 要求关键词必须保留 High-Level Synthesis，不使用 Ciphertext Production 作为关键词；当前关键词为 Fully Homomorphic Encryption、TFHE、FPGA、High-Level Synthesis、Computational Storage、Streaming Dataflow。
- 第三章保留系统组织、应用任务模型、算子图与设备执行三个小节。远端服务用于定位本工作的 CSD 与 Host 范围，概述中仅说明它是带有同态计算加速器的隐私计算外包原型，不展开 HPU 型号、内部实现或远端协议。
- 作者当前将工作聚焦于 FHE input preparation，即持久化数据到 Host 内存中的密文就绪；正文的工程贡献和执行路径不再包含 FPGA 解密与结果回写；Background 中的 FHE 正确性和基本解密定义可以保留。
- 系统架构图的数据流只保留 persistent data → Operator Pool → Host → external consumer 的准备与交付方向。Device Runtime 的配置与完成控制箭头连接 Operator Pool，Storage-side data path 为分组边界。Host 通信功能使用 Ciphertext Transfer 名称，表示经 PCIe 获取密文并经网络交付外部消费者。
- 2026-10-06 作者要求系统架构图放在第三页顶部；新增引言动机图后，系统架构图顺延为 Figure 2。通过提前声明双栏浮动图实现，编译后核对实际页码。检查普通正文页的左右栏末行对齐，以及同级标题前后与正文的实际可见间距，不能只检查标题宏定义或标题后间距。沿用 ACM 的标题定义和双栏排版，不用逐标题 vspace 或字号、页边距修改来修补。当前普通正文段落以零自然间距、有限伸展分担齐底余量，避免余量集中在标题前。
- 引言动机图比较两组相同的 Host CPU input preparation，仅将后端 homomorphic computation 从 CPU 软件改为 Zama 的开源 FPGA-based HPU 加速。引言首次说明 HPU 来源并引用官方仓库；板卡型号、版本和配置留在实验章节。Input preparation 从 SSD 读取开始，经过 encoding、encryption，到密文在 Host 内存中就绪；网络传输不计入，本文不将网络优化作为贡献。准备阶段占比从 0.11%–0.75% 上升到 17.7%–36.6%，表述为相对重要性上升，不将 CSD preparation 误作该图中的测量路径。
- 动机图保留作者上传的原始 PNG 作为数值与误差线依据。作者确认柱形采用平均值、误差线表示观测到的最小值和最大值；正文沿用这一统计定义，不推测重复次数或改成 SD、SEM、置信区间。2026-10-06 作者进一步要求动机图放在第一页单栏，允许按栏宽调整布局，并要求图中文字加粗。作者随后明确要求两个面板并列一行、压缩图注，测量口径放入正文。作者最新要求改为较细的横向堆叠条形，两组均去掉 Right shift。当前首页单栏一行两面板，共用操作名称和图例，Latency share (%) 为水平数值轴，保留 8–9pt 加粗文字；仅展示 AND、Add、Mul，正文保留完整操作名称，条形厚度为 8pt。作者进一步要求收窄左侧标签区、缩小三条间距、同时数字化显示两阶段份额。当前条形中心间距 23pt，两阶段数值在每条下方同一行分别以蓝色与深橙色显示；单位由数值轴提供。均值柱形和文字为矢量，未提供数值的 min/max 误差线从原始图裁剪保留，不据图推断边界值。
- 引言中的 record selection 是可选加密前 preprocessing 的一个例子，仅用一句解释排除无关数据可减少后续密码计算与密文搬移；不将筛选视作所有任务的默认阶段，也不把它加入动机图已测量的准备步骤。Introduction、摘要和题目均采用作者最新确认的版本。
- 第三章先定义通用 input-preparation task：有序应用值序列对应有序 ciphertext bundle 序列，再展开 selection/projection case。$Q$ 为可选 preprocessing，$O$ 为 Host 内存中的输出位置；source-record mapping 只用于 selective task。Device completion 表示 output SLM 中密文就绪，application completion 表示 Host 内存中密文就绪，随后才可向外部消费者转发。第五章先解释共同的流执行、输出容量与完成机制，再展开 discovery pass 等选择性任务机制。
- 第五章为 Storage-Side FHE Input Preparation，包含 Operator Stream Composition、Execution and Output Management 两个小节。叙事从 SSD 加载开始，展开 input SLM、read DMA、同一个共享算子流交换机、算子链、write DMA、output SLM 与 Host 的关系。Selection/Projection 是算子组合示例。
- 第五章按作者算子互连图（当前 Figure 4）的方法层记法解释 Slot ID、Port ID、`{3,1}` 算子目的地和 `{F,2}` 内存输出目的地。Scheduler 建立下一跳绑定，正文着重于流的组织与执行；选择性任务承接第三章的 preprocessing 和 `Q`。
- 当前选择性任务先在 FPGA 上生成 selection manifest，Host 获取选中数量和索引后，复用同一 input SLM 执行 encryption pass。非空选择准确表述为一次 SSD 加载、两次 SLM 扫描；空选择跳过 encryption pass。encryption pass 内算子之间直接流交接。
- 2026-10-08 全文按 FHE input preparation 主线对齐：input preparation 统一指从持久化数据访问到 Host 内存中密文就绪的整体阶段、任务、框架路径和性能指标；producer–consumer 仅描述工作流角色。ciphertext generation 和 incremental ciphertext output 用于算子内部实际生成动作，不另引入 ciphertext preparation。方法贡献固定为 operator stream composition method，考虑密文膨胀：连接可选预处理与加密，依据值数量与密文表示确定缓冲容量，以流控协调组合算子。正文保持 encryption operator、stream switch、ciphertext transfer、operator context、execution program、SLM range、selection manifest、discovery pass、encryption pass、terminal beat、valid ciphertext length 等术语的固定指代；不同抽象对象保留明确的不同名称，不为同一对象随意换词。
- 本工作中特定含义的术语在首次正文使用时给出简短功能解释，后文再展开字段和执行机制。FHE、TFHE、LWE 等领域缩写首次展开；CPU、SSD、FPGA、FIFO、LUT、FF、DSP 以及目标读者熟悉的 PCIe、DMA、AXI4-Stream、BRAM 不机械展开。SLM 采用 NVM Express 标准名称 Subsystem Local Memory，取代本地旧 API 文档的 Storage Layer Memory。TKEEP/TDEST 为信号名，说明作用而不另造全称。保留作者确认的图片与短图注，不把引言改成术语表。
- 具体原型参数、固定宽度和配置数值集中在 Experimental Evaluation；设计章节使用符号参数，不反复称系统为 prototype 或写成产品报告。摘要按作者确认保留 1 KiB 明文输入量和对应性能结果；算子互连图的路由值是作者示意图中的例子。
- 作者自行补充实验参数说明；删除 Encryption and stream configuration for evaluation 参数表及关联说明，不再提前填入这一配置表。
- 第三至第五章依据代码展开机制和设计理由，以研究论文方式组织。作者于 2026-10-08 将篇幅目标更新为参考文献之前的正文在第 10 页内结束，取代此前实验前约七页的目标。优先压缩跨章节重复和泛化总结，保留设计机制与实验依据；保持 ACM 双栏版式、字号、页边距及作者确认的图，使用常规浮动排版，不以强制分页或缩小字体凑页。

- 2026-10-08 合并师弟 GitHub 提交 d14429fde 的实验修改。作者明确确认新增 4096-byte、5.33× 结果和所有性能实验每配置 10 次有效测量均有新测量依据；本次采用该新口径，不再用旧数据快照中的 4096 median 备注或 3/2 次性能记录覆写新稿。原始历史快照保留，不补造样本。6.6 的 104 次正确性验证为独立实验，保持原表和 3/2 次计数。

## 工作区 Skills

下列技能已安装在项目的 `skills/` 目录。开展相关任务时，先读取对应 `SKILL.md`，再按其路由读取需要的支持文件；不必对每次小修改执行全部流程。

| Skill | 说明文件 | 用途 |
|---|---|---|
| academic-writing | `skills/academic-writing/SKILL.md` | 各章节起草、重构、翻译和润色 |
| paper-writing | `skills/paper-writing/SKILL.md` | 根据论点和证据修改正文 |
| academic-writer | `skills/academic-writer/SKILL.md` | 压缩冗余、改善自然学术表达 |
| anti-defensive-writing | `skills/anti-defensive-writing/SKILL.md` | 围绕最强贡献组织叙事，消除重复防御性表达 |
| paper-review | `skills/paper-review/SKILL.md` | 作者自审、论证和修改核验 |
| paper-figures-tables | `skills/paper-figures-tables/SKILL.md` | 架构图、实验图、表格与图注 |
| paper-policy | `skills/paper-policy/SKILL.md` | 学术事实、LaTeX 和已核实投稿要求检查 |
| academic-research-suite | `skills/academic-research-suite/SKILL.md` | ARS-Codex 套件：文献研究、论文起草、审阅、研究流程和实验规划 |

下载来源、实际 GitHub 提交版本和文件摘要见 `skills/sources.json`；简明列表见 `skills/README.md`。这些是按名称与用途选择的公开仓库版本。`academic-writer` 的共享依赖保存在 `skills/_shared/core/safety-rules.md`。

`academic-research-suite` 是用户额外指定的 Imbad0202/academic-research-skills-codex 中的单套件入口。按任务范围使用其路由；内嵌工作流及运行时资源随套件保留。安装不启用额外 hooks、改变当前模型或启动研究流程。

## 写作约定

- 以“FPGA 计算存储将持久化数据转为 FHE 加速器的输入密文”为叙事主线，明确给出支持它的实现和测量。
- Introduction 按研究类别概述文献，每类集中引用 3–7 篇，不逐篇点名介绍工作；具体工作及其机制、接口和差异放在 Related Work 中比较。方案定义所需的基础引用不受分组数量限制。
- 直接陈述已支持的贡献和结果。把必要的测量条件写在对应主张中，把影响结论的限制集中放在方法或讨论中。
- 保留事实、不利结果和实质性不确定性；避免防御性写作不意味着选择性隐藏证据。写作建议不覆盖用户要求或学术事实。
- 作者于 2026-10-06 确认：1 KiB 明文输入下，5.00× 对应 encryption operator 相对 host CPU encryption；2.45× 对应 end-to-end ciphertext production 相对 host CPU baseline。该结果取代旧的 4 KiB、5.33×、3.26× 表述。新测量的 value width 和具体配置由作者在实验章节说明，不自动沿用旧 unsigned 8-bit 限定。
- 作者于 2026-10-08 确认 Figure 5 的 CPU baseline 与 FHEStore 实际比较条件相同。曲线、右轴、图例及第 6.3 节使用 Speedup，保留 2.45×–2.77× 结果；旧源 JSON 的 unmatched/historical comparability 备注已被本次作者确认取代。数值快照保持原样，最新解释记录在 figures/input-hpu-ratio/author-confirmation.json，不再在正文添加不同环境的推断或相应防御性说明。
- 作者于 2026-10-08 再次明确：2.45× 是 1 KiB 明文 input preparation 的 FHEStore 相对 CPU baseline 加速比。6.3 中第二个操作数由输入派生，不能将两个构造的操作数缓冲区字节总和用于改写作者确认的明文输入量；保留摘要、引言和结论的 1 KiB 表述。
- 作者于 2026-10-08 明确 Table 2 以当前 PDF 数值为准，这是师弟测量的结果，对应报告不在本地记录中。保留 266538 LUT、343422 FF、21750 LUTRAM 等原值，不用本地其他版本的报告替换，不重复要求作者核对同一事项。
- 2026-10-08 全文复核后，末页使用标准 flushend 包，并通过 acmart 的 balance=false 关闭冲突的旧平衡算法。字号、页边距、标题与图文间距仍沿用 ACM；不同时加载/调用旧 balance。最后一页两栏齐底，正文在第 10 页内结束。
- 缺少实现、实验或引用依据时保留明确的待补充项，不编造数值、消融实验、参数、统计显著性或首创性。
- 改善自然表达时依据实际语言问题，不使用 AI 检测分数判断作者身份或论文质量。
- 保持 ACM `sigconf` 双栏匿名格式，保留标签、引用、公式和编译入口。修改排版后编译并检查最终 PDF。
- 保留模板生成的 ACM Reference Format 和首页会议脚注，不以清空 `\footnotetextcopyrightpermission` 隐藏它们。匿名草稿保留 `\setcopyright{none}` 和空 `\copyrightyear{}`，避免单独打印一个年份，会议年份仍为 2027。首页会议行使用原生会议简称和地点，日期保留在会议元数据中；未分配的 DOI/ISBN 留空。录用后按 ACM eRights 提供的命令填写最终出版元数据。格式依据见 `acmart-primary/FPGA27_格式核验.md`。
- 会议页数、投稿政策和日期以核实的官方要求为准；Skill 中的通用示例不作为具体会议规定。
