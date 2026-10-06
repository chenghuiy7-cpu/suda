# FHEStore 工作区

## 论文文件与依据

- 主文稿：`acmart-primary/FHEStore_FPGA27.tex`；编译产物为同目录下的 `FHEStore_FPGA27.pdf`。
- 文献库：`acmart-primary/FHEStore_refs.bib`。
- 题目和摘要以作者在会话中最新确认的版本为准；当前采用 2026-10-03 提供的 FHEStore 版本。原始参考文档为 `acmart-primary/FHEDock_题目&摘要.docx`。保持作者提供的题目、摘要和数值；修改这些内容需要对应的写作任务。
- 系统名称使用 FHEStore，主文稿和文献库使用上述 FHEStore 文件名。
- 后续章节直接在主文稿中写作和修改，并编译整篇论文；不再创建独立章节摘录。Introduction 和 Background 已有作者完成版本，后续章节的叙事、术语与符号应与它们对齐。
- 第二章标题固定为 Background，不添加 Motivation。
- 第三章保留系统组织、应用任务模型、算子图与设备执行三个小节。远端服务用于定位本工作的 CSD 与 Host 范围，概述中仅说明它是带有同态计算加速器的隐私计算外包原型，不展开 HPU 型号、内部实现或远端协议。
- 作者当前将工作聚焦于 ciphertext production。正文的工程贡献和执行路径不再包含 FPGA 解密与结果回写；Background 中的 FHE 正确性和基本解密定义可以保留。
- Figure 1 的数据流只保留 persistent data → Operator Pool → Host → external consumer 的生产方向。Device Runtime 的配置与完成控制箭头连接 Operator Pool，Storage-side data path 为分组边界。Host 通信功能使用 Ciphertext Transfer 名称，表示经 PCIe 获取密文并经网络交付外部消费者。
- 2026-10-06 作者要求 Figure 1 放在第三页顶部；通过提前声明双栏浮动图实现，编译后核对实际页码。检查普通正文页的左右栏末行对齐，以及同级标题前后与正文的实际可见间距，不能只检查标题宏定义或标题后间距。沿用 ACM 的标题定义和双栏排版，不用逐标题 vspace 或字号、页边距修改来修补。当前普通正文段落以零自然间距、有限伸展分担齐底余量，避免余量集中在标题前。
- 第五章为 Storage-Side Ciphertext Production，包含 Streaming Ciphertext Production、Execution and Output Management 两个小节。叙事从 SSD 加载开始，展开 input SLM、read DMA、同一个共享算子流交换机、算子链、write DMA、output SLM 与 Host 的关系。Selection/Projection 是算子组合示例。
- 第五章按作者 Figure 3 的方法层记法解释 Slot ID、Port ID、`{3,1}` 算子目的地和 `{F,2}` 内存输出目的地。Scheduler 建立下一跳绑定，正文着重于流的组织与执行；选择性任务承接第三章的 preprocessing 和 `Q`。
- 当前选择性任务先在 FPGA 上生成 selection manifest，Host 获取选中数量和索引后，复用同一 input SLM 执行 production pass。非空选择准确表述为一次 SSD 加载、两次 SLM 扫描；空选择跳过 production pass。production pass 内算子之间直接流交接。
- 2026-10-05 作者要求暂不修改摘要；从 Introduction 起按 production 主线核对全文。正文统一使用 ciphertext production、encryption operator、stream switch、ciphertext transfer、operator context、execution program、SLM range、selection manifest、discovery pass、production pass、terminal beat、valid ciphertext length 等术语；不同抽象对象保留明确的不同名称，不为同一对象随意换词。
- 具体原型参数、固定宽度和配置数值集中在 Experimental Evaluation；设计章节使用符号参数，不反复称系统为 prototype 或写成产品报告。摘要保持原样属于此次修改的明确例外；Figure 3 的路由值是作者示意图中的例子。
- 第三至第五章依据代码展开机制和设计理由，以研究论文方式组织，当前目标为实验前内容约七页。通过实质内容扩展达到篇幅目标，保持 ACM 双栏版式和作者确认的图，不用字号、间距或强制分页补足页数。

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
- 5.33× 对应 FPGA TFHE 加密相对主机 CPU 加密；3.26× 对应端到端输入准备相对主机基线。两项结果的输入为 unsigned 8-bit、批次为 4 KiB。
- 缺少实现、实验或引用依据时保留明确的待补充项，不编造数值、消融实验、参数、统计显著性或首创性。
- 改善自然表达时依据实际语言问题，不使用 AI 检测分数判断作者身份或论文质量。
- 保持 ACM `sigconf` 双栏匿名格式，保留标签、引用、公式和编译入口。修改排版后编译并检查最终 PDF。
- 保留模板生成的 ACM Reference Format 和首页会议脚注，不以清空 `\footnotetextcopyrightpermission` 隐藏它们。匿名草稿保留 `\setcopyright{none}`，未分配的 DOI/ISBN 留空；录用后按 ACM eRights 提供的命令填写最终出版元数据。格式依据见 `acmart-primary/FPGA27_格式核验.md`。
- 会议页数、投稿政策和日期以核实的官方要求为准；Skill 中的通用示例不作为具体会议规定。
