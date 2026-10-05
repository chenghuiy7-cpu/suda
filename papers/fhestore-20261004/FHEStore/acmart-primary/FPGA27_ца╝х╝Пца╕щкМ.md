# FPGA ’27 模板格式核验

核验日期：2026-10-03。范围：当前主稿的 ACM 模板选项、首页引用与会议脚注、匿名设置；不评估整篇论文是否达到投稿就绪状态。

## 当前会议的官方要求

依据 [FPGA 2027 官方 CFP](https://www.isfpga.org/call-for-papers/)，Submission Guidelines and Processes → Submission Template，以及 Conference Policies → Double Blind Policy：

- 提交英文 PDF，使用 ACM `sigconf`，LaTeX 按官方模板包的 `sample-sigconf.pdf` 格式。
- Long 最多 10 页、Short 最多 6 页，均不含参考文献。
- 双盲，不得标识作者或单位；自引用第三人称，资助标识和随附代码仓库也须匿名。
- CFP 没有另行规定 `review` 行号开关、字体大小覆盖值、隐藏 ACM Reference Format 或清空首页版权脚注。当前稿使用 `sigconf` 的默认 9pt 与默认版心，不自行覆盖字体、页边距或行距。

当前 [Instructions to Authors](https://www.isfpga.org/instructions-to-authors/) 仍链接 `fpga26.hotcrp.com` 与 `fpga2026.htm`，内容是 FPGA 2026 的 camera-ready 说明，不能用来替代 FPGA 2027 的投稿要求。网页抽取快照保存在 `../tmp/pdfs/fpga27-format/official-sources.txt`。

## 首页差异的原因和修正

此前主稿主动设置 `printacmref=false`，并重定义 `\footnotetextcopyrightpermission` 为空，分别隐藏了 ACM Reference Format 和横线下方的会议/出版区域。这不是 FPGA 2027 CFP 指定的修改。

本次恢复 `printacmref=true`，删除空脚注重定义，由 `acmart` 排版首页引用和会议脚注。`sigconf,anonymous`、页码与 CCS 保留，正文及文献库不修改。

本地 `acmart.cls` 为 2.20（2026-08-16），未改动；其默认设置与生成逻辑见：

- 第 273–274 行：`sigconf` 默认 9pt。
- 第 1787–1812 行：`printacmref` 开关、默认开启设置，以及多页文稿关闭时的警告。
- 第 2235–2294 行：首页会议/版权脚注按版权模式生成。
- 配套 `samples/samples.dtx` 的 Rights management information：版权命令及出版元数据须以完成 rights form 后收到的实际值替换示例。

## 匿名投稿稿与正式出版稿

作者提供的 HERA PDF 是带真实作者、DOI、ISBN 和论文集起始页码 265 的正式出版稿。它首页的许可文字涉及美国政府共同作者，是该论文的具体授权情形，不应复制到 FHEStore。

当前 FHEStore 是匿名草稿。保留 `\setcopyright{none}`，会议名称、年份、时间和地点采用已公开的 FPGA 2027 信息；用 `\acmDOI{}` 和 `\acmISBN{}` 清除类文件的示例值。因此恢复后的横线下方显示会议信息和年份，不生成未经确认的许可声明或 DOI/ISBN。

录用后，以 [ACM LaTeX Best Practices](https://authors.acm.org/binaries/content/assets/publications/taps/latex-best_practices-06-may-2020.pdf) 的 eRights 流程及届时 FPGA 2027 的 camera-ready 通知为准，填写作者、许可、DOI、ISBN、论文集名称与正式出版元数据。当前核验不把 FPGA 2026 出版稿或模板示例值当作 FPGA 2027 已分配的信息。

## 核验边界

本次结论是恢复指定模板的默认首页结构并保留双盲设置，不声称 FPGA 2027 CFP 明文要求匿名稿使用某一种版权许可。正式出版元数据尚未分配，不能预先确认。后续若会议或投稿系统发布更具体指令，应重新核对。

修改前主稿及 PDF 备份保存在 `../tmp/pdfs/fpga27-format/`。编译、最终页数、引用解析与逐页排版检查结果见该目录的 `verification.json`。

最终两轮 `pdflatex` 编译成功；PDF 共 5 页，US Letter（612 × 792 PDF points）。已逐页检查：首页引用、会议脚注、匿名作者及后续双栏布局正常，无裁切或重叠。最后一轮日志无 Overfull、未定义引用或标签重编译提示，此前 ACM Reference Format 关闭警告已消失。源码对比确认从题目起的全部内容保持不变，参考文献库与类文件未改动。源文件、PDF、模板和官方网页抽取快照的 SHA-256 记录在 `verification.json` 中。
