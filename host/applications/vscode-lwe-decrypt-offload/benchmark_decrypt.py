#!/usr/bin/env python3
"""Benchmark CPU and FPGA Big-LWE u8 decryption on identical ciphertext bytes."""
import argparse
import csv
import json
import hashlib
import re
import statistics
import struct
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

SIZES = (1, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096)
WORDS_PER_U8 = 4 * (2048 + 1)
BYTES_PER_U8 = WORDS_PER_U8 * 8
HEADER = struct.Struct("<9Q")
CPU_RE = re.compile(r"cpu_benchmark_ms runs=(\d+) median=([\d.]+)")
STAGES_RE = re.compile(r"benchmark_stage_ms (.+)")
PROFILE_RE = re.compile(r"hw_profile available=yes (.+)")


def load_source(path):
    data = path.read_bytes()
    if data[:8] != b"LWEHLS01" or len(data) < 8 + HEADER.size:
        raise ValueError("source must be a LWEHLS01 ciphertext dump")
    version, dimension, count, blocks, width, carry, padding, delta, words = HEADER.unpack_from(data, 8)
    if (version, dimension, blocks, width, carry, padding, delta) != (1, 2048, 4, 2, 2, 1, 59):
        raise ValueError("source has incompatible psi64/u8 parameters")
    if not count or words != count * WORDS_PER_U8:
        raise ValueError("source has inconsistent ciphertext count")
    clear_start = 8 + HEADER.size
    payload_start = clear_start + count * 8
    if len(data) != payload_start + words * 8:
        raise ValueError("source ciphertext length does not match its header")
    expected = bytes(
        struct.unpack_from("<Q", data, clear_start + index * 8)[0]
        for index in range(count)
    )
    return data[payload_start:], expected


def make_fixture(payload, expected, count, input_path, expected_path):
    source_count = len(expected)
    whole, tail = divmod(count, source_count)
    with input_path.open("wb") as dest:
        for _ in range(whole):
            dest.write(payload)
        dest.write(payload[:tail * BYTES_PER_U8])
    expected_path.write_bytes(expected * whole + expected[:tail])


def parse_result(output, cpu_only, require_hw_profile=False):
    match = CPU_RE.search(output)
    if not match:
        raise ValueError("CPU benchmark measurement missing")
    result = {"cpu_decrypt_ms": float(match.group(2))}
    if not cpu_only:
        if "lwe_decrypt FPGA execution passed" not in output or "correctness_checked=yes" not in output:
            raise ValueError("FPGA correctness confirmation missing")
        match = STAGES_RE.search(output)
        if not match:
            raise ValueError("FPGA stage measurements missing")
        for key, value in re.findall(r"([a-z_]+)=([\d.]+)", match.group(1)):
            result[key] = float(value)
        if "fpga_execute" not in result or result["fpga_execute"] <= 0:
            raise ValueError("FPGA execute duration missing")
        result["execute_request_speedup"] = result["cpu_decrypt_ms"] / result["fpga_execute"]
        result["pipeline_speedup"] = result["cpu_decrypt_ms"] / result["fpga_pipeline"]
        profile = PROFILE_RE.search(output)
        if profile:
            values = dict(re.findall(r"([a-z_]+)=([^\s]+)", profile.group(1)))
            if values.get("boundary") != "ap_start_to_finish_valid" or values.get("clock_hz") != "250000000":
                raise ValueError("unsupported hardware profile boundary or clock")
            result["hw_stream_ms"] = float(values["stream_ms"])
            result["hw_input_wait_ms"] = float(values["input_wait_ms"])
            result["hw_output_wait_ms"] = float(values["output_wait_ms"])
            result["hw_stream_cycles"] = int(values["cycles"])
            if result["hw_stream_ms"] <= 0:
                raise ValueError("hardware stream duration must be positive")
            result["hardware_stream_speedup"] = result["cpu_decrypt_ms"] / result["hw_stream_ms"]
        elif require_hw_profile:
            raise ValueError("hardware profile missing; update BOOT.bin and ARM runtime before this sweep")
    return result


