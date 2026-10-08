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
| SLM range | runtime 管理的设备内存对象、偏移和范围绑定；SLM 为 Subsystem Local Memory |
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

## 2026-10-06：Introduction 与输入准备动机图

作者确认图中两组采用同一条 Host CPU input preparation 路径：SSD 读取、
encoding、encryption，到密文在 Host 内存中就绪。唯一改变的执行阶段是
后端 homomorphic computation，由 CPU 软件改为开源 HPU 加速。
网络传输不计入。CPU-only preparation 与 CSD preparation 的比较不是
这幅图的实验变量。

Introduction 按六段重新组织：FHE 加速与持久化输入需求、CPU/HPU 对照
测量、Host 生产成本、现有研究与本文边界、CSD 放置理由与 FHEStore
机制、主要结果；随后保留三项贡献。输入准备占比从 0.11%–0.75%
上升到 17.7%–36.6%，解释为消费侧加速提高生产侧的相对重要性。
筛选只用一句作为可选加密前 preprocessing 的例子，说明排除无关数据
能够减少后续密码计算和密文搬移，不将其加入动机图的测量步骤。

消费侧加速文献归入首段，加密硬件和 client-side/end-to-end 系统文献
归入研究定位段，CSD 参考文献归入方案段。保留现有 citation keys，
未新增文献库条目。5.33× 和 3.26× 分别对应 TFHE encryption 和
end-to-end ciphertext production，结果数值只在引言结果段出现一次。
没有将加速结果扩展到包含同态计算的整个应用或网络传输。

动机图直接使用作者附图的原始 PNG，保存为
`figures/input-preparation-motivation-v1/author-upload.png`，尺寸
1504 × 639，SHA-256 为
`471e27fcb20ed8b90b7fde11c2497d9e1aa2d424e9403cf904ad07f17adedc0b`。
未重绘、裁切、重采样或改变误差线。作者确认柱形采用平均值、误差线
表示观测到的最小值与最大值，已在图注说明；原始测量与重复次数尚未
提供，没有据图推断这些数据。其 provenance
保存在同目录的 `source-info.json` 和 `README.md`。

新增动机图为 Figure 1；系统架构、加密 IP、算子互连图的编号依次
顺延为 Figure 2、3、4。提前系统架构图的浮动声明，使其继续位于
第三页顶部。题目与摘要保持原样，Background 及后续章节的正文与
公式保持原样；浮动声明和必要的分段、断行控制属于版式调整。

整篇重新编译后，动机图位于第二页，系统架构图位于第三页顶部，
加密 IP 图位于第六页，算子互连图位于第七页且保持单栏。
实验从第八页开始，整篇为十页。新增图导致重新分页，没有通过
改变字号、页边距或强制分页调整篇幅。

有限正文段落伸展由 3pt 调为 6pt，自然段落间距仍为零，并通过
两处语义分段和一段局部断行减少标题前集中伸展。最终普通标题到
正文的可见间距仍为 2.660–2.661pt，普通正文到标题的间距约为
10.7–14.2pt。第七页的图注到标题、第八页的表格到标题属于浮动体
间距，未将其与普通正文边界强行等同。普通正文栏保持模板齐底，
数学、斜体和正文字形的 bounding box 底边差异不作为基线错位。

最终源稿逐项比较通过：题目、摘要 exact match，Background 及后续
正文文字、公式和 citation commands 不变，原有 labels 和三幅图资产
SHA-256 保持一致；动机图原始字节保持一致。最终日志无编译错误、
未定义引用或 overfull box，剩余三个 underfull hbox 渲染无越界。


## 2026-10-06：动机图首页单栏与加粗文字

按作者最新要求将 Figure 1 改为第一页右栏单栏。横排原图直接缩至栏宽
时，标签只有约 4pt；改为上下两个面板，正文放置时标签约 7.9–8.9pt，
图例、坐标轴、操作名称、面板标题和百分比均加粗。纵轴采用
Latency share (%)，明确表达准备与同态计算延迟之和中各阶段的占比。
图注保留同一 Host CPU preparation、平均值、min/max 和不计网络传输
的定义。

新版本保存在 `figures/input-preparation-motivation-v2/`。均值柱形与
文字为矢量，八个百分比来自作者确认的数值；没有原始数值的误差线
通过原始 PNG 的局部裁剪和相同百分比坐标映射保留，不推断边界或
重复次数。原始 PNG、系统架构图、加密 IP 图与算子互连图 SHA-256
均不变。保留生成脚本、可编辑 SVG、PDF、README 和 provenance。

整篇重新编译并检查实际 PDF：动机图在第一页，系统架构 Figure 2
仍在第三页顶部；Figure 3 在第六页，Figure 4 在第七页，实验仍从
第八页开始，全文十页。题目、摘要、全部正文、公式、citation commands
和 labels 与此次版式修改前一致。日志无编译错误、未定义引用或
Overfull box；后部模板和参考文献的 Underfull 警告经渲染检查无越界。
ACM 标题定义、栏宽、字号、页边距与齐底规则未改动。


## 2026-10-06：动机图横排与短图注

作者要求首页单栏图的两个面板并列为一行，图注压缩，必要的测量解释
移入正文。新版本位于 `figures/input-preparation-motivation-v3/`：
242 × 169pt 的两面板图共用图例、百分比纵轴及刻度，文字保持 8–9pt
加粗。横轴为 AND/RShift/Add/Mul，正文写出四种完整操作名称。仅标注
准备阶段的八个均值，省去百分号后缀和计算阶段的补值标注，计算阶段
仍由 100% 堆叠完整表达。原始误差线与源图字节保留。

