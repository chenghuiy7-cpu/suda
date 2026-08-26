#!/usr/bin/env python3

import argparse
import csv
import math
import statistics
from pathlib import Path


SECTIONS = {
    "client_rpc": "tcp_connect_ms",
    "remote": "remote_stage_ms",
    "encrypt": "benchmark_stage_ms",
    "decrypt": "decrypt_stage_ms",
}

FIELDS = [
    ("ssd_to_slm_ms", "encrypt", "ssd_to_slm"),
    ("encrypt_program_setup_ms", "encrypt", "program_setup"),
    ("encrypt_fpga_execute_ms", "encrypt", "fpga_execute"),
    ("encrypt_slm_to_host_ms", "encrypt", "slm_to_host"),
    ("encrypt_unpack_verify_ms", "encrypt", "fpga_unpack_verify"),
    ("encrypt_csd_cleanup_ms", "encrypt", "csd_cleanup"),
    ("tcp_connect_ms", "client_rpc", "tcp_connect_ms"),
    ("request_send_ms", "client_rpc", "request_send_ms"),
    ("wait_response_header_ms", "client_rpc", "wait_response_header_ms"),
    ("response_receive_ms", "client_rpc", "response_receive_ms"),
    ("rpc_round_trip_ms", "client_rpc", "rpc_round_trip_ms"),
    ("remote_request_receive_ms", "remote", "request_receive"),
    ("remote_validate_ms", "remote", "validate"),
    ("remote_decode_ms", "remote", "decode"),
    ("remote_hpu_prepare_ms", "remote", "hpu_prepare"),
    ("remote_hpu_enqueue_ms", "remote", "hpu_enqueue"),
    ("remote_hpu_wait_sync_ms", "remote", "hpu_wait_sync"),
    ("remote_hpu_output_convert_ms", "remote", "hpu_output_convert"),
    ("remote_result_encode_ms", "remote", "result_encode"),
    ("remote_mem_sanitizer_ms", "remote", "mem_sanitizer"),
    ("remote_process_ms", "remote", "process"),
    ("remote_response_send_ms", "remote", "response_send"),
    ("remote_server_total_ms", "remote", "server_total"),
    ("decrypt_slm_create_ms", "decrypt", "slm_create"),
    ("decrypt_slm_zero_ms", "decrypt", "slm_zero"),
    ("decrypt_host_to_slm_ms", "decrypt", "host_to_slm"),
    ("decrypt_slm_write_verify_ms", "decrypt", "slm_write_verify"),
    ("decrypt_program_setup_ms", "decrypt", "program_setup"),
    ("decrypt_fpga_execute_ms", "decrypt", "fpga_execute"),
    ("decrypt_slm_to_ssd_ms", "decrypt", "slm_to_ssd"),
    ("decrypt_ssd_readback_ms", "decrypt", "ssd_readback"),
    ("decrypt_cleanup_ms", "decrypt", "cleanup"),
    ("online_e2e_ms", "encrypt", "online_e2e"),
    ("one_shot_e2e_ms", "encrypt", "one_shot_e2e"),
    ("process_ms", "encrypt", "process"),
]

TABLES = [
    (
        "本地 CSD、FPGA 加密与输出",
        [
            "ssd_to_slm_ms",
            "encrypt_program_setup_ms",
            "encrypt_fpga_execute_ms",
            "encrypt_slm_to_host_ms",
            "encrypt_unpack_verify_ms",
            "encrypt_csd_cleanup_ms",
        ],
    ),
    (
        "TCP 与远端 HPU",
        [
            "tcp_connect_ms",
            "request_send_ms",
            "wait_response_header_ms",
            "response_receive_ms",
            "rpc_round_trip_ms",
            "remote_request_receive_ms",
            "remote_decode_ms",
            "remote_hpu_prepare_ms",
            "remote_hpu_enqueue_ms",
            "remote_hpu_wait_sync_ms",
            "remote_hpu_output_convert_ms",
            "remote_result_encode_ms",
            "remote_process_ms",
            "remote_response_send_ms",
            "remote_server_total_ms",
        ],
    ),
    (
        "返回后的 FPGA 解密与 SSD",
        [
            "decrypt_slm_create_ms",
            "decrypt_slm_zero_ms",
            "decrypt_host_to_slm_ms",
            "decrypt_program_setup_ms",
            "decrypt_fpga_execute_ms",
            "decrypt_slm_to_ssd_ms",
            "decrypt_ssd_readback_ms",
            "decrypt_cleanup_ms",
        ],
    ),
    (
        "端到端总量",
        ["online_e2e_ms", "one_shot_e2e_ms", "process_ms"],
    ),
]

