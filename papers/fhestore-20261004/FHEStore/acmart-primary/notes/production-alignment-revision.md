# Production 主线与第三至第五章扩展

日期：2026-10-05。主文稿：`../FHEStore_FPGA27.tex`；整篇 PDF：
`../FHEStore_FPGA27.pdf`。本记录保存代码依据与修改边界，不属于论文正文。

## 本次作者要求

- 摘要暂时保留，从 Introduction 起统一为 FHE ciphertext production。
- 同一对象使用固定术语；不同层次的对象明确区分。
- 具体实现参数集中在 Experimental Evaluation，设计章节采用符号描述。
- 依据代码扩充 Chapters 3–5 的机制和设计理由，使实验之前约七页结束。

Introduction、Background、设计章节、实验叙述、Conclusion 和附录写作提示均已
按 production 主线检查。Background 中的 FHE 正确性和基本解密定义保留；系统
贡献不包含解密 IP、远端结果回传或结果写回。远端仅作为外部消费者的位置，
其同态计算在实验中用于验证产出密文的兼容性。

## 术语约定

| 固定用词 | 含义与区分 |
| --- | --- |
| ciphertext production | 从持久化输入经访问、可选 preprocessing、encoding 和 encryption 得到密文的完整流程 |
| encryption operator | FPGA operator pool 内集成 encoding 和 TFHE encryption 的算子；不与整个 production 流程混同 |
| ciphertext transfer | Host 经 PCIe 获取已产出的密文，再经网络交付 external consumer 的功能 |
| external consumer / remote server | 前者是 producer–consumer 关系中的角色，后者是 Figure 1 中承载该角色的外部节点 |
| FHE accelerator | 远端同态计算加速器，区别于本工作的 encryption operator |
| task runtime / device runtime | 分别负责 Host 应用任务解释和 CSD 资源绑定、执行协调 |
| operator graph / execution program | 分别是计算连接抽象和提交设备的执行描述，两者不互作同义词 |
| operator context | 与逻辑算子关联的任务处理配置，包括相应本地 key material |
| SLM range | runtime 管理的设备内存对象、偏移和范围绑定；SLM 为 storage-layer memory |
| operator interconnect / stream switch | interconnect 包含 DMA、算子与交换机；stream switch 是其中执行流转发的组件 |
| selection manifest | discovery pass 的逐记录条目和数量汇总，Host 据此得到选中数量和 source-record mapping |
| discovery pass / production pass | 分别确定选择信息和产出密文；非空选择复用 input SLM，空选择跳过 production pass |
| ciphertext bundle | 一个 application value 的有序 radix LWE blocks，是应用层输出单元 |
| terminal beat / valid ciphertext length | 分别为执行结束帧和可交付密文的字节长度；终止帧与分配填充不属于有效密文 |

摘要内旧称谓按作者要求保留。章节标题、句首和图中标签的大小写以及作定语时
的连字符不改变上述术语。Figure 3 的 `{3,1}`、`{F,2}` 等是作者示意图中的
路由例子，未把固定实例的宽度、容量和配置带回设计正文。

## 第三章：应用语义到设备执行

保留 System Organization、Application Task Model、Operator Graphs and Device
Execution 三个小节。第一节说明 CSD/Host 的范围和 PCIe/网络交接；第二节以
`T=(D,Q,E,O)` 定义输入、preprocessing、encoding/encryption 配置与输出，说明
有序密文和选中记录之间的关系；第三节解释 graph、context、SLM range 和
execution program 如何组合，及 completion 与输出获取之间的契约。

代码路径以下均相对 `/home/yangchenghui/suda`。

| 机制 | 主要依据 |
| --- | --- |
| 输入范围、schema、query 与配置检查 | `host/applications/vscode-selective-lwe-full-pipeline/run_task.py:63–109,134–169,195–201`；同目录 runner `.cpp:487–519` |
| 本地 key 加载、binary validation 和 context 组装 | 同 runner `.cpp:687–789,2027–2030,2075` |
| 算子需求和逻辑连接表达 | 同 runner `.cpp:791–835`；`host/api/libnvme/src/nvme/types.h:8073–8123` |
| SLM range 与执行绑定 | 同 runner `.cpp:2290–2377`；`types.h:8042–8062`；`host/api/libnvme/src/nvme/ioctl.c:2651–2668,2697–2742` |
| 设备算子、上下文和目的地绑定 | `device/platform/software_stack/nf_spdk/lib/hlsacccompute/hlsacccompute.c:706–784,1019–1057,1178–1217` |
| mapping、空选择、完成长度 | 同 runner `.cpp:1902–1992,2249–2271,2365–2440`；`device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c:883–927,1691–1708` |

## 第四章：编码、算术与序列化

标题采用 HLS Design of TFHE Encryption。保留 CPU coefficient layout，以
`w,p,K,n,q,Delta,W,L` 表达数据和密码表示。四个小节依次解释接口、输入模式
和内部 encoding、单遍 coefficient arithmetic 与状态、packetization 和
completion。增加累计递推和 serialized ciphertext bundle size 公式。

| 机制 | 主要依据 |
| --- | --- |
| AXI4-Stream 与 BRAM context | `device/operators/hls/lwe_encrypt/lwe_encrypt.cpp:367–383`；`device/shared_components/hls/hlsacc_types.hpp:9–12,33–38` |
| packed/scalar 输入与 count-bound 消费 | encrypt `.cpp:420–487`；encrypt `.hpp:106–118` |
| radix digit 提取、内部 encoding 和逐 digit 复用 core | encrypt `.cpp:317–364` |
| packed binary key 和系数累计 | encrypt `.cpp:33–41,238–280` |
| mask 生成同时进入累计与 packetizer，保留有限状态 | encrypt `.cpp:251–279,329–339` |
| beat 打包、body flush 和块末 zero padding | encrypt `.cpp:44–65,124–139,279` |
| terminal beat、固定数量仍等待终止帧和过早终止 | encrypt `.cpp:97–121,425–440` |

这里 encrypt `.cpp/.hpp` 指上述 `lwe_encrypt` 路径。主文稿不新增 loop II、
多 core、并行 digit、吞吐率或资源结果。mask PRNG 的具体 xorshift 实例移至
实验配置，不添加密码安全采样或精确分布保证，也不插入产品式 caveat。

## 第五章：路由、流控与数据相关输出管理

保留 Streaming Ciphertext Production 和 Execution and Output Management。
第一节从 SSD 加载到 input SLM 开始，区分 DMA 地址/范围与 TDEST 下一跳，
按 Figure 3 解释 scheduler、operator controller 和同一个 stream switch。
Selection/projection 用于承接前文 preprocessing，production pass 内直接把
选中字段交给 encryption operator。第二节讨论 ciphertext expansion 引出的
输出容量、discovery pass、RX-before-TX、有效长度和分批 source-record mapping。

| 机制 | 主要依据 |
| --- | --- |
| SSD 加载、复用 input SLM、production pass 连接 | selective runner `.cpp:2161,2206,2290,2359` |
| manifest 格式、解析、数量与记录身份检查 | 同 runner `.cpp:1902–1971`；同目录 `selection_manifest.hpp:14–39` |
| 未选中记录字段为零，选中条目保留 projected value | `device/operators/hls/selective_filter/selective_filter.cpp:318` |
| 下一跳 tag、输入 FIFO 与 backpressure | `device/platform/ips/rtl/OperatorController.v:728,755,768,815` |
| receive descriptors 先于 transmit 启动 | `hlsacccompute.c:1319` |
| 接收终止帧并报告 payload 字节数 | `mcdma.c:883–927,1153–1157,1691–1708` |
| sequential batches 与全局记录偏移 | `run_task.py:173,217–251`；runner `write_selection_result:1978` |

代码核对后，明确非空选择一次 SSD load、两次 SLM scan；空选择不运行
production pass。没有声称 discovery 不向 Host 传 projected plaintext。
Figure 3 保留作者的方法层示意，不把底层物理目的地重映射展开成工程流水账。

## 参数迁移与保留项

具体 value/radix/coefficient/stream width、LWE dimension、encoding interval、
packed key size、logical/serialized bundle bytes、allocation alignment 和
terminal-beat bytes 集中到 Experimental Evaluation 的配置表。具体 PRNG
实例、block beats/padding 和 encoding bit positions 也仅在该章节说明。
Introduction 和 Conclusion 保留已有 5.33×、3.26× 结果，配置条件指向实验。

摘要内容、三幅图资产、已有 citation commands、已有 labels 和原有两项性能
数值保留。实验平台、测量边界和更多结果的原有 Writing guide 待补项没有
被替换成编造的数据。本次没有新增性能实验或修改硬件代码。

