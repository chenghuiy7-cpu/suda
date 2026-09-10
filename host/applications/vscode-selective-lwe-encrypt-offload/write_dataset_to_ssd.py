#!/usr/bin/env python3
"""Write a benchmark image to an nvmq SSD and verify its padded readback."""

import argparse
import hashlib
import os
from pathlib import Path


LBA_BYTES = 4096


def transfer_exact(fd: int, data: bytes, offset: int) -> None:
    done = 0
    while done < len(data):
        written = os.pwrite(fd, data[done:], offset + done)
        if written <= 0:
            raise OSError("short SSD write")
        done += written


def read_exact(fd: int, size: int, offset: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        chunk = os.pread(fd, size - len(chunks), offset + len(chunks))
        if not chunk:
            raise OSError("short SSD read")
        chunks.extend(chunk)
    return bytes(chunks)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="testdata/tpch_like_512b.bin")
    parser.add_argument("--device", default="/dev/nvmq0n1")
    parser.add_argument("--lba", type=int, required=True)
    args = parser.parse_args()

    source_image = Path(args.input).read_bytes()
    if not source_image:
        parser.error("input image must not be empty")
    if args.lba < 0:
        parser.error("--lba must be nonnegative")

    padded_bytes = ((len(source_image) + LBA_BYTES - 1) // LBA_BYTES) * LBA_BYTES
    image = source_image + bytes(padded_bytes - len(source_image))

    byte_offset = args.lba * LBA_BYTES
    fd = os.open(args.device, os.O_RDWR | os.O_SYNC)
    try:
        transfer_exact(fd, image, byte_offset)
        os.fsync(fd)
        readback = read_exact(fd, len(image), byte_offset)
    finally:
        os.close(fd)

    if readback != image:
        raise SystemExit("SSD readback differs from input image")
    print("TPC-H-like SSD write passed")
    print(f"input={args.input} device={args.device} lba={args.lba}")
    print(
        f"source_bytes={len(source_image)} written_bytes={len(image)} "
        f"padding_bytes={len(image) - len(source_image)} lbas={len(image) // LBA_BYTES}"
    )
    print(f"source_sha256={hashlib.sha256(source_image).hexdigest()}")
    print(f"written_sha256={hashlib.sha256(image).hexdigest()}")
    print("readback_verified=yes")


if __name__ == "__main__":
    main()