E2E_SHARE_STAGES = [
    ("SSD->SLM", "ssd_to_slm_ms"),
    ("加密程序配置", "encrypt_program_setup_ms"),
    ("FPGA 加密", "encrypt_fpga_execute_ms"),
    ("加密 output SLM->Host", "encrypt_slm_to_host_ms"),
    ("Host 密文解包校验", "encrypt_unpack_verify_ms"),
    ("加密侧 CSD 清理", "encrypt_csd_cleanup_ms"),
    ("TCP 连接", "tcp_connect_ms"),
    ("远端 RPC（不含 TCP 连接）", "rpc_round_trip_ms"),
    ("解密 SLM 创建", "decrypt_slm_create_ms"),
    ("解密 output SLM 清零", "decrypt_slm_zero_ms"),
    ("Host->decrypt input SLM", "decrypt_host_to_slm_ms"),
    ("解密程序配置", "decrypt_program_setup_ms"),
    ("FPGA 解密", "decrypt_fpga_execute_ms"),
    ("decrypt output SLM->SSD", "decrypt_slm_to_ssd_ms"),
    ("SSD 回读校验", "decrypt_ssd_readback_ms"),
    ("解密侧清理", "decrypt_cleanup_ms"),
]

RPC_SHARE_STAGES = [
    ("远端接收请求", ("remote_request_receive_ms",)),
    ("远端校验与解码", ("remote_validate_ms", "remote_decode_ms")),
    (
        "HPU 准备、入队、等待与输出转换",
        (
            "remote_hpu_prepare_ms",
            "remote_hpu_enqueue_ms",
            "remote_hpu_wait_sync_ms",
            "remote_hpu_output_convert_ms",
        ),
    ),
    (
        "结果编码与内存清理",
        ("remote_result_encode_ms", "remote_mem_sanitizer_ms"),
    ),
    ("远端发送响应", ("remote_response_send_ms",)),
]


def parse_values(line):
    values = {}
    for token in line.strip().split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        try:
            values[key] = float(value)
        except ValueError:
            continue
    return values


def parse_log(path):
    sections = {}
    passed = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line == "lwe full SSD-to-remote-HPU-to-SSD pipeline passed":
            passed = True
        for section, prefix in SECTIONS.items():
            if line.startswith(prefix):
                sections[section] = parse_values(line)
                break

    row = {"run": path.stem, "passed": "yes" if passed else "no"}
    for output_name, section, source_name in FIELDS:
        if section not in sections or source_name not in sections[section]:
            raise ValueError(
                f"{path}: missing {section}.{source_name} benchmark value"
            )
        row[output_name] = sections[section][source_name]
    return row


def percentile(values, fraction):
    ordered = sorted(values)
    index = max(0, math.ceil(len(ordered) * fraction) - 1)
    return ordered[index]


def metric_stats(rows, field):
    values = [float(row[field]) for row in rows]
    return (
        statistics.mean(values),
        statistics.median(values),
        percentile(values, 0.95),
        min(values),
        max(values),
    )


def print_table(title, rows, fields):
    print(f"\n## {title}\n")
    print("| 阶段 | 平均(ms) | 中位(ms) | P95(ms) | 最小(ms) | 最大(ms) |")
    print("|---|---:|---:|---:|---:|---:|")
    for field in fields:
        mean, median, p95, minimum, maximum = metric_stats(rows, field)
        print(
            f"| `{field}` | {mean:.3f} | {median:.3f} | {p95:.3f} | "
            f"{minimum:.3f} | {maximum:.3f} |"
        )


