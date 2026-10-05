# FHEStore 服务器迁移说明

本包由当前工作区生成，日期为 2026-10-04。主文稿入口仍使用历史文件名 `acmart-primary/CipherStore_FPGA27.tex`；系统名称为 FHEStore。

## 选择压缩包

- `FHEStore-template-20261004.zip`：论文 LaTeX、BibTeX 文献库、本地 ACM 类和参考文献样式、类源文件与许可证，以及当前 PDF 预览。
- `FHEStore-workspace-20261004.zip`：以上内容，加上根目录 `AGENTS.md`、完整 `skills/`、作者提供的题目/摘要和叙事文档、三篇本地参考论文，以及现有文献核验和修改说明。适合继续写作。

解压后的根目录均为 `FHEStore/`。选择其中一个压缩包即可。编译中间文件、旧稿备份及 `tmp/` 不在包内。Windows 上的 MiKTeX 程序不随包迁移。

## 传到支持 SSH 的 Linux 服务器

在 Windows PowerShell 中，进入本说明所在的目录，再运行：

```powershell
scp -P 22 .\FHEStore-workspace-20261004.zip USER@SERVER:~/
```

把 `USER`、`SERVER` 和端口 `22` 替换为实际用户名、地址和 SSH 端口。只传模板时，将文件名改成模板 ZIP。身份验证在本机 SSH 客户端中完成。

在服务器终端中，将压缩包解到一个新的目录：

```bash
mkdir -p ~/papers/fhestore-20261004
unzip ~/FHEStore-workspace-20261004.zip -d ~/papers/fhestore-20261004
cd ~/papers/fhestore-20261004/FHEStore/acmart-primary
```

也可以用支持 SFTP 的文件传输工具上传 ZIP，然后执行上述解压命令。

## 编译

目标服务器需要提供 `pdflatex`、`bibtex` 和 ACM 类依赖的宏包及字体。当前类版本为 acmart v2.20（2026-08-16），主稿使用 `\AddToHookNext`，需要支持该命令的 LaTeX 内核。本机已验证的编译日志采用 LaTeX2e 2025-11-01；服务器可使用包完整的较新 TeX Live 或 MiKTeX 环境。

当前主稿没有外部图片或其他 `\input` 文件。保持本地 `acmart.cls`、`ACM-Reference-Format.bst` 与主稿同目录，以使用随包带上的版本。关键字体和依赖包括 Libertine、Inconsolata（`zi4`）、NewTX Math、`hyperxmp` 和 `balance`。

在 `acmart-primary/` 中运行：

```bash
pdflatex -interaction=nonstopmode -halt-on-error CipherStore_FPGA27.tex
bibtex CipherStore_FPGA27
pdflatex -interaction=nonstopmode -halt-on-error CipherStore_FPGA27.tex
pdflatex -interaction=nonstopmode -halt-on-error CipherStore_FPGA27.tex
```

如果已安装 latexmk，也可使用：

```bash
latexmk -pdf CipherStore_FPGA27.tex
```

输出为同目录下的 `CipherStore_FPGA27.pdf`。包内已有一份本机编译的 PDF，可用于对照。服务器端宏包、字体或 TeX 引擎版本不同可能影响分页。

## 文件完整性与继续写作

每个 ZIP 内均附 `TRANSFER_MANIFEST.json`，列出源文件及 SHA-256。下载后可核对外部 `SHA256SUMS.txt`。

继续写作时，以根目录 `AGENTS.md` 和最新 `.tex` 为依据：题目与摘要采用作者最新确认的 FHEStore 版本；5.33× 和 3.26× 分别对应 FPGA 加密与端到端输入准备。相关证据和待补充项保存在正文及说明文件中。

参考资料：

- [OpenSSH scp 官方手册](https://man.openbsd.org/scp)
- [TeX Live 官方入口](https://tug.org/texlive/)