## 编译与复核

- `latexmk -pdf -interaction=nonstopmode -halt-on-error FHEStore_FPGA27.tex`。
- 同一对象的术语、prototype/HPU 残留和实验外的固定实例参数检查。
- 摘要 exact comparison、citation commands、旧 labels、图资产 SHA-256 检查。
- 分别复核任务/执行、encryption datapath、production workflow 的代码依据。
- PDF 为 9 页，Experimental Evaluation 从第 7 页后半段开始。
- Figure 1 在第 4 页，Figure 2 在第 5 页，Figure 3 在第 7 页且占单栏。
- 可见页面已渲染检查，未通过修改字体、页边距或强制分页补足篇幅。
- 最终日志无编译错误、未定义引用或 overfull box。模板浮动体与双栏布局仍
  产生 underfull 警告，渲染无内容越界。

## 2026-10-06：Figure 1 与栏底、标题间距

作者要求 Figure 1 移至第三页顶部，并检查原第四页左栏末行偏高和同级标题
间距。只提前图的浮动声明到 Background 的最后一个小节之前，保留 `[t]`
和全部图内容，不改变正文、图资产、标题宏、字体、页边距或强制分页。

原第四页左栏末行 bounding box 底部为 689.589pt，右栏为 710.485pt，相差
20.896pt。`acmart.cls:749` 已启用 `flushbottom`，但纯段落栏没有可伸长的
垂直 glue；另一栏的标题前 glue 吸收剩余栏高。这一问题在上次渲染检查中
漏检。图位置调整后重新排版，第四页两栏末行均为 710.485pt，第二至第七页
普通正文栏的末行位置也一致。

同级标题使用 `acmart.cls:3267–3279` 的统一定义。Section 和 subsection
到后续正文的 after-skip 都为 0.25 个 baseline；标题前 glue 由模板用于
分页伸缩。当前正文没有实际 subsubsection，也没有逐标题间距覆盖。
逐项提取 PDF 标题和后续普通正文的文字位置，subsection 的可见边界间距
为 2.660–2.661pt；实验提示采用 italic 字体，其 glyph bounding box 不同，
但仍使用相同的标题 after-skip。

重新编译整篇 PDF 并检查受重排影响的第三至第九页：Figure 1 位于第三页
顶部，Figure 2 仍在第五页，Figure 3 仍在第七页单栏。实验从第七页开始，
整篇仍为九页。摘要、全部正文文字、citation commands、labels 和图资产
校验保持一致；日志无编译错误、未定义引用或 overfull box。

## 2026-10-06：Background 命名与标题前留白复核

第二章改名为 Background。上一轮只测量了标题后的间距，未充分检查齐底
造成的标题前伸展。作者指出的可见差异存在：第二、三、四章标题前的
文字边界间距原为 10.891、35.374、19.210pt。

保留 ACM 的 section/subsection 定义、字号、页边距和 flushbottom。
普通正文段落之间使用 `\bodypar`，自然间距为零、可伸展量设为 3pt，
让段落边界与标题前 glue 一起分担齐底余量；此命令不紧邻标题、公式、
列表或浮动体。Background 最后一段、System Organization 第一段和
Integrated Encoding and Encryption 第一段各选择多一行的断行方式。
第四章开头和第五章末尾各按语义分为两段，全部文字、公式及引用不变。

最终主 PDF 中，第二、三、四章标题前的可见间距分别为
10.891、11.047、11.316pt。普通正文到标题的间距约为 10.2–13.2pt，
保留模板正常的有限伸缩；22 个标题到普通正文的可见间距均为
2.660–2.661pt。章标题后紧接小节标题、跨行标题、斜体 Writing guide
和参考文献使用不同上下文或字体边界，不将这些测量与普通正文混同。

重新编译并渲染检查标题所在页面，第二至第七页正文左右栏末行位置
均为 710.485pt。Figure 1 保持第三页顶部，Figure 2 位于第五页，
Figure 3 保持第七页单栏，实验仍从第七页开始，整篇仍为九页。
与本轮修改前源文稿逐项比较：除第二章名称、上述排版控制和分段外，
正文文字、摘要、公式、citation commands、labels 和图路径不变；
三幅作者图资产 SHA-256 不变。最终日志无编译错误、未定义引用或
overfull box，剩余三个 underfull hbox 已检查渲染，无越界。