def sum_field(rows, field):
    return sum(float(row[field]) for row in rows)


def print_share_tables(rows):
    e2e_total = sum_field(rows, "online_e2e_ms")
    accounted = 0.0

    print("\n## 在线端到端阶段占比\n")
    print("占比按 20 次累计阶段时间 / 20 次累计 `online_e2e_ms` 计算。\n")
    print("| 阶段 | 平均延迟(ms) | 在线端到端占比 |")
    print("|---|---:|---:|")
    for label, field in E2E_SHARE_STAGES:
        value = sum_field(rows, field)
        accounted += value
        print(f"| {label} | {value / len(rows):.3f} | {value / e2e_total * 100:.3f}% |")
    residual = e2e_total - accounted
    print(
        f"| 其他计时边界间隙 | {residual / len(rows):.3f} | "
        f"{residual / e2e_total * 100:.3f}% |"
    )
    print(f"| **合计** | **{e2e_total / len(rows):.3f}** | **100.000%** |")

    rpc_total = sum_field(rows, "rpc_round_trip_ms")
    rpc_accounted = 0.0
    print("\n## RPC 内部占比\n")
    print("该表是上一张表中 RPC 阶段的内部拆分，不能再次与 RPC 整体相加。\n")
    print("| RPC 子阶段 | 平均延迟(ms) | RPC 内占比 | 在线端到端占比 |")
    print("|---|---:|---:|---:|")
    for label, fields in RPC_SHARE_STAGES:
        value = sum(sum_field(rows, field) for field in fields)
        rpc_accounted += value
        print(
            f"| {label} | {value / len(rows):.3f} | "
            f"{value / rpc_total * 100:.3f}% | {value / e2e_total * 100:.3f}% |"
        )
    residual = rpc_total - rpc_accounted
    print(
        f"| RPC 计时边界差值 | {residual / len(rows):.3f} | "
        f"{residual / rpc_total * 100:.3f}% | {residual / e2e_total * 100:.3f}% |"
    )
    print(
        f"| **RPC 合计** | **{rpc_total / len(rows):.3f}** | "
        f"**100.000%** | **{rpc_total / e2e_total * 100:.3f}%** |"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Aggregate full NEST pipeline run_*.log benchmark data."
    )
    parser.add_argument("log_dir", type=Path)
    parser.add_argument("--csv", type=Path, required=True)
    args = parser.parse_args()

    paths = sorted(args.log_dir.glob("run_*.log"))
    if not paths:
        raise SystemExit(f"no run_*.log files under {args.log_dir}")

    try:
        rows = [parse_log(path) for path in paths]
    except ValueError as error:
        raise SystemExit(str(error))

    fieldnames = ["run", "passed"] + [field[0] for field in FIELDS]
    with args.csv.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    passed = sum(row["passed"] == "yes" for row in rows)
    print("# 完整端到端 20 次阶段统计")
    print(f"\nsamples={len(rows)} passed={passed} failed={len(rows) - passed}")
    print(f"\n逐次原始数据：`{args.csv}`")
    for title, fields in TABLES:
        print_table(title, rows, fields)
    print_share_tables(rows)

    print("\n## 口径说明\n")
    print("- `online_e2e_ms`：从 SSD->SLM 开始，到目标 SSD 写入并回读校验完成。")
    print("- `one_shot_e2e_ms`：在 online 口径上增加最初的 encrypt SLM 创建。")
    print("- `rpc_round_trip_ms` 已包含请求发送、远端处理和响应接收，不能与其子阶段重复相加。")
    print("- `remote_server_total_ms` 已包含远端 request receive、HPU、编码和 response send。")
    print("- `wait_response_header_ms` 与远端接收/处理并行重叠，不是独立可加阶段。")


if __name__ == "__main__":
    main()
