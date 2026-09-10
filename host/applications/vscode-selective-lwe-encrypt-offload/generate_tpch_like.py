#!/usr/bin/env python3
"""Generate deterministic fixed-size binary records for selective TFHE tests."""

import argparse
import csv
import hashlib
import random
import struct
from pathlib import Path


RECORD_BYTES = 512
QUANTITY_OFFSET = 4


def predicate_match(quantity: int, threshold: int, predicate: str) -> bool:
    if predicate == "gt":
        return quantity > threshold
    return quantity == threshold


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="testdata/tpch_like_512b.bin")
    parser.add_argument("--manifest", default="testdata/tpch_like_512b.csv")
    parser.add_argument("--records", type=int, default=16)
    parser.add_argument("--seed", type=int, default=20260901)
    parser.add_argument("--predicate", choices=("gt", "eq"), default="gt")
    parser.add_argument("--threshold", type=int, default=32)
    args = parser.parse_args()

    if args.records <= 0:
        parser.error("--records must be positive")
    if not 0 <= args.threshold <= 255:
        parser.error("--threshold must fit uint8")

    output = Path(args.output)
    manifest = Path(args.manifest)
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)
    rows = []
    image = bytearray()

    for index in range(args.records):
        record_id = 100000 + index
        quantity = rng.randrange(0, 64)
        order_key = rng.getrandbits(64)
        extended_price_cents = rng.randrange(100, 1_000_000)
        record = bytearray(rng.getrandbits(8) for _ in range(RECORD_BYTES))
        struct.pack_into("<I", record, 0, record_id)
        struct.pack_into("<B", record, QUANTITY_OFFSET, quantity)
        struct.pack_into("<B", record, 5, index & 0x3)
        struct.pack_into("<H", record, 6, 0)
        struct.pack_into("<Q", record, 8, order_key)
        struct.pack_into("<Q", record, 16, extended_price_cents)
        selected = predicate_match(quantity, args.threshold, args.predicate)
        rows.append((index, record_id, quantity, int(selected)))
        image.extend(record)

    output.write_bytes(image)
    with manifest.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("record_index", "id", "quantity", "selected"))
        writer.writerows(rows)

    selected_quantities = [str(row[2]) for row in rows if row[3]]
    print("TPC-H-like 512B binary dataset generated")
    print(f"output={output}")
    print(f"manifest={manifest}")
    print(f"record_bytes={RECORD_BYTES} records={args.records} bytes={len(image)}")
    print(f"quantity_offset={QUANTITY_OFFSET} predicate={args.predicate} threshold={args.threshold}")
    print(f"selected_count={len(selected_quantities)} total_count={args.records}")
    print(f"selected_quantities={','.join(selected_quantities)}")
    print(f"sha256={hashlib.sha256(image).hexdigest()}")


if __name__ == "__main__":
    main()
