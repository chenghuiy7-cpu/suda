# SUDA(Computational **S**torage with **U**nified Programming Model **D**evice **A**rchetype)
![SUDAlogo](docs/images/sudalogo.png)
[中文](README.md) | [English](README_EN.md)

## 什么是SUDA？
SUDA是基于具有通用处理器核的片上系统（SoC）以及可编程逻辑门阵列（FPGA）的，具有统一编程模型（流式编程模型+内存搬运的编程模型）的可计算存储设备架构。

## 可计算存储是什么？

可计算存储设备（Computational Storage Device, CSD）是一类将算力设备（如CPU或FPGA）部署在存储设备内部或者附近的新型设备。
在传统的计算机系统中，数据需要从存储盘中读取到主机处理器进行计算，存储盘与主机的带宽限制了数据处理的性能（具体体现在数据吞吐率，处理时间），而主机处理器长期被存储有关的任务占据，为其他任务执行带来了更高延迟。
当前云和数据中心需要处理的数据量呈指数级增加，需要更强的处理性能和更高的数据吞吐，可计算存储设备将和存储有关的任务卸载到与存储盘更近、带宽更高的算力资源上，这些算力资源计算完成后再将结果返回给主机处理器。由于这些算力资源和存储盘之间的带宽相比主机-存储要更高，因此在算力资源性能足够的情况下，能够提供更高的数据吞吐，主机处理器也从繁重的IO任务中解放，能够执行其他的计算任务。

![csdshow](docs/images/oldarchvscsdarch.png)



## SUDA解决什么问题？

针对传统单一算力CSD的固有局限，新型SoC-FPGA混合架构实现了异构计算优势互补：包含通用处理器核的SoC负责灵活的任务调度，FPGA则专注高性能计算。然而该架构的CSD在实际使用中还面临两个层面的挑战：运行层面，FPGA在CSD上缺乏支撑抢占式调度的上下文切换机制，其使用的传统先进先出策略难以满足日益增长的实时数据处理需求；开发层面，虽然SoC在调度之外仍有计算余力，但其与 FPGA 的协同开发复杂度高，限制了SoC的计算潜力。

SUDA解决SoC-FPGA CSD在这两个层面存在的问题。
具体关键技术包括：
- 轻量级 FPGA 任务切换机制。SUDA的基于流式FPGA的上下文任务切换机制能够有效减少上下文开销时间到与软件上下文切换处于同一个数量级，为未来的各种调度算法实现提供了基础基础。
- 异构算力统一开发工具链。SUDA基于高层次综合的异构算力统一开发工具链，消除异构平台开发冗余。


## SUDA的结构

SUDA包括四个层级：主机上的应用、主机上的SUDA运行时、板卡上的控制平面软件栈、以及板卡上的SoC和FPGA的算子池。


主机上的应用程序可以使用SNIA API或者NVMe API（非NVMe标准定义，但是往驱动发送的是符合NVMe CS标准的NVMe命令）向SoC-FPGA CSD请求配置/调用算子，请求会经过libnvme/liburing传递到SUDA驱动，SUDA驱动会将请求处理为符合NVMe标准的命令传输到SoC-FPGA CSD，SoC的控制平面会处理请求，并为和计算有关的请求创建任务上下文进行调度，调度器控制轻量级FPGA任务切换机制，对已经在FPGA算子池上部署的算子进行控制，或者将SoC的算子分配到固定的计算线程进行执行。

算子使用高层次综合开发，算子代码经过统一开发工具链，可以编译为硬件算子（Verilog实现）或者是软件算子（动态链接库）。

![sudaarch](docs/images/SUDAArch.png)

## 项目结构


SUDA项目包含主机和SoC-FPGA CSD两个部分。

device文件夹包含了SoC-FPGA CSD需要的依赖，operators文件夹需要在主机上完成编辑，将算子编译后，拷贝到platforms/basic_shell下的资源文件夹中，生成为比特流，或者是通过API以动态链接库的方式动态拷贝到SoC-FPGA CSD。platform文件夹中，basic_shell帮助生成FPGA比特流，ips为basic_shell需要的ip核，software_stack包含SoC的软件栈代码，需要拷贝到SoC上运行。

host文件夹包含了主机需要的全部依赖，SUDA当前的实现要求用户运行qemu文件夹下的虚拟机，在虚拟机中编译并执行调用SoC-FPGA CSD的应用程序（在Applications文件夹下）。

```c++
device #SoC-FPGA需要的依赖
│   ├── operators #算子设计
│   │   ├── hls
│   │   ├── Makefile
│   │   └── scripts
│   ├── platform #底层硬件和软件
│   │   ├── basic_shell
│   │   ├── ips #底层硬件需要的特殊ip核
│   │   └── software_stack
│   └── shared_components #operators和platform都需要的依赖
│       └── hls
├── docs #文档
│   ├── api
│   ├── architecture
│   ├── images
│   │   ├── oldarchvscsdarch.png
│   │   ├── SUDAArch.png
│   │   └── sudalogo.png
│   └── user-guides
├── host #主机需要的依赖
│   ├── api
│   │   ├── libnvme
│   │   ├── liburing
│   │   └── Makefile
│   ├── applications #可用应用程序
│   │   ├── Makefile
│   │   ├── vscode-blowfish-offload
│   │   ├── vscode-grep-sw
│   │   └── vscode-slmcopy-test
│   ├── drivers #主机SUDA驱动
│   │   ├── Makefile
│   │   ├── nvmq
│   │   └── qdma
│   ├── kernel #主机内核
│   │   ├── 0001-Add-QDMA-header-files.patch
│   │   ├── 0002-Export-bio_map_user_iov.patch
│   │   └── source
│   ├── Makefile
│   ├── mk
│   │   └── common.mk
│   └── qemu #虚拟机资源
│       ├── bind_vfio.sh
│       ├── bzImage
│       ├── debian_qdma_dev.img
│       ├── init.sh
│       ├── qemu
│       └── run_qemu.sh
├── LICENSE
├── README_EN.md
└── README.md
```

## 部署方式

如下为SUDA原型经过验证的实验环境：
