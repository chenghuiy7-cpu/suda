#!/usr/bin/env python3
"""Versioned storage/filter/encryption tasks; transport policy is separate.

No SSD writes or device IO occur during validation/dry-run. Execution uses
bounded batches, device-derived selection and the native FPGA pipeline.
"""
import argparse
import hashlib
import json
import math
import re
import subprocess
import sys
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
MAX_REQUEST = 128 * 1024 * 1024
LBA_BYTES = 4096
TYPES = {"u8": 1, "u16": 2, "u32": 4, "u64": 8,
         "i8": 1, "i16": 2, "i32": 4, "i64": 8}


def obj(value, allowed, required, name):
    if not isinstance(value, dict):
        raise ValueError(f"{name} must be an object")
    unknown = set(value) - set(allowed)
    missing = set(required) - set(value)
    if unknown or missing:
        raise ValueError(f"{name}: unknown keys {sorted(unknown)}, missing keys {sorted(missing)}")
    return value


def integer(value, minimum, maximum, name):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{name} must be an integer in [{minimum}, {maximum}]")
    return value


def text(value, name):
    if not isinstance(value, str) or not value or '\0' in value:
        raise ValueError(f"{name} must be a nonempty string")
    return value


def path_ref(value, base, name):
    p = Path(text(value, name)).expanduser()
    return str(p if p.is_absolute() else (base / p).resolve())


def load_json(path):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=pairs)


