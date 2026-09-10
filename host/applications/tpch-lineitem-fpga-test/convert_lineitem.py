#!/usr/bin/env python3
"""Convert dbgen lineitem.tbl rows to fixed 512-byte FPGA records."""

from __future__ import annotations

import argparse
import csv
import hashlib
import struct
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
from pathlib import Path


RECORD_BYTES = 512
RAW_LINE_OFFSET = 164
RAW_LINE_CAPACITY = RECORD_BYTES - RAW_LINE_OFFSET

SCHEMA = (
    "orderkey:u64@0,partkey:u64@8,suppkey:u64@16,linenumber:u8@24,"
    "quantity:u8@25,returnflag:u8@26,linestatus:u8@27,"
    "shipmode_code:u8@28,shipinstruct_code:u8@29,"
    "extendedprice_cents:u64@32,discount_bp:u16@40,tax_bp:u16@42,"
    "shipdate:u32@44,commitdate:u32@48,receiptdate:u32@52,source_row:u64@56"
)

# This is the WHERE clause from TPC-H Q6. The FPGA projects quantity because
# the downstream scalar-stream LWE interface currently accepts one u8 value.
Q6_FILTER_QUERY = (
    "SELECT quantity FROM lineitem WHERE "
    "shipdate >= 19940101 AND shipdate < 19950101 AND "
    "discount_bp >= 500 AND discount_bp <= 700 AND quantity < 24"
)

SHIPMODE_CODES = {
    "AIR": 1,
    "FOB": 2,
    "MAIL": 3,
    "RAIL": 4,
    "REG AIR": 5,
    "SHIP": 6,
    "TRUCK": 7,
}

SHIPINSTRUCT_CODES = {
    "COLLECT COD": 1,
    "DELIVER IN PERSON": 2,
    "NONE": 3,
    "TAKE BACK RETURN": 4,
}


@dataclass(frozen=True)
class Lineitem:
    orderkey: int
    partkey: int
    suppkey: int
    linenumber: int
    quantity: int
    extendedprice_cents: int
    discount_bp: int
    tax_bp: int
    returnflag: str
    linestatus: str
    shipdate: int
    commitdate: int
    receiptdate: int
    shipinstruct: str
    shipmode: str
    comment: str
    raw_line: str


def parse_uint(text: str, bits: int, name: str) -> int:
    try:
        value = int(text, 10)
    except ValueError as exc:
        raise ValueError(f"invalid {name}: {text!r}") from exc
    if not 0 <= value < (1 << bits):
        raise ValueError(f"{name} does not fit u{bits}: {value}")
    return value


def parse_scaled_decimal(text: str, scale: int, bits: int, name: str) -> int:
    try:
        scaled = Decimal(text) * scale
    except InvalidOperation as exc:
        raise ValueError(f"invalid {name}: {text!r}") from exc
    integral = scaled.to_integral_value()
    if scaled != integral:
        raise ValueError(f"{name} has more precision than scale {scale}: {text!r}")
    value = int(integral)
    if not 0 <= value < (1 << bits):
        raise ValueError(f"scaled {name} does not fit u{bits}: {value}")
    return value


def parse_quantity(text: str) -> int:
    try:
        quantity = Decimal(text)
    except InvalidOperation as exc:
        raise ValueError(f"invalid quantity: {text!r}") from exc
    if quantity != quantity.to_integral_value():
        raise ValueError(f"fractional TPC-H quantity is unsupported: {text!r}")
    value = int(quantity)
    if not 0 <= value <= 255:
        raise ValueError(f"quantity does not fit u8: {value}")
    return value


def parse_date(text: str, name: str) -> int:
    parts = text.split("-")
    if len(parts) != 3 or any(not part.isdigit() for part in parts):
        raise ValueError(f"invalid {name}: {text!r}")
    year, month, day = (int(part) for part in parts)
    if not (1 <= month <= 12 and 1 <= day <= 31):
        raise ValueError(f"invalid {name}: {text!r}")
    return year * 10000 + month * 100 + day


def parse_lineitem(line: str) -> Lineitem:
    raw_line = line.rstrip("\r\n")
    columns = raw_line.split("|")
    if columns and columns[-1] == "":
        columns.pop()
    if len(columns) != 16:
        raise ValueError(f"lineitem row has {len(columns)} columns, expected 16")

    returnflag, linestatus = columns[8], columns[9]
    if len(returnflag.encode("ascii")) != 1 or len(linestatus.encode("ascii")) != 1:
        raise ValueError("returnflag and linestatus must each be one ASCII byte")
    if columns[13] not in SHIPINSTRUCT_CODES:
        raise ValueError(f"unknown shipinstruct: {columns[13]!r}")
    if columns[14] not in SHIPMODE_CODES:
        raise ValueError(f"unknown shipmode: {columns[14]!r}")

    return Lineitem(
        orderkey=parse_uint(columns[0], 64, "orderkey"),
        partkey=parse_uint(columns[1], 64, "partkey"),
        suppkey=parse_uint(columns[2], 64, "suppkey"),
        linenumber=parse_uint(columns[3], 8, "linenumber"),
        quantity=parse_quantity(columns[4]),
        extendedprice_cents=parse_scaled_decimal(columns[5], 100, 64, "extendedprice"),
        discount_bp=parse_scaled_decimal(columns[6], 10000, 16, "discount"),
        tax_bp=parse_scaled_decimal(columns[7], 10000, 16, "tax"),
        returnflag=returnflag,
        linestatus=linestatus,
        shipdate=parse_date(columns[10], "shipdate"),
        commitdate=parse_date(columns[11], "commitdate"),
        receiptdate=parse_date(columns[12], "receiptdate"),
        shipinstruct=columns[13],
        shipmode=columns[14],
        comment=columns[15],
        raw_line=raw_line,
    )


