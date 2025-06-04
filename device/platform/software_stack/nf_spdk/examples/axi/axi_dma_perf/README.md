# MCDMA DPDK/SPDK driver

The real driver, i.e., interaction with MCDMA hardware is in DPDK (rte_axi_dma.c).
The driver was tested under SPDK because I originally intented to use MCDMA as an NVMe-oF transport.

Steps to use the MCDMA driver:

1. Allocate hugepages:
```
$SPDK_ROOT/scripts/setup.sh
```

2. Bind MCDMA devices to vfio-platform driver, so our DPDK driver can accesses them via VFIO.
```
$SPDK_ROOT/scripts/axi_setup.sh
```

3. Run your application.