#!/usr/bin/env python3
"""Verify fixed FPGA records against their source dbgen lineitem rows."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

import convert_lineitem as converter


def verify(source_path: Path, binary_path: Path, records: int, skip_rows: int) -> list[int]:
    image = binary_path.read_bytes()
    expected_bytes = records * converter.RECORD_BYTES
    if len(image) != expected_bytes:
        raise ValueError(f"binary has {len(image)} bytes; expected {expected_bytes}")

    selected: list[int] = []
    checked = 0
    with source_path.open("r", encoding="ascii", newline="") as source:
        for source_row, line in enumerate(source, start=1):
            if source_row <= skip_rows:
                continue
            if checked == records:
                break
            item = converter.parse_lineitem(line)
            record = image[
                checked * converter.RECORD_BYTES : (checked + 1) * converter.RECORD_BYTES
            ]
            actual = {
                "orderkey": struct.unpack_from("<Q", record, 0)[0],
                "partkey": struct.unpack_from("<Q", record, 8)[0],
                "suppkey": struct.unpack_from("<Q", record, 16)[0],
                "linenumber": record[24],
                "quantity": record[25],
                "returnflag": chr(record[26]),
                "linestatus": chr(record[27]),
                "shipmode_code": record[28],
                "shipinstruct_code": record[29],
                "extendedprice_cents": struct.unpack_from("<Q", record, 32)[0],
                "discount_bp": struct.unpack_from("<H", record, 40)[0],
                "tax_bp": struct.unpack_from("<H", record, 42)[0],
                "shipdate": struct.unpack_from("<I", record, 44)[0],
                "commitdate": struct.unpack_from("<I", record, 48)[0],
                "receiptdate": struct.unpack_from("<I", record, 52)[0],
                "source_row": struct.unpack_from("<Q", record, 56)[0],
            }
            expected = {
                "orderkey": item.orderkey,
                "partkey": item.partkey,
                "suppkey": item.suppkey,
                "linenumber": item.linenumber,
                "quantity": item.quantity,
                "returnflag": item.returnflag,
                "linestatus": item.linestatus,
                "shipmode_code": converter.SHIPMODE_CODES[item.shipmode],
                "shipinstruct_code": converter.SHIPINSTRUCT_CODES[item.shipinstruct],
                "extendedprice_cents": item.extendedprice_cents,
                "discount_bp": item.discount_bp,
                "tax_bp": item.tax_bp,
                "shipdate": item.shipdate,
                "commitdate": item.commitdate,
                "receiptdate": item.receiptdate,
                "source_row": source_row,
            }
            if actual != expected:
                raise ValueError(
                    f"record {checked} differs from source row {source_row}: "
                    f"actual={actual} expected={expected}"
                )

            raw_length = struct.unpack_from("<H", record, 160)[0]
            raw = record[
                converter.RAW_LINE_OFFSET : converter.RAW_LINE_OFFSET + raw_length
            ].decode("ascii")
            if raw != item.raw_line:
                raise ValueError(f"raw text mismatch at source row {source_row}")

            if (
                19940101 <= actual["shipdate"] < 19950101
                and 500 <= actual["discount_bp"] <= 700
                and actual["quantity"] < 24
            ):
                selected.append(actual["quantity"])
            checked += 1

    if checked != records:
        raise ValueError(f"source ended after {checked} checked rows; expected {records}")
    return selected


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--records", type=int, default=128)
    parser.add_argument("--skip-rows", type=int, default=0)
    args = parser.parse_args()
    try:
        selected = verify(args.input, args.binary, args.records, args.skip_rows)
    except (OSError, UnicodeError, ValueError) as exc:
        parser.error(str(exc))
    print("TPC-H lineitem binary verification passed")
    print(f"records_checked={args.records}")
    print(f"selected_count={len(selected)}")
    print(f"selected_quantities={','.join(str(value) for value in selected)}")
    print(f"selected_prefix={bytes(selected).hex()}")
    print(f"binary_sha256={hashlib.sha256(args.binary.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()