def plan(task, runtime, task_base=HERE, runtime_base=HERE):
    obj(task, ("version", "data", "query", "encryption", "downstream", "validation"),
        ("version", "data", "query", "encryption", "downstream"), "task")
    integer(task["version"], 1, 1, "task.version")
    data = obj(task["data"], ("nsid", "lba", "records", "record_bytes", "schema"),
               ("nsid", "lba", "records", "record_bytes", "schema"), "data")
    nsid = integer(data["nsid"], 1, 2**32 - 1, "data.nsid")
    lba = integer(data["lba"], 0, 2**64 - 1, "data.lba")
    records = integer(data["records"], 1, 2**32 - 1, "data.records")
    stride = integer(data["record_bytes"], 64, 4096, "data.record_bytes")
    if stride % 64:
        raise ValueError("record_bytes must be a multiple of 64")
    if not isinstance(data["schema"], list) or not data["schema"]:
        raise ValueError("data.schema must be a nonempty field array")
    fields, names, occupied = [], set(), set()
    for field in data["schema"]:
        obj(field, ("name", "type", "offset"), ("name", "type", "offset"), "field")
        name = text(field["name"], "field.name")
        kind = text(field["type"], "field.type")
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) or name.lower() in names:
            raise ValueError("invalid or duplicate field name")
        names.add(name.lower())
        if kind not in TYPES:
            raise ValueError(f"unsupported field type: {kind}")
        offset = integer(field["offset"], 0, stride - 1, "field.offset")
        width = TYPES[kind]
        lanes = set(range(offset, offset + width))
        if offset + width > stride or offset % 64 + width > 64 or lanes & occupied:
            raise ValueError("field crosses row/beat boundary or overlaps another field")
        occupied |= lanes
        fields.append(f"{name}:{kind}@{offset}")
    query = text(task["query"], "query")
    enc = obj(task["encryption"], ("preset", "key_ref", "layout"),
              ("preset", "key_ref", "layout"), "encryption")
    if enc["preset"] != "psi64-big-lwe-u8":
        raise ValueError("supported encryption preset is psi64-big-lwe-u8")
    if enc["layout"] not in ("cpu", "hpu-native"):
        raise ValueError("layout must be cpu or hpu-native")
    key = path_ref(enc["key_ref"], task_base, "key_ref")
    downstream = obj(task["downstream"], ("mode", "output_dir", "remote", "ssd_result"),
                     ("mode", "output_dir"), "downstream")
    mode = downstream["mode"]
    if mode not in ("generate", "update"):
        raise ValueError("downstream.mode must be generate or update")
    if mode == "generate" and ("remote" in downstream or "ssd_result" in downstream):
        raise ValueError("generate does not accept remote or SSD update settings")
    output_dir = path_ref(downstream["output_dir"], task_base, "output_dir")
    total_lbas = (records * stride + LBA_BYTES - 1) // LBA_BYTES
    if lba + total_lbas > 2**64 - 1:
        raise ValueError("source LBA range overflows")
    remote_args, destination = [], None
    if mode == "update":
        if enc["layout"] != "hpu-native":
            raise ValueError("update requires hpu-native ciphertexts")
        remote = obj(downstream.get("remote"), ("host", "port", "operation", "scalar"),
                     ("host", "port", "operation", "scalar"), "remote")
        if remote["operation"] not in ("adds", "echo"):
            raise ValueError("remote.operation must be adds or echo")
        remote_args = ["--server", text(remote["host"], "remote.host"), "--server-port",
                       str(integer(remote["port"], 1, 65535, "remote.port")),
                       "--remote-operation", remote["operation"], "--scalar",
                       str(integer(remote["scalar"], 0, 255, "remote.scalar"))]
        dst = obj(downstream.get("ssd_result"), ("nsid", "lba"), ("nsid", "lba"), "ssd_result")
        dst_nsid = integer(dst["nsid"], 1, 2**32 - 1, "ssd_result.nsid")
        dst_lba = integer(dst["lba"], 0, 2**64 - 1, "ssd_result.lba")
        if dst_lba + total_lbas > 2**64 - 1:
            raise ValueError("destination LBA range overflows")
        if dst_nsid == nsid and lba < dst_lba + total_lbas and dst_lba < lba + total_lbas:
            raise ValueError("task interface requires a disjoint copy-on-write destination")
        destination = (dst_nsid, dst_lba)
    validation = obj(task.get("validation", {}), ("reference",), (), "validation")
    reference = path_ref(validation["reference"], task_base, "reference") if "reference" in validation else None
    obj(runtime, ("version", "devices", "batch_records", "transport", "benchmark", "seed", "nonce"),
        ("version",), "runtime")
    integer(runtime["version"], 1, 1, "runtime.version")
    devices = obj(runtime.get("devices", {}), ("admin", "io"), (), "devices")
    transport = obj(runtime.get("transport", {}), ("read_chunk_bytes", "write_chunk_bytes", "read_queue_depth"), (), "transport")
    read = integer(transport.get("read_chunk_bytes", 16 * 1024 * 1024), 4096, MAX_REQUEST, "read_chunk_bytes")
    write = integer(transport.get("write_chunk_bytes", 16 * 1024 * 1024), 4096, MAX_REQUEST, "write_chunk_bytes")
    depth = integer(transport.get("read_queue_depth", 1), 1, 64, "read_queue_depth")
    if depth not in (1, 2, 4):
        raise ValueError("read_queue_depth must be 1, 2 or 4")
    if read % 4096 or write % 4096:
        raise ValueError("transport chunks must be multiples of 4096")
    batch = integer(runtime.get("batch_records", 128), 1, 1024, "batch_records")
    alignment = LBA_BYTES // math.gcd(stride, LBA_BYTES)
    if batch % alignment:
        raise ValueError(f"batch_records must be a multiple of {alignment} for LBA-aligned boundaries")
    benchmark = runtime.get("benchmark", True)
    if type(benchmark) is not bool:
        raise ValueError("benchmark must be boolean")
    common = ["--ssd-nsid", str(nsid), "--record-bytes", str(stride), "--schema", ",".join(fields),
              "--query", query, "--key", key, "--output-layout", enc["layout"],
              "--admin", text(devices.get("admin", "nvmq0"), "devices.admin"),
              "--io", text(devices.get("io", "nvmq0n1"), "devices.io"),
              "--slm-read-chunk-bytes", str(read), "--slm-write-chunk-bytes", str(write),
              "--slm-read-queue-depth", str(depth)]
    if benchmark:
        common += ["--benchmark"]
    for name in ("seed", "nonce"):
        if name in runtime:
            common += [f"--{name}", str(integer(runtime[name], 0, 2**64 - 1, name))]
    if reference:
        common += ["--reference", reference]
    common += ["--encrypt-only"] if mode == "generate" else remote_args
    return dict(task=task, runtime=runtime, common=common, records=records, stride=stride, batch_records=batch,
                source_lba=lba, destination=destination, output_dir=output_dir,
                mode=mode, key=key, reference=reference, layout=enc["layout"])


