#!/usr/bin/env python3
"""Write a converted TPC-H lineitem image to NVMQ and verify readback."""

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
    output = bytearray()
    while len(output) < size:
        chunk = os.pread(fd, size - len(output), offset + len(output))
        if not chunk:
            raise OSError("short SSD read")
        output.extend(chunk)
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--device", default="/dev/nvmq0n1")
    parser.add_argument("--lba", required=True, type=int)
    args = parser.parse_args()
    if args.lba < 0:
        parser.error("--lba must be nonnegative")
    source = args.input.read_bytes()
    if not source:
        parser.error("input image must not be empty")
    padded_size = ((len(source) + LBA_BYTES - 1) // LBA_BYTES) * LBA_BYTES
    image = source + bytes(padded_size - len(source))
    byte_offset = args.lba * LBA_BYTES

    fd = os.open(args.device, os.O_RDWR | os.O_SYNC)
    try:
        transfer_exact(fd, image, byte_offset)
        os.fsync(fd)
        readback = read_exact(fd, len(image), byte_offset)
    finally:
        os.close(fd)
    if readback != image:
        raise SystemExit("SSD readback differs from TPC-H input image")

    print("TPC-H lineitem SSD write passed")
    print(f"input={args.input} device={args.device} lba={args.lba}")
    print(
        f"source_bytes={len(source)} written_bytes={len(image)} "
        f"padding_bytes={len(image) - len(source)} lbas={len(image) // LBA_BYTES}"
    )
    print(f"source_sha256={hashlib.sha256(source).hexdigest()}")
    print(f"written_sha256={hashlib.sha256(image).hexdigest()}")
    print("readback_verified=yes")


if __name__ == "__main__":
    main()

