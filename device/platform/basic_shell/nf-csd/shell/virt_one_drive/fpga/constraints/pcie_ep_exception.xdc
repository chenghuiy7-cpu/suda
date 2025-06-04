# ZynqMP PL CLK0 vs. PCIe EP AXI user clk
set_clock_groups -name async_pl_clk1_pcie_ep_user -asynchronous \
    -group [get_clocks clk_pl_1] \
    -group [get_clocks -of_objects [get_pins -hierarchical *bufg_gt_userclk/O*]]