图注缩为 10 个英文词、实际两行：Latency shares with (a) CPU and
(b) HPU homomorphic computation. 相同 Host CPU input preparation、
归一化分母、均值与 min/max、网络传输不计入等定义合并到引言第二段，
去掉该段结果句中重复的分母说明。题目、摘要、Background 及后续正文、
公式、引用与 labels 保持不变。

横排图与短图注引起重排；发现手动提前启用 bibliography balancing
使第九页参考文献末行下侵正文底界。移除该手动条件块，保留 acmart
默认的末页平衡，参考文献自然续到第十页；不修改 class、字号、页边距
或强制分页。独立临时编译及实际渲染验证该可见越界已修复。末页短
appendix 的内部平衡仍有小幅 Overfull vbox 警告，实际文字没有越界，
不为消除警告扩大排版修改。


## 2026-10-06：横向细条与移除 Right shift

按作者最新要求改为横向 100% 堆叠条形，CPU 与 HPU 两面板仍并列
为一行，图仍在第一页单栏。条形厚度 8pt，操作名称完整共享显示在
左侧；数值轴改为水平 Latency share (%)，保留加粗文字与短图注。
两组均移除 Right shift，Introduction 操作列表及 Description 同步
只列 Bitwise AND、Addition、Multiplication。CPU 准备占比为
0.75/0.38/0.11%，HPU 为 36.6/26.9/17.7%；范围句仍准确。

新版本位于 `figures/input-preparation-motivation-v4/`，保留生成脚本、
SVG、PDF 与 provenance。原始误差线像素局部裁剪后按百分比轴旋转，
原图 y=444/85 分别映射到水平 0/100%，未推断新的误差边界。原始
PNG 字节不变。题目、摘要、Background 及后续正文、公式、citation
commands 和 labels 均不变。

重新编译与实际渲染检查通过：Figure 1 在第一页，Figure 2 仍在第三页
顶部，Figure 3 在第五页、Figure 4 在第七页，实验从第八页开始，
全文十页。六个数值、全体加粗文字、操作删除与误差线旋转均经独立
agent 复核。正文栏底没有可见越界，末页短 appendix 的内部平衡仍有
小幅警告，无需扩大版式修改。


## 2026-10-06：收窄操作标签与补全两阶段数值

按作者要求把共享左侧操作名称缩为 AND/Add/Mul，标签区由 58pt
收窄到 26pt，释放的宽度用于两组条形。条形仍厚 8pt，中心间距
由 30pt 缩为 23pt。每条下方同一行同时显示 preparation 与
homomorphic computation 份额，左侧蓝色、右侧深橙色；原柱形橙色
不变，数值使用更深的同色系提高白底对比。

新版本位于 `figures/input-preparation-motivation-v5/`，尺寸为
242 × 133pt。CPU 两阶段为 0.75/99.25、0.38/99.62、0.11/99.89%，
HPU 为 36.6/63.4、26.9/73.1、17.7/82.3%，均与作者图对应，每组和
为 100%。误差线保留源像素并按新 100pt 图宽旋转到水平百分比轴，
未反推 min/max；原始 PNG 字节不变。全部文字仍 8–9pt 加粗。

整篇重新编译并按实际栏宽渲染，数值与标签无重叠，独立 agent
检查通过。Figure 1 仍在第一页单栏，Figure 2 仍在第三页顶部，
全文十页。主文稿仅调整动机图路径和 Description，短图注、全部正文、
题目、摘要、公式、引用和 labels 不变。日志没有新增的越界问题，
原末页短 appendix 的内部平衡警告不扩大处理。

## 2026-10-06：将已确认的 SVG 用于主文稿

直接将 v5 的既有 SVG 导出为 PDF，并由主文稿 Figure 1 引用，未重跑
生成器，保留该 SVG 的全部内容。源文件 SHA-256 和导出方式记录于
v5/source-info.json，README 补充直接导出命令。重新编译通过，第一页
实际渲染确认仍为右栏单栏图，十二个份额数值和加粗字体正常。正文、
图注及版式未改，全文仍为十页。

## 2026-10-06：引言与第三至第五章的叙述对齐

根据作者确认的修改方向，补强引言中存储侧组织的理由：持久化输入、
算子流交接和膨胀密文输出构成设备侧执行路径。贡献列表明确 HLS 算子
每个 LWE block 的一次 mask 遍历同时用于点积累计和增量输出，以及
任务控制、流控制、输出管理与 ciphertext expansion 的关系。

第三章将任务公式推广为通用的有序应用值 x_j 到 ciphertext bundle
c_j，再用 x_j=projection(r_i) 展开 selective case；保留原公式 label。
Q 可省略，O 定义为 Host 内存位置，source-record mapping 限定于选择性
任务。说明 device completion 为 output SLM 中就绪，application
completion 为 Host 内存中就绪，外部消费者转发发生在 production 之后。
第四章开头按有序输入与输出序列承接这一任务模型。

第五章先讲直接输入到 encryption 的流路径，再展开 optional preprocessing。
5.2 将通用接收设置、terminal beat 与 valid ciphertext length 提前，随后
讨论选择性任务的 discovery pass。补充共同 output contract：相同配置下，
packed input 与 projected scalar input 的 serialized ciphertext representation
相同，容量和有效长度按 M 与 C 解释，独立于中间流路由。选择性任务的
一次 SSD 加载、两次 SLM 扫描、空选择处理和 sequential batching 均保留。

代码依据与修改语义经独立复核；摘要、题目、Background、实验及后续章节、
引用命令、labels、图片资产与图注均保持原样。最终编译通过，Figure 1/2/3/4
分别在第 1/3/5/7 页，实验从第 8 页开始，全文 10 页。第 1、3、4、7、10 页
实际渲染检查通过，第七页完整结束第五章，附录不跨出两行尾段。ACM 标题、
geometry、balance 均未修改；标题后正文的测量间距保持统一，无新增可见越界。

## 2026-10-06：特定术语首次解释与缩写检查

按作者要求检查全文首次使用。引言简释 encryption operator 为 streaming
FPGA processing module，operator context 为传入单个算子任务参数的配置。
第三章首次使用 operator graph、execution program、selection manifest、
valid ciphertext length 时补充功能或单位；第四章首次使用 beat 和 terminal
beat 时解释流传输单位和 completion metadata。production pass 留到第五章
首次解释为将 projected values 流式送入 encryption 的执行，discovery pass
及其两阶段机制保持原样。没有新增术语表或修改图片。

正文补齐 CPU、SSD、FPGA、TFHE、LWE、PCIe、SLM、AXI、BRAM、PRNG、DMA、
FIFO、LUT、FF、DSP 的展开；HPU 调整为在缩略形式前展开。FHE、HLS、CSD
已有首次定义，后文避免重复展开。SLM 的正式名称为 Storage Layer Memory，
本地依据为 `suda/docs/api/APIReference.md:131` 及本记录前文的 SLM range
定义。AXI4-Stream 保留协议名并展开 AXI；TKEEP/TDEST 为信号名，已有语义
说明，未编造全称。AND/Add/Mul 为操作或图标签，KiB 为单位，C++ 为语言名。
Writing guide 中单次使用的 storage I/O 改为一致的 storage access。

同时落实上一轮术语审查：自然引入 ciphertext producer/consumer，保持
input preparation 与 ciphertext production 两视角；Background、Conclusion
及 Figure 2 Description 的 production 边界统一到 Host memory，preprocessing
为可选。第四章接收路径使用 device completion，区别于应用完成。旧实验
writingguide 的网络优化/流量要求收回到 production 范围，无新测量主张。

摘要与题目保持作者当前版本；摘要里的 CPU、FPGA 未展开及 input production
旧用词已标记，留待摘要写作任务。引用、labels、编号公式和图片资产保持原样。
定义语义经独立复核，最终编译全文 10 页，Figure 1/2/3/4 位于第 1/3/5/7 页，
实验从第 8 页开始。引言、系统定义、HLS 接口和资源表的实际渲染无重叠或越界。

## 2026-10-06：按作者约定修正缩写与实验参数表

删除 Encryption and stream configuration for evaluation 表及其关联参数说明，
移除对该表的交叉引用；实验平台表中的 Encryption configuration 恢复为
待补充。该表源码原在实验章节，但浮动位置落在第五章末尾。实验参数由作者
自行说明，不在设计章节补入固定配置。

撤回上一条记录中对 CPU、SSD、FPGA、FIFO、LUT、FF、DSP 等常见缩写的
机械展开；PCIe、DMA、AXI4-Stream、BRAM 也按 FPGA 领域读者的常用术语处理。
保留 FHE、TFHE、LWE 等领域缩写的首次展开，以及本工作中特定术语的简短定义。
摘要与题目仍保持原样；摘要的 input production 用词留待摘要写作任务处理，
CPU、FPGA 不再列为需要补全称的事项。

SLM 统一为 Subsystem Local Memory，取代上一条记录对旧本地 API 文档名称
的引用。已核对作者提供的 [NVM Express 官方说明](https://nvmexpress.org/nvm-express-computational-storage-standardizing-storage-management-and-reducing-storage-and-latency-costs-in-the-enterprise/)，
其中明确列出 Subsystem Local Memory Command Set。正文只采用该术语，
不附加标准符合性主张；本地代码与 API 文档不在此次论文修改范围内。

最终编译为 9 页，第五章在第 7 页结束，实验从第 8 页开始；Figure 1/2/3/4
仍位于第 1/3/5/7 页。删表后默认末页 balance 在第二栏调用，造成末页两栏
长度不齐，因此在参考文献开始的第一栏显式调用一次原生 `\balance`。
重新编译和实际渲染确认末页齐底、无越界，未修改标题、字体、页边距或图片。
最终日志无编译错误、未定义引用、overfull 或 balance 警告，只有三个既有
underfull hbox。题目、摘要、全部编号公式、citation commands 和图环境保持
不变；唯一移除的 label 是已删配置表的 label。

## 2026-10-06：引言明确动机测量所用的 HPU

按作者确认，仅将 Figure 1 对应句中的 HPU 描述改为 Zama's open-source
FPGA-based homomorphic processing unit (HPU)，并加入 `zama2025hpu` 引用。
元数据采用 [官方 README 的 Citations 段](https://github.com/zama-ai/hpu_fpga#citations)
推荐条目，作者为 Zama、年份为 2025；已于 2026-10-06 独立核对。
板卡、版本和配置不加入引言正文，其他正文及图片不变。

重新编译通过，全文仍为 9 页，四幅图仍位于第 1/3/5/7 页，实验从第 8 页
开始。首页与末页实际渲染正常，最终日志无未定义引用、overfull 或 balance
警告，保留三个既有 underfull hbox。

## 2026-10-06：采用作者确认的三段摘要并同步测量结果

摘要采用作者确认的原文：以 ciphertext producer/consumer 引出定性动机，
先说明整合 radix encoding 与 TFHE encryption 的 HLS operator，再提出
streaming ciphertext production 方法。摘要不加入动机占比或远端 HPU
演示句。题目保持原样。

作者最新确认的结果为 1 KiB plaintext input、encryption operator 5.00×、
end-to-end ciphertext production 2.45×，均相对于对应 host CPU baseline。
已同步摘要、引言结果句、实验结果表及其说明、测量范围说明和结论，取代
此前的 4 KiB、5.33× 和 3.26×。本次未给出新测量的输入值宽，移除旧结果
附带的 unsigned 8-bit 限定；完整实验配置仍由作者补充，代码未修改。
历史修订记录保留原始数值，以便追溯。

对摘要末段局部设置 `\emergencystretch=1em`，修正一行轻微越出栏宽，
未改作者文字、字体、页边距或标题间距。最终编译通过，全文 9 页，四幅图
仍在第 1/3/5/7 页，实验从第 8 页开始。首页及第 8–9 页实际渲染复核正常；
最终日志无编译错误、未定义引用、overfull 或 balance 警告。三个既有
underfull hbox 和第 8 页 underfull vbox 未造成可见越界或栏底异常，
保留原生末页平衡。作者确认的摘要逐词一致，题目、编号公式、labels、
citation commands、图环境和四幅图片资产均与本轮修改前一致。

## 2026-10-06：摘要首句改为补充说明消费者角色

按作者确认，将摘要首句中的 `encrypted data and serve as ciphertext
consumers` 改为 `encrypted data, serving as ciphertext consumers`。
其余摘要、全文正文和数值逐字保持不变。重新编译通过，首页实际渲染
正常；全文仍为 9 页，四幅图位于第 1/3/5/7 页，实验从第 8 页开始，
未新增 overfull、未定义引用或 balance 警告。

## 2026-10-06：更新作者确认的论文题目

完整题目改为 `FHEStore: A Streaming FHE Ciphertext Producer with
FPGA-Based Computational Storage`，短标题同步为 `FHEStore: A Streaming
FHE Ciphertext Producer`。主文稿仅修改 `\title` 命令，摘要、正文、数值、
引用、公式和图片保持不变；工作区约定同步记录最新题目。

重新编译通过，PDF 元数据及 ACM Reference Format 自动采用新题目。
首页题目自然排为两行，实际渲染无越界。全文仍为 9 页，四幅图位于
第 1/3/5/7 页，实验从第 8 页开始；无新增编译或排版警告。

## 2026-10-06：新题目确定后的全文语义与排版审查

审查主文稿全文（含实验、Related Work、Conclusion、参考文献和附录），
并实际渲染查看全部 9 页。另核对本地引用库、交叉引用、公式符号和部分
对应代码；本轮不新增文献或测量，不改作者确认的题目、摘要及图片。

整体叙事一致：第四章的增量密文生成和第五章的算子流组织对应摘要两层
方法；input preparation 与 ciphertext production 均以 Host memory 中
密文就绪为应用边界。Device completion 与 application completion 的区别
清楚，selection 为可选 preprocessing。SSD 加载和算子流执行的顺序、
selective task 的一次 SSD load 与两次 SLM scan 均明确。

落实五处局部修正：

1. 引言方案段末句改为 `We jointly design ...`，修正 organization 作为
   designs 执行主体的不自然表述，保留原设计含义。
2. 第二项贡献标签统一为 `Streaming ciphertext production`，与摘要
   方法名和第五章小节名一致。
3. Radix 公式之前明确输入为 unsigned w-bit application value，补齐
   数学取值域；代码 `device/operators/hls/lwe_encrypt/lwe_encrypt.cpp`
   的 ap_uint 输入支持该解释。正文未将 w 固定为 8，未给新测量附加
   未确认的值宽或参数。
4. 实验测量边界段的 host baseline 统一为 host CPU baseline。
5. 结论首句直接说明 FHEStore 实现 streaming ciphertext production，
   与新题目及摘要的方法定位衔接。

未完成内容保留为作者待补：实验平台表有 10 个 To be supplied 条目；
实验、Related Work 和复现附录共保留 9 处 Writing guide。动机 Figure 1
的 CPU/HPU profiling 配置和两项新 speedup 的计时起止点应分别说明，
不能把新结果的 1 KiB 输入量自动视为 Figure 1 的配置。完整 Related Work
比较和复现步骤尚未填写。本地引用 key 均存在、labels 无重复且交叉引用
可解析；两篇未使用 bib 条目保留供后续 Related Work 写作使用。

编译通过，全文仍 9 页，四图位于第 1/3/5/7 页，实验从第 8 页开始。
全部图表、公式无可见越界；普通正文页栏底正常。22 个普通标题到 Roman
正文的 top delta 为 12.969–12.970 pt，局部修正前后标题位置和间距不变。
第 5 页标题前的几何脚本大间距来自跨栏图内容分类，不是实际空白异常。
末页原生 balance 有约一个参考文献行高的离散残差，无需额外干预。
最终日志保留三个既有 underfull hbox 和第 8 页 underfull vbox，未产生
新的排版警告；无 overfull、未定义引用或 balance 警告。题目、摘要、
编号公式、labels、citation commands、图环境及四幅图片 SHA-256 与
本轮修改前一致。未修改代码或提交/push。

## 2026-10-06：CCS、Keywords 与首页会议脚注

对照作者提供的 HERA FPGA ’26 论文首页，保留原生 ACM 栏目名
CCS Concepts 和 Keywords；栏目名首字母大写并加粗。三项 CCS 均使用
500 权重，以原生样式加粗：Hardware accelerators、Storage architectures、
Cryptography。关键词正文使用普通字体，采用首字母大写。

按作者最新要求，关键词保留 High-Level Synthesis，不加入 Ciphertext
Production。六项关键词为 Fully Homomorphic Encryption、TFHE、FPGA、
High-Level Synthesis、Computational Storage、Streaming Dataflow。正文中
ciphertext production 的方法与功能名称不变。正式投稿元数据中的对应
CCSXML 留待通过 ACM 分类工具生成，未编造分类标识。

HERA 首页会议行实际只列会议简称与地点，不列日期。本稿保留原生
`FPGA ’27, California, USA`，March 14–16, 2027 仍在会议元数据中；日期
已与 FPGA ’27 官方 CFP 核对。匿名草稿保留 setcopyright{none}，将
copyrightyear 置空，消除单独的 `2027.` 行。最终版权文本、ISBN 和 DOI
依照 ACM eRights 提供的出版命令填写，不将 HERA 已出版论文的版权
文本或美国政府许可说明直接复制到草稿。

重新编译成功，全文仍为 9 页，四幅图位于第 1/3/5/7 页，实验从第 8 页
开始。实际查看首页及末页，无可见溢出；无新增排版警告。题目、摘要、
Introduction 起的全部正文与本轮修改前逐字一致，未提交/push。


## 2026-10-08：删除合并稿附录

按作者要求，在 FHEStore_new 主稿中删除 Reproducibility Details 附录及
其 Writing guide 模板文字。Related Work、Conclusion、摘要、正文实验
和全部数值保持不变，本轮只讨论两节的后续组织，不提前实施重写。

删除附录后，本地重新编译的末页参考文献因自定义 interlinepenalty=10000
禁止条目内换栏而出现 6.21349 pt Overfull vbox。移除该自定义钩子，
恢复 ACM/natbib 原生断行惩罚并保留 balance。最终编译仍为 12 页，
附录文字已消失；无 Overfull、未定义引用或 balance 警告。实际查看
末页，参考文献无越界。保留原生参考文献跨栏行为，没有改变字体、
页边距或标题间距。此前检查出的正文页数问题尚未在本轮处理。

备份与编译核对产物位于 /tmp/fhestore-remove-appendix-20261008。
未修改原路径旧稿，未提交或 push。

## 2026-10-08：重写 Related Work 与 Conclusion

对照作者提供的 HERA 论文 PDF 第 10 页（印刷页码 274）的第 6、7 节，
借鉴按类别概括机制、集中引用并比较设计差异的 Related Work 组织方式，
以及以核心设计和实验结果收束的 Conclusion 写法。保留适合 FHEStore
的四类：同态计算加速、加密与客户端处理、可计算存储、存储耦合同态
计算。比较集中于生产阶段、增量密文输出及算子流组合，不罗列工程接口，
不将前人工作的未提及功能表述为缺失。

结论概括 HLS 加密算子的系数累积与增量输出、可选预处理和加密的流式
组合及输出管理，保留 1 KiB 下 5.00× 和 2.45× 两项已报告结果，纳入
Host CPU time、peak memory 收益及 HPU 正确性验证。未新增测量或
改变实验边界。

保留全文全部 31 个引用 key；题目、摘要、方法章节、实验文字和数据、
参考文献库及图片不变。单个原生 balance 命令移至末页区域的首栏，
避免改写后的第二栏触发 balance 警告；未修改字体、页边距或标题间距。
最终 PDF 为 12 页，Related Work 与 Conclusion 完整位于第 11 页，
第 12 页为参考文献。重新编译并实际检查两页渲染，无 Overfull、
未定义引用或 balance 警告。正文页数问题留待后续整体压缩处理。

备份和核对产物位于 /tmp/fhestore-related-conclusion-20261008。
只修改 FHEStore_new 主稿，未提交或 push。

## 2026-10-08：正文压缩至 10 页

按作者要求通读全文，压缩参考文献之前的正文至第 10 页结束。保持原生
ACM sigconf 字号、页边距、标题定义和图片尺寸，未删除章节、实验、图表
或公式。全文 PDF 为 11 页，References 从第 10 页开始，第 11 页仅有
参考文献。

主要调整：

- Introduction 收紧动机和方案概述，将独立性能预告并入 Evaluation
  contribution，保留同一 Host CPU preparation、仅更换计算后端、SSD 到
  Host memory 的边界、网络排除及 Figure 1 的统计定义。
- Background 删除与引言及 System Organization 重复的两段生产路径
  过渡，压缩 Computational Storage 的泛化铺垫，保留基础公式、可信域
  和 producer/consumer 的参数、编码与布局兼容条件。
- System Overview 压缩系统职责及任务字段的复述，保留 graph/context/
  SLM range 之间较完整的接口解释与 device/application completion。
- HLS Design 保留输入模式、参数、接口、编码、包化细节，收紧引导及
  系数/状态总结；mask 同时累积和增量输出的设计仍完整。
- Storage-Side Production 收紧预处理、速率协调与批次总结，保留
  路由绑定、容量公式、三种 padding、manifest、Host 获取 count/mapping、
  一次 SSD load 与两次 SLM scan、空选择及 source-record mapping。
- Experimental Evaluation 简化各小节重复的 CPU/CSD 路径介绍、
  图表复述和泛总结，保留全部结果、输入与配置、计时与统计定义；
  正确性实验的操作/批次列表改为指向 6.3 的交叉引用。将原注释中的
  四-worker 资源 baseline 与单核加密 baseline 区分简短写入正文。
- Related Work 和 Conclusion 正文保持本轮压缩前版本。

近似英文正文词数（排除图表、公式、行内数学与 LaTeX 命令，不计摘要）：

| 章节 | 修改前 | 修改后 | 减少 |
|---|---:|---:|---:|
| Introduction | 571 | 461 | 110 |
| Background | 700 | 396 | 304 |
| System Overview | 1093 | 822 | 271 |
| HLS Design of TFHE Encryption | 1189 | 1069 | 120 |
| Storage-Side Ciphertext Production | 1179 | 940 | 239 |
| Experimental Evaluation | 1561 | 1055 | 506 |
| Related Work | 227 | 227 | 0 |
| Conclusion | 108 | 108 | 0 |

正文近似由 6628 词减至 5078 词，减少 1550 词。常规浮动的空间利用
允许在 10 页内保留充分的方法解释，未为减少页数牺牲图表文字尺寸。

排版修正：实验中的 8 个 [H] 改为 [!htbp]，消除压缩后第 7/8 页
固定图表导致的大块段间空白。表 3 的 Speedup 表头与其他表头对齐；
原生 balance 放在参考文献前，重新确认在第一栏执行。未引入强制
分页、逐标题间距或字体修改。实际逐页检查渲染，最终编译无 Overfull、
未定义引用或 balance 警告。43 个 labels 唯一、全部引用可解析、31 个
引用 key 保留；8 幅图、5 张表、9 条公式及实验数字保留。

发现需作者核对的既有实验数据：表 3 的 4096-byte 行为
2197.291 / 411.879 ms、5.33×，但当前可读数据文件
acmart-primary_eva/figures/encrypt-stage-comparison/data.csv 的该行只有
CPU mean=2056.0883976 ms，FPGA mean/SD/speedup 为空。1–2048 的九行
与表中四舍五入值吻合。源码中称 4096 来自另一组 median 记录，但未
找到注释所指的原始 CSV，不能将该口径视为已核实。按压缩任务范围
保留原表值，未据缺失记录改写其统计解释，已向作者单独报告。

原稿备份、计数与编译/渲染核对产物：/tmp/fhestore-compress-20261008；
最终日志 verified-build.log，最终渲染 delivery-page-*.png。未修改旧稿，
未提交或 push。


## 2026-10-08：实验图表的可读性与数据口径修正

按作者要求调整前轮指出的实验图表问题。Figure 1–4 的作者确认版本未修改。
Figure 5 保留双栏、两面板和全部 70 组柱形/误差线，35 个 batch 标签改为
8 pt 粗体，扩大左面板并分层显示 batch/operation；纵轴注明 preparation
相对于 HPU computation 的比值。Figure 6–8 保留单栏，统一 Tinos Bold
8–8.8 pt、CPU 蓝色/FHEStore 橙色与斜纹、0.65 pt 轴线和浅灰横向网格。
所有四图使用源 JSON 重绘，保留均值、sample SD、样本数、operation/condition
映射及 panel(b) 七种 operation means 的 min–max 范围；没有从旧 PDF 柱高
反推数值。新增同名 SVG、可复现脚本、轻量数据快照和来源/QA 记录。

Table 1 Configuration 列改为 raggedright，避免长平台名称撑大词间距；
添加 array 包支持该列格式。Figure 7 caption 中 FHEStore 使用 mbox，
保持品牌完整。Table 3 Speedup 表头继续沿用前轮对齐修正。

精确数值来源（读取师弟工作区，未写入该工作区）：
- Figure 5: /home/yuanzhihao/suda/CipherStore/samples/figures/fig_input_hpu_overlay_data.json
- Figure 6/7: /home/yuanzhihao/suda/CipherStore/samples/figures/fig_encrypt_host_resources.json
- Figure 8: /home/yuanzhihao/suda/logs/application-query-preview_20261006/combined-data.json

核对过程中发现并处理两项既有口径问题：
1. Table 3 旧 4096 行的 2197.291 / 411.879 ms、5.33× 来自
   /home/yuanzhihao/suda/logs/lwe_encrypt_host_ready_sweep_20260923/encrypt_cpu_fpga_summary.csv。
   CPU 为 encrypt_and_pack_ms median，FPGA 为 Host 计时 fpga_execute_ms median，
   与当前 1–2048 的 encryption-only mean / FPGA hardware counter mean 不同。
   同口径 4096 FPGA counter 记录仍缺失。因此移出该行和对应 5.33× 叙述，
   将 6.2 输入范围改为 1–2048；Host resource 实验中有真实同口径数据的
   4096 配置保持不变。原稿行保存在本轮 before.tex 中。
2. Figure 5 源 JSON 和 input-preparation-speedup-preview_20261006/data.json
   均明确 CPU 为历史 physical x86，FHEStore 为 QEMU client；输入和传输
   路径未匹配。6.3 改写实际条件，删除此前 same QEMU 的错误描述；派生
   曲线/右轴和对应正文改称 Latency ratio，保留全部原数值。
   新的 cpu_hpu_compare_qemu_cpu1_20261005 与 CSD 1024 输入 hash 匹配，
   但只有 ADD/AND/MUL、且没有 SSD-read 计时，不能直接替换完整七种操作
   的 SSD-to-host-memory preparation 比较。

待作者核对：作者确认的摘要、引言及结论中 2.45× end-to-end speedup
暂未修改。本轮已异步询问是否有新的同条件测量；当前可读原始数据支持
独立路径的 latency ratio，不能据此声称上述条件已统一。

最终主稿正文在第 10 页结束，PDF 共 11 页；8 幅图、5 张表、9 条公式、
43 个唯一 label 和全部引用保留。实际检查第 7–10 页最终渲染，表头、
标签和图注没有裁切/重叠。末轮 FHEStore_FPGA27.log 无 Overfull、undefined、
rerun 或 balance 警告；参考文献仍有两项既有 Underfull 提示。未改 ACM
字号、页边距或标题间距，未提交或 push。

本轮备份、编译与渲染位于 /tmp/fhestore-figure-fixes-20261008。


## 2026-10-08：按作者确认恢复 Figure 5 的 Speedup 表述

作者明确回复“实则是相同的，不用过于防御性写作”，确认 Figure 5 两条
路径的实际比较条件相同。以此确认取代上一轮依据旧 JSON comparability
备注所作的不同环境判断；上一轮该图相关“待核对”事项已由作者澄清。

Figure 5 的绿色曲线、图例、右轴恢复 Speedup；同步更新图注、Description
和 6.3 的指标定义与收益段，删除 physical-x86/QEMU 差异、separately
recorded、respective environments 的解释。正文说明两条路径使用相同
client environment、TPC-H records 和加密参数，恢复 batch/operand 输入
定义，直接报告 2.45×–2.77× 的 input-preparation speedup 和 1024 下
1211.67→494.50 ms 的结果。与既有摘要、引言、结论中的 2.45× 对齐。

数值源快照保持逐字节不变，未改均值、误差线、样本数和聚合方法。
更新绘图脚本、PDF/SVG/PNG、README 和 QA；新增 author-confirmation.json
区分保留的旧备注和最新解释，避免后续重复采用已被作者纠正的备注。
Table 3 的 4096 统计口径处理保持不变，本轮澄清针对 Figure 5。

重新编译并检查第 8 页的最终图/图注/6.3 段落和第 10 页结尾。正文仍在
第 10 页结束，全文 11 页，无 Overfull、undefined 或 balance 警告。
本轮备份和核对产物：/tmp/fhestore-speedup-restore-20261008。
未提交或 push。


## 2026-10-08：全文语义、数据与排版复核

依据作者要求复核合并稿全部 11 页、对应代码、实验数据与图表。
作者再次确认：2.45× 对应 1 KiB 明文的 input preparation 加速比，
保持摘要、引言和结论的输入量及两个性能结果。6.3 的 B 明确为
plaintext input bytes，第二操作数由这些输入派生；不将构造后的两个
操作数缓冲区总字节数用于改写作者定义的输入量。作者亦确认 Table 2
来自师弟测量、报告不在本地，现有 PDF 为准；全部资源数值保持不变。
上述确认已写入父目录 AGENTS.md，输入量澄清同步保存于
figures/input-hpu-ratio/author-confirmation.json；原数值源快照不改。

实际修改：
- 第 8 页原右栏底部比左栏短约 26.16 pt，原因是连续浮动图和双行
  图注使剩余空间不足容纳正文。Figure 6/7 图注收紧为单行，Figure 7
  自然浮至第 9 页；第 8 页现由正文补满，墨迹底差约 5.65 pt。
  第 9 页图表顺序为 Figure 7、Table 4、Figure 8，无裁切或重叠；
  该浮动图表栏约 18 pt 底部余量属于正常浮动排版，不用负间距追齐。
- 3.1/5.2 修正 Host 取回范围：实际读取对齐后的 output SLM range，
  再按 reported valid ciphertext length 识别序列；并非只读取有效
  密文字节。终止拍和 allocation padding 不计入有效长度，块内
  serialized padding 属于密文表示。代码依据见
  host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
  :298–305,1287–1294，以及选择性完整路径 :2219–2221,2452–2459。
- 5.1 明确 device runtime 为 read DMA 配置入口目的地，算子
  controllers 为后续输出添加下一跳 TDEST；scheduler 属于 device
  runtime。将 reserved terminal route 改为 reserved output route，
  避免与 terminal beat 混淆。依据 hlsacccompute.c:1210,1262 及
  OperatorController.v:768–769,821–823。
- 6.1 的同一 64-byte-padded layout 描述限定于 encryption-stage、
  host-resource 与 preprocessing 实验；6.3 将就绪终点说明为
  HPU input layout 的密文在 Host 内存可用，不展开其内部布局。
- 6.5 标题改为 Application-Directed Input Preparation，与
  SSD 到 Host 内存的测量范围和正文术语一致。
- RLWE 首次给出 ring learning-with-errors，Related Work 首次展开
  BFV/CKKS；后两者全称对照 OpenFHE 官方文档核实：
  https://openfhe-development.readthedocs.io/en/latest/ 。
- Equation 9 末尾逗号改为句号，与后续独立句一致，公式本身不变。
- 原 balance 的末页分盒产生 1.16599 pt Overfull 且两栏差一行；
  提前调用和 ACM pbalance（此处仍回退旧 balance）均未解决。
  最终以标准 flushend 自动平衡末页，acmart balance=false 避免
  输出例程冲突。第 11 页两栏墨迹底均为 263.46183 pt；31 条参考
  文献去除分页、页眉和换行差异后与原 PDF 全文一致。

核验结果：
- 9 条公式的符号、radix 顺序、模加、bundle 容量、输出 extent
  和 speedup 定义一致；Write DMA 先准备、terminal beat 排除、
  discovery/production 两次 SLM 扫描和 batch offset 与代码相符。
- Figure 1 数值、Figure 5 全部 70 个柱形及五个 speedup、Figure 6/7
  各配置均值/样本标准差、Figure 8 六组条件及 Table 3 九行均与
  数值源一致。Table 5 的 104 条结果计数及逐元素校验记录吻合。
  Table 2 百分比算术正确，计数来源按作者确认保留师弟测量结果。
- 8 幅图、5 张表、9 条公式、43 个唯一 labels、31 个 citation keys
  保留，无未定义引用。数字、均值、误差线和样本数未改。
- 最终 PDF 仍为 11 页，Conclusion 和正文在第 10 页结束。
  没有 Overfull、未定义引用或重编译提示。日志保留两项参考文献
  Underfull hbox，以及第 10 页 Underfull vbox 提示；实际页面
  无溢出或裁切，不将这些提示描述为零警告。
- 未改字体、页边距、各级标题定义或原图；未提交或 push。

备份、对比、编译日志、渲染与核验记录位于
/tmp/fhestore-full-audit-20261008。最终编译 build-flushend.log。


## 2026-10-08：补充 Background 2.1 的定义与表示衔接

按作者确认调整 2.1。开头分别用一句话解释 FHE 的功能和 TFHE 的
方案定位，随后介绍编码、密钥材料和原有正确性关系。保留 torus-LWE
密文与 phase 公式，将消息恢复说明收紧为一句。增加两段概念背景：
q-bit 系数表示如何将 torus 加法映射为模 2^q 整数加法，以及 radix
digits 如何分别生成 LWE blocks 并组成有序 ciphertext bundle，
从而解释参数与编码共同决定的密文膨胀。未加入具体原型参数。

所有改动仅在 2.1，其他章节、标题、图表、公式内容、实验数据与
摘要保持逐字节不变。其他章节/小节的标题建议在会话中讨论，
尚未根据建议替换标题。

编译通过，无 Overfull 或未定义引用；正文仍在第 10 页结束，全文
11 页，系统架构图仍在第 3 页顶部。核验原有 9 条公式完全不变、
43 个 labels 唯一和 31 个 citation keys 保留。
备份及编译/渲染核验记录：/tmp/fhestore-background-20261008。
未提交或 push。

本轮实际检查 p2/3 与受重排影响的 p5/6/8–11。Figure 3/4 和
实验图表无裁切、孤立标题或异常大空白；Table 5 自然浮至 p10 左栏
顶部，Conclusion 完整保留在 p10。参考文献末页因新增正文改变
分割位置，flushend 留约 24 pt 正常余量；全部引用内容完整，
未针对这一正常浮动余量修改局部间距或字体。


## 2026-10-08：收紧公式 2、3 的 modulo-one 标记

按作者意见，将两个 \pmod{1} 改为紧凑的
\;(\mathrm{mod}\,1)，减少默认 display-math 模标记的前置间距。
保留括号形式、数学含义、公式编号及其他全部源文。
编译并实际检查第 2 页：两式清晰，无溢出，全文仍 11 页。
核验只有两处数学间距变化；备份及渲染位于
/tmp/fhestore-mod-notation-20261008。


## 2026-10-08：按作者确认调整章节标题

第四章改为 HLS Design of TFHE Encoding and Encryption；5.1 为
Operator Stream Composition；6.2 为 Encryption Operator Performance；
6.3 为 Input Preparation Performance。6.5 保持此前已统一的
Application-Directed Input Preparation，准确涵盖该实验的数据访问、
筛选投影、编码加密、输出组织与 Host 内存就绪。其结果解释中的
smaller ciphertext-generation workload 改为 smaller encryption workload，
具体指选择数量减少带来的加密工作量变化。

2.1 在 TFHE 的 torus 定义中补充 period is normalized to one，
解释式 2/3 模 1 的来源；有限精度段继续说明对应模 2^q 整数运算。
其他正文、公式、图表、标签、摘要和数字均不变。

编译通过，无 Overfull 或未定义引用；系统架构图仍在第三页顶部，
正文在第 10 页结束，PDF 共 11 页。保持 ACM 字号、边距和标题
定义，未添加局部标题间距。备份与渲染核验记录位于
/tmp/fhestore-headings-20261008。未提交或 push。


## 2026-10-08：收紧 Table 4 的行间距

Table 4 的 p 列单元格逐项使用末尾 \par，与 p 列自动追加的
行高 strut 叠加，产生多余空行。改为列定义中的
>{\raggedright\arraybackslash}，去掉逐单元格分组和末尾 \par。
保留原字号、arraystretch、列宽、数据集分组间隔以及 C3/C6 的
两行复合谓词格式。表内字段、条件、阈值、标题和标签均不变，
本轮其他 LaTeX 内容保持逐字节一致。

编译通过并检查第 9 页，表格更紧凑且无溢出。正文仍在第 10 页
结束，PDF 共 11 页。备份与核验记录位于
/tmp/fhestore-table4-spacing-20261008。未提交或 push。