def batches(p):
    for base in range(0, p["records"], p["batch_records"]):
        count = min(p["batch_records"], p["records"] - base)
        lba_offset = base * p["stride"] // LBA_BYTES
        args = p["common"] + ["--records", str(count), "--record-base", str(base),
                               "--ssd-lba", str(p["source_lba"] + lba_offset)]
        if p["destination"]:
            nsid, lba = p["destination"]
            args += ["--output-ssd-nsid", str(nsid), "--output-ssd-lba", str(lba + lba_offset)]
        yield base, count, args


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--task", type=Path, required=True)
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--binary", type=Path, default=HERE / "vscode-selective-lwe-full-pipeline")
    args = parser.parse_args()
    try:
        task = load_json(args.task)
        runtime = load_json(args.runtime) if args.runtime else {"version": 1}
        p = plan(task, runtime, args.task.resolve().parent,
                 args.runtime.resolve().parent if args.runtime else HERE)
        binary = str(args.binary.resolve())
        # Native SQL compiler checks supported SQL/projection before any SSD IO.
        checked = subprocess.run([binary, *p["common"], "--explain-filter"], check=False)
        if checked.returncode:
            raise ValueError("native query preflight failed")
        if args.dry_run:
            print(json.dumps({"batch_count": math.ceil(p["records"] / p["batch_records"]),
                              "mode": p["mode"], "commands": [[binary, *cmd] for _, _, cmd in batches(p)]}, indent=2))
            return 0
        if not Path(p["key"]).is_file():
            raise ValueError("key_ref does not resolve to a key file")
        if p["reference"] and Path(p["reference"]).stat().st_size < p["records"] * p["stride"]:
            raise ValueError("optional reference is shorter than the task input")
        run_dir = Path(p["output_dir"]) / ("run-" + uuid.uuid4().hex)
        run_dir.mkdir(parents=True, exist_ok=False)
        task_digest = hashlib.sha256(json.dumps(task, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        result = {"task_sha256": task_digest, "task": task, "runtime": runtime, "version": 1, "status": "running", "mode": p["mode"],
                  "selection_source": "fpga", "record_count": p["records"],
                  "layout": p["layout"], "ciphertext_file_format": "LWEHLS01-logical", "selected_count": 0, "batches": []}
        print(f"task_run_dir={run_dir}", flush=True)
        for base, count, cmd in batches(p):
            mapping = run_dir / f"batch-{base:010d}.selection.json"
            cipher = run_dir / f"batch-{base:010d}.lwehls.bin"
            cmd += ["--selection-output", str(mapping)]
            if p["mode"] == "generate":
                cmd += ["--output", str(cipher)]
            print(f"task_batch record_base={base} records={count}", flush=True)
            rc = subprocess.run([binary, *cmd], check=False).returncode
            if rc:
                result["status"] = "failed"
                result["failed_batch"] = {"record_base": base, "exit_code": rc}
                (run_dir / "progress.json").write_text(json.dumps(result, indent=2) + "\n")
                print("Task failed; progress.json lists previously completed batches. No final result is published.", file=sys.stderr)
                return 1
            selection = load_json(mapping)
            if (selection.get("selection_source") != "fpga" or selection.get("record_base") != base
                    or selection.get("record_count") != count):
                raise ValueError("native batch returned mismatched selection metadata")
            ids, values = selection.get("indices"), selection.get("values_u8")
            if (not isinstance(ids, list) or not isinstance(values, list)
                    or selection.get("selected_count") != len(ids) or len(ids) != len(values)
                    or any(type(i) is not int or not base <= i < base + count for i in ids)
                    or ids != sorted(set(ids))
                    or any(type(v) is not int or not 0 <= v <= 255 for v in values)):
                raise ValueError("invalid native selection mapping")
            result["selected_count"] += selection["selected_count"]
            result["batches"].append({"record_base": base, "record_count": count,
                                     "selected_count": selection["selected_count"],
                                     "selection": mapping.name,
                                     **({"ciphertext": cipher.name} if p["mode"] == "generate" else {})})
            (run_dir / "progress.json").write_text(json.dumps(result, indent=2) + "\n")
        result["status"] = "passed"
        temporary = run_dir / "result.json.tmp"
        temporary.write_text(json.dumps(result, indent=2) + "\n")
        temporary.replace(run_dir / "result.json")
        print(f"task_passed selected_count={result['selected_count']} result={run_dir / 'result.json'}")
        return 0
    except (ValueError, OSError, json.JSONDecodeError) as error:
        print(f"Task error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