def main():
    app_dir = Path(__file__).resolve().parent
    repo = app_dir.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=repo / "host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_128b.bin")
    parser.add_argument("--key", type=Path, default=repo / "device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin")
    parser.add_argument("--app", type=Path, default=app_dir / "vscode-lwe-decrypt-offload")
    parser.add_argument("--output-dir", type=Path, default=app_dir / ("benchmark_decrypt_" + datetime.now().strftime("%Y%m%d_%H%M%S")))
    parser.add_argument("--fixture-dir", type=Path, help="where to hold the temporary ciphertext fixture")
    parser.add_argument("--sizes", type=int, nargs="+", default=SIZES)
    parser.add_argument("--trials", type=int, default=5)
    parser.add_argument("--cpu-runs", type=int, default=5)
    parser.add_argument("--slm-write-chunk-bytes", type=int, default=16 * 1024 * 1024)
    parser.add_argument("--fpga-input-layout", choices=("cpu", "cpu-padded", "hpu-native"), default="cpu")
    parser.add_argument("--require-hw-profile", action="store_true",
                        help="fail rather than label request latency as hardware time")
    parser.add_argument("--timeout", type=int, default=240)
    parser.add_argument("--cpu-only", action="store_true")
    parser.add_argument("--resume", action="store_true", help="continue completed trials in --output-dir")
    args = parser.parse_args()
    if any(size <= 0 for size in args.sizes) or args.trials < 1 or not 1 <= args.cpu_runs <= 1000:
        parser.error("sizes and trials must be positive; cpu-runs must be 1..1000")
    payload, expected = load_source(args.source)
    if args.key.stat().st_size != 2048:
        parser.error("key must contain 2048 binary coefficients")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    log_dir = args.output_dir / "logs"
    log_dir.mkdir(exist_ok=True)
    rows = []
    fields = ["bytes", "trial", "ciphertext_bytes", "cpu_decrypt_ms", "fpga_execute", "host_to_slm",
              "slm_to_host", "fpga_pipeline", "execute_request_speedup", "pipeline_speedup",
              "hw_stream_ms", "hw_stream_cycles", "hw_input_wait_ms", "hw_output_wait_ms",
              "hardware_stream_speedup"]
    results_path = args.output_dir / "results.csv"
    summary_path = args.output_dir / "summary.csv"
    if args.resume and results_path.exists():
        with results_path.open(newline="") as csv_file:
            for saved in csv.DictReader(csv_file):
                row = {
                    "bytes": int(saved["bytes"]),
                    "trial": int(saved["trial"]),
                    "ciphertext_bytes": int(saved["ciphertext_bytes"]),
                }
                if saved.get("operator_speedup") and not saved.get("execute_request_speedup"):
                    saved["execute_request_speedup"] = saved["operator_speedup"]
                for field in fields[3:]:
                    if saved.get(field):
                        row[field] = float(saved[field])
                if row["bytes"] not in args.sizes or not 1 <= row["trial"] <= args.trials:
                    raise ValueError("saved results do not match requested sizes/trial count")
                if row["ciphertext_bytes"] != row["bytes"] * BYTES_PER_U8:
                    raise ValueError("saved ciphertext size does not match compact logical layout")
                if ("fpga_execute" in row) == args.cpu_only:
                    raise ValueError("saved results use a different CPU-only/FPGA mode")
                if args.require_hw_profile and "hw_stream_ms" not in row:
                    raise ValueError("saved results lack required hardware profiling; use a new output directory")
                rows.append(row)
        if len({(row["bytes"], row["trial"]) for row in rows}) != len(rows):
            raise ValueError("saved results contain duplicate trials")
        print(f"resuming {len(rows)} completed trials from {results_path}", flush=True)
    elif results_path.exists():
        raise ValueError("results.csv already exists; use --resume or a new --output-dir")
    definitions = {
        "cpu_decrypt_ms": "warm host reference kernel; key indices precomputed; includes result check",
        "fpga_execute": "synchronous NVMe execute request including ARM/context/SLM-stream DMA/completion",
        "hw_stream_ms": "250MHz ap_start to first sampled finish TVALID, including setup and stream stalls",
        "fpga_pipeline": "SLM creation + input write + program setup + execute + output read; excludes parse/cleanup",
        "pipeline_speedup": "CPU kernel / FPGA pipeline ratio, not an equal-scope end-to-end CPU speedup",
        "stall_counts": "input and output stall intervals may overlap; never subtract their sum as pure compute time",
    }
    config = {"fpga_input_layout": args.fpga_input_layout, "cpu_only": args.cpu_only,
              "require_hw_profile": args.require_hw_profile,
              "slm_write_chunk_bytes": args.slm_write_chunk_bytes, "cpu_runs": args.cpu_runs,
              "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
              "key_sha256": hashlib.sha256(args.key.read_bytes()).hexdigest(),
              "app_sha256": hashlib.sha256(args.app.read_bytes()).hexdigest()}
    metadata_path = args.output_dir / "measurement.json"
    if args.resume and metadata_path.exists():
        previous = json.loads(metadata_path.read_text())
        if previous.get("config") != config:
            raise ValueError("saved benchmark configuration differs; use a new output directory")
    elif args.resume and rows and args.fpga_input_layout != "cpu":
        raise ValueError("legacy results use compact cpu layout; use a new output directory")
    metadata_path.write_text(json.dumps({"config": config, "definitions": definitions}, indent=2) + "\n")
    completed = {(row["bytes"], row["trial"]) for row in rows}
    with tempfile.TemporaryDirectory(dir=args.fixture_dir) as fixture_directory:
        fixture = Path(fixture_directory)
        input_path = fixture / "input.logical"
        expected_path = fixture / "expected.u8"
        for count in args.sizes:
            if all((count, trial) in completed for trial in range(1, args.trials + 1)):
                continue
            make_fixture(payload, expected, count, input_path, expected_path)
            for trial in range(1, args.trials + 1):
                if (count, trial) in completed:
                    continue
                command = [
                    str(args.app.resolve()), "--input", str(input_path),
                    "--input-format", "logical", "--fpga-input-layout", args.fpga_input_layout,
                    "--plaintext-bytes", str(count), "--expect-file", str(expected_path),
                    "--key", str(args.key.resolve()), "--cpu-benchmark-runs", str(args.cpu_runs),
                    "--skip-output", "--benchmark",
                ]
                if args.cpu_only:
                    command.append("--inspect-only")
                else:
                    command += ["--slm-write-chunk-bytes", str(args.slm_write_chunk_bytes)]
                try:
                    run = subprocess.run(command, cwd=app_dir, capture_output=True, text=True,
                                         timeout=args.timeout, check=False)
                    log = run.stdout + run.stderr
                except subprocess.TimeoutExpired as error:
                    log = (error.stdout or b"").decode(errors="replace") + (error.stderr or b"").decode(errors="replace")
                    (log_dir / f"{count}B_trial{trial}.log").write_text(log)
                    raise RuntimeError(f"{count}B trial {trial} timed out; stopping sweep") from error
                (log_dir / f"{count}B_trial{trial}.log").write_text(log)
                if run.returncode:
                    raise RuntimeError(f"{count}B trial {trial} failed (exit={run.returncode}); see {log_dir}")
                result = parse_result(log, args.cpu_only, args.require_hw_profile)
                row = {"bytes": count, "trial": trial, "ciphertext_bytes": count * BYTES_PER_U8, **result}
                rows.append(row)
                with results_path.open("w", newline="") as csv_file:
                    writer = csv.DictWriter(csv_file, fieldnames=fields, extrasaction="ignore")
                    writer.writeheader()
                    writer.writerows(rows)
                print(f"{count}B trial {trial}/{args.trials}: CPU={result['cpu_decrypt_ms']:.3f} ms" +
                      (f", FPGA request={result['fpga_execute']:.3f} ms, request speedup={result['execute_request_speedup']:.3f}x"
                       if not args.cpu_only else "") +
                      (f", HW stream={result['hw_stream_ms']:.6f} ms, stream speedup={result['hardware_stream_speedup']:.3f}x"
                       if "hw_stream_ms" in result else ""), flush=True)
            input_path.unlink()
            expected_path.unlink()

    with summary_path.open("w", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(["bytes", "ciphertext_bytes", "cpu_median_ms", "fpga_execute_median_ms",
                         "host_to_slm_median_ms", "fpga_pipeline_median_ms", "execute_request_speedup",
                         "pipeline_speedup", "hw_stream_median_ms", "hardware_stream_speedup"])
        for count in args.sizes:
            group = [row for row in rows if row["bytes"] == count]
            median = lambda field: statistics.median(row[field] for row in group)
            cpu = median("cpu_decrypt_ms")
            if args.cpu_only:
                writer.writerow([count, count * BYTES_PER_U8, cpu, "", "", "", "", "", "", ""])
            else:
                fpga = median("fpga_execute")
                pipeline = median("fpga_pipeline")
                writer.writerow([count, count * BYTES_PER_U8, cpu, fpga, median("host_to_slm"),
                                 pipeline, cpu / fpga, cpu / pipeline,
                                 median("hw_stream_ms") if all("hw_stream_ms" in row for row in group) else "",
                                 cpu / median("hw_stream_ms") if all("hw_stream_ms" in row for row in group) else ""])
    print(f"results={results_path}\nsummary={summary_path}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f"benchmark failed: {error}", file=sys.stderr)
        sys.exit(1)