def put_ascii(record: bytearray, offset: int, capacity: int, value: str, name: str) -> None:
    encoded = value.encode("ascii")
    if len(encoded) > capacity:
        raise ValueError(f"{name} exceeds {capacity} bytes")
    record[offset : offset + len(encoded)] = encoded


def encode_record(item: Lineitem, source_row: int) -> bytes:
    record = bytearray(RECORD_BYTES)
    struct.pack_into("<Q", record, 0, item.orderkey)
    struct.pack_into("<Q", record, 8, item.partkey)
    struct.pack_into("<Q", record, 16, item.suppkey)
    struct.pack_into("<B", record, 24, item.linenumber)
    struct.pack_into("<B", record, 25, item.quantity)
    struct.pack_into("<B", record, 26, ord(item.returnflag))
    struct.pack_into("<B", record, 27, ord(item.linestatus))
    struct.pack_into("<B", record, 28, SHIPMODE_CODES[item.shipmode])
    struct.pack_into("<B", record, 29, SHIPINSTRUCT_CODES[item.shipinstruct])
    struct.pack_into("<Q", record, 32, item.extendedprice_cents)
    struct.pack_into("<H", record, 40, item.discount_bp)
    struct.pack_into("<H", record, 42, item.tax_bp)
    struct.pack_into("<I", record, 44, item.shipdate)
    struct.pack_into("<I", record, 48, item.commitdate)
    struct.pack_into("<I", record, 52, item.receiptdate)
    struct.pack_into("<Q", record, 56, source_row)

    put_ascii(record, 64, 32, item.shipinstruct, "shipinstruct")
    put_ascii(record, 96, 16, item.shipmode, "shipmode")
    put_ascii(record, 112, 48, item.comment, "comment")
    raw = item.raw_line.encode("ascii")
    if len(raw) > RAW_LINE_CAPACITY:
        raise ValueError(f"raw line exceeds {RAW_LINE_CAPACITY} bytes")
    struct.pack_into("<H", record, 160, len(raw))
    record[RAW_LINE_OFFSET : RAW_LINE_OFFSET + len(raw)] = raw
    return bytes(record)


def q6_selected(item: Lineitem) -> bool:
    return (
        19940101 <= item.shipdate < 19950101
        and 500 <= item.discount_bp <= 700
        and item.quantity < 24
    )


def convert(input_path: Path, output_path: Path, manifest_path: Path,
            record_limit: int, skip_rows: int) -> tuple[int, list[int]]:
    if record_limit <= 0:
        raise ValueError("records must be positive")
    if skip_rows < 0:
        raise ValueError("skip-rows must be nonnegative")

    image = bytearray()
    manifest_rows: list[list[object]] = []
    selected_quantities: list[int] = []
    converted = 0
    with input_path.open("r", encoding="ascii", newline="") as source:
        for source_row, line in enumerate(source, start=1):
            if source_row <= skip_rows:
                continue
            item = parse_lineitem(line)
            selected = q6_selected(item)
            image.extend(encode_record(item, source_row))
            manifest_rows.append([
                converted,
                source_row,
                item.orderkey,
                item.partkey,
                item.suppkey,
                item.linenumber,
                item.quantity,
                item.extendedprice_cents,
                item.discount_bp,
                item.tax_bp,
                item.returnflag,
                item.linestatus,
                item.shipdate,
                item.commitdate,
                item.receiptdate,
                item.shipinstruct,
                item.shipmode,
                item.comment,
                int(selected),
            ])
            if selected:
                selected_quantities.append(item.quantity)
            converted += 1
            if converted == record_limit:
                break

    if converted != record_limit:
        raise ValueError(
            f"lineitem input ended after {converted} converted rows; requested {record_limit}"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(image)
    with manifest_path.open("w", encoding="utf-8", newline="") as manifest:
        writer = csv.writer(manifest)
        writer.writerow([
            "record_index", "source_row", "orderkey", "partkey", "suppkey",
            "linenumber", "quantity", "extendedprice_cents", "discount_bp",
            "tax_bp", "returnflag", "linestatus", "shipdate", "commitdate",
            "receiptdate", "shipinstruct", "shipmode", "comment", "q6_selected",
        ])
        writer.writerows(manifest_rows)
    return converted, selected_quantities


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert real dbgen lineitem.tbl rows to fixed FPGA records"
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("testdata/tpch_lineitem_512b.bin"))
    parser.add_argument("--manifest", type=Path, default=Path("testdata/tpch_lineitem_512b.csv"))
    parser.add_argument("--records", type=int, default=128)
    parser.add_argument("--skip-rows", type=int, default=0)
    args = parser.parse_args()

    try:
        count, selected = convert(
            args.input, args.output, args.manifest, args.records, args.skip_rows
        )
    except (OSError, UnicodeError, ValueError) as exc:
        parser.error(str(exc))

    image = args.output.read_bytes()
    print("TPC-H dbgen lineitem FPGA dataset generated")
    print(f"input={args.input}")
    print(f"output={args.output}")
    print(f"manifest={args.manifest}")
    print(f"record_bytes={RECORD_BYTES} records={count} bytes={len(image)}")
    print(f"schema={SCHEMA}")
    print(f"query={Q6_FILTER_QUERY}")
    print(f"selected_count={len(selected)}")
    print(f"selected_quantities={','.join(str(value) for value in selected)}")
    print(f"selected_prefix={bytes(selected).hex()}")
    print(f"sha256={hashlib.sha256(image).hexdigest()}")


if __name__ == "__main__":
    main()
