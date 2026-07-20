#!/usr/bin/env python3
"""Re-create the p2-rtx import-patched portal2.exe from a vanilla one.

p2-rtx loads by adding ``p2-rtx.dll`` to portal2.exe's import table, so the
game itself pulls the mod in at process start (no ASI loader, no dll proxy).
Steam's update/verify flow restores the vanilla portal2.exe, which silently
disables the mod. This tool re-applies the import patch to any vanilla
portal2.exe so an update never requires shipping a new binary:

  * a new import table is appended in a 512-byte block past the last section
    (the existing descriptors are copied verbatim, then a descriptor for
    p2-rtx.dll importing AmdPowerXpressRequestHighPerformance and
    NvOptimusEnablement is added),
  * the last section is extended to cover the block (write enabled,
    discardable cleared),
  * every section holding an existing IAT is made writable,
  * SizeOfImage and the import data directory are updated.

Given the vanilla exe of game build 23973718 the output is byte-identical to
the previously shipped pre-patched exe (SHA1 cca4a727...).

Usage:
  python patch_portal2_imports.py <portal2.exe> [-o output.exe]

Without -o the file is patched in place and the vanilla original is kept
next to it as portal2.exe.vanilla. Already-patched files are detected and
left untouched. Stdlib only; Python 3.8+.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
import sys
from pathlib import Path

DLL_NAME = b"p2-rtx.dll"
IMPORT_FUNCS = [b"AmdPowerXpressRequestHighPerformance", b"NvOptimusEnablement"]

# Known hashes for game build 23973718 (informational - the patch itself is
# generic and works on any 32-bit portal2.exe).
SHA1_VANILLA_23973718 = "754149fc8da2e131c2f13324c9e087f2a690f197"
SHA1_PATCHED_23973718 = "cca4a727f24b3e2eca89cbcc9e2f74908d0ce578"

IMAGE_SCN_MEM_DISCARDABLE = 0x02000000
IMAGE_SCN_MEM_WRITE = 0x80000000


class PatchError(Exception):
    pass


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def sha1(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()


class Pe32:
    """Minimal 32-bit PE reader/patcher over a mutable byte buffer."""

    def __init__(self, data: bytearray):
        self.data = data
        if data[:2] != b"MZ":
            raise PatchError("not an MZ executable")
        (self.e_lfanew,) = struct.unpack_from("<I", data, 0x3C)
        if data[self.e_lfanew : self.e_lfanew + 4] != b"PE\0\0":
            raise PatchError("PE signature not found")
        self.file_header = self.e_lfanew + 4
        (self.num_sections,) = struct.unpack_from("<H", data, self.file_header + 2)
        (opt_size,) = struct.unpack_from("<H", data, self.file_header + 16)
        self.opt_header = self.file_header + 20
        (magic,) = struct.unpack_from("<H", data, self.opt_header)
        if magic != 0x10B:
            raise PatchError(f"not a PE32 (32-bit) image, optional header magic {magic:#x}")
        (self.section_align,) = struct.unpack_from("<I", data, self.opt_header + 32)
        (self.file_align,) = struct.unpack_from("<I", data, self.opt_header + 36)
        self.section_table = self.opt_header + opt_size

    # -- optional header fields ------------------------------------------------
    @property
    def size_of_image(self) -> int:
        return struct.unpack_from("<I", self.data, self.opt_header + 56)[0]

    @size_of_image.setter
    def size_of_image(self, value: int) -> None:
        struct.pack_into("<I", self.data, self.opt_header + 56, value)

    @property
    def checksum(self) -> int:
        return struct.unpack_from("<I", self.data, self.opt_header + 64)[0]

    @checksum.setter
    def checksum(self, value: int) -> None:
        struct.pack_into("<I", self.data, self.opt_header + 64, value)

    def data_directory(self, index: int) -> tuple[int, int]:
        off = self.opt_header + 96 + index * 8
        return struct.unpack_from("<II", self.data, off)

    def set_data_directory(self, index: int, rva: int, size: int) -> None:
        struct.pack_into("<II", self.data, self.opt_header + 96 + index * 8, rva, size)

    # -- sections --------------------------------------------------------------
    def section(self, index: int) -> dict:
        off = self.section_table + index * 40
        name, vsize, va, raw_size, raw_ptr = struct.unpack_from("<8sIIII", self.data, off)
        (chars,) = struct.unpack_from("<I", self.data, off + 36)
        return {
            "index": index,
            "offset": off,
            "name": name.rstrip(b"\0").decode(errors="replace"),
            "vsize": vsize,
            "va": va,
            "raw_size": raw_size,
            "raw_ptr": raw_ptr,
            "chars": chars,
        }

    def sections(self) -> list[dict]:
        return [self.section(i) for i in range(self.num_sections)]

    def section_containing_rva(self, rva: int) -> dict:
        for s in self.sections():
            span = max(s["vsize"], s["raw_size"])
            if s["va"] <= rva < s["va"] + align_up(span, self.section_align):
                return s
        raise PatchError(f"no section contains RVA {rva:#x}")

    def rva_to_offset(self, rva: int) -> int:
        s = self.section_containing_rva(rva)
        delta = rva - s["va"]
        if delta >= s["raw_size"]:
            raise PatchError(f"RVA {rva:#x} has no file-backed data")
        return s["raw_ptr"] + delta


def read_import_descriptors(pe: Pe32) -> tuple[list[bytes], set[int]]:
    """Return raw 20-byte import descriptors and the set of IAT (FirstThunk) RVAs."""
    imp_rva, _ = pe.data_directory(1)
    if imp_rva == 0:
        raise PatchError("image has no import table")
    descriptors: list[bytes] = []
    iat_rvas: set[int] = set()
    off = pe.rva_to_offset(imp_rva)
    while True:
        raw = bytes(pe.data[off : off + 20])
        if raw == b"\0" * 20:
            break
        descriptors.append(raw)
        iat_rvas.add(struct.unpack_from("<I", raw, 16)[0])
        off += 20
    return descriptors, iat_rvas


def build_import_block(pe: Pe32, block_rva: int) -> bytes:
    """Build the appended block: descriptors + name + hint/names + IAT + INT."""
    old_descriptors, _ = read_import_descriptors(pe)

    n_desc = len(old_descriptors) + 2  # +new dll, +null terminator
    name_off = n_desc * 20
    hint_offs = []
    cursor = name_off + len(DLL_NAME) + 1
    for func in IMPORT_FUNCS:
        hint_offs.append(cursor)
        cursor += 2 + len(func) + 1  # hint word + name + NUL
    iat_off = cursor
    cursor += (len(IMPORT_FUNCS) + 1) * 4
    int_off = cursor
    cursor += (len(IMPORT_FUNCS) + 1) * 4

    block = bytearray(align_up(cursor, pe.file_align))

    pos = 0
    for raw in old_descriptors:
        block[pos : pos + 20] = raw
        pos += 20
    struct.pack_into(
        "<5I", block, pos,
        block_rva + int_off, 0, 0, block_rva + name_off, block_rva + iat_off,
    )
    # null terminator descriptor is already zero

    block[name_off : name_off + len(DLL_NAME)] = DLL_NAME
    for func, off in zip(IMPORT_FUNCS, hint_offs):
        block[off + 2 : off + 2 + len(func)] = func  # hint stays 0
    thunks = [block_rva + off for off in hint_offs] + [0]
    struct.pack_into(f"<{len(thunks)}I", block, iat_off, *thunks)
    struct.pack_into(f"<{len(thunks)}I", block, int_off, *thunks)

    return bytes(block), cursor, len(old_descriptors) + 1


def is_already_patched(pe: Pe32) -> bool:
    try:
        descriptors, _ = read_import_descriptors(pe)
    except PatchError:
        return False
    for raw in descriptors:
        name_rva = struct.unpack_from("<I", raw, 12)[0]
        try:
            off = pe.rva_to_offset(name_rva)
        except PatchError:
            continue
        name = bytes(pe.data[off : off + len(DLL_NAME) + 1])
        if name.lower() == DLL_NAME.lower() + b"\0":
            return True
    return False


def patch(data: bytes) -> bytes:
    pe = Pe32(bytearray(data))

    bound_rva, bound_size = pe.data_directory(11)
    if bound_rva or bound_size:
        raise PatchError("image uses bound imports; refusing to patch")

    last = max(pe.sections(), key=lambda s: s["va"])
    if last["raw_ptr"] + last["raw_size"] != len(pe.data):
        raise PatchError(
            "last section does not end at EOF "
            f"({last['raw_ptr'] + last['raw_size']:#x} vs {len(pe.data):#x}); "
            "overlay data present - refusing to patch"
        )
    if pe.size_of_image != align_up(last["va"] + last["vsize"], pe.section_align):
        raise PatchError("SizeOfImage does not follow the last section; unexpected layout")

    # Bytes appended to the last section's raw data are mapped directly after
    # its existing raw data - NOT at SizeOfImage, which only coincides when
    # the raw size equals the section-aligned virtual size.
    if last["vsize"] > last["raw_size"]:
        raise PatchError(
            "last section has virtual-only (zero-fill) data past its raw data; "
            "appended bytes would overlap it - refusing to patch"
        )
    block_rva = last["va"] + last["raw_size"]

    block, used, _ = build_import_block(pe, block_rva)
    _, iat_rvas = read_import_descriptors(pe)

    # Extend the last section over the appended block.
    off = last["offset"]
    struct.pack_into("<I", pe.data, off + 8, block_rva + used - last["va"])  # VirtualSize
    struct.pack_into("<I", pe.data, off + 16, last["raw_size"] + len(block))  # SizeOfRawData
    chars = (last["chars"] | IMAGE_SCN_MEM_WRITE) & ~IMAGE_SCN_MEM_DISCARDABLE
    struct.pack_into("<I", pe.data, off + 36, chars)

    # The loader writes resolved addresses into the original IATs - make their
    # sections writable (the shipped patched exe does the same for .rdata).
    for rva in iat_rvas:
        s = pe.section_containing_rva(rva)
        struct.pack_into("<I", pe.data, s["offset"] + 36, s["chars"] | IMAGE_SCN_MEM_WRITE)

    pe.size_of_image = max(pe.size_of_image, align_up(block_rva + used, pe.section_align))
    pe.set_data_directory(1, block_rva, (len(read_import_descriptors(pe)[0]) + 2) * 20)
    if pe.checksum:
        pe.checksum = 0  # stale after edits; loader does not verify exe checksums

    return bytes(pe.data) + block


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("exe", type=Path, help="path to portal2.exe")
    ap.add_argument("-o", "--output", type=Path,
                    help="write patched exe here instead of patching in place")
    ap.add_argument("--check", action="store_true",
                    help="only report whether the exe is patched (exit 0), vanilla (exit 1) "
                         "or not a patchable PE (exit 2)")
    args = ap.parse_args(argv)

    data = args.exe.read_bytes()
    try:
        pe = Pe32(bytearray(data))
        already = is_already_patched(pe)
    except (PatchError, struct.error) as e:
        print(f"{args.exe}: not a patchable 32-bit PE: {e}", file=sys.stderr)
        return 2

    if args.check:
        if already:
            print(f"{args.exe}: patched (imports {DLL_NAME.decode()})")
            return 0
        print(f"{args.exe}: vanilla (no {DLL_NAME.decode()} import)")
        return 1

    if already:
        print(f"{args.exe}: already imports {DLL_NAME.decode()} - nothing to do")
        if args.output:
            args.output.write_bytes(data)
            print(f"copied unchanged to {args.output}")
        return 0

    in_sha = sha1(data)
    if in_sha != SHA1_VANILLA_23973718:
        print(f"note: input SHA1 {in_sha} is not the known build-23973718 vanilla exe; "
              "patching generically")

    try:
        patched = patch(data)
    except PatchError as e:
        print(f"{args.exe}: cannot patch: {e}", file=sys.stderr)
        return 3

    if args.output:
        args.output.write_bytes(patched)
        target = args.output
    else:
        # the input was just proven vanilla - always refresh the backup so a
        # Steam update's newer vanilla exe is never lost to a stale copy
        backup = args.exe.with_suffix(args.exe.suffix + ".vanilla")
        shutil.copy2(args.exe, backup)
        print(f"vanilla exe backed up to {backup}")
        args.exe.write_bytes(patched)
        target = args.exe

    out_sha = sha1(patched)
    print(f"patched {target} ({len(data)} -> {len(patched)} bytes, SHA1 {out_sha})")
    if in_sha == SHA1_VANILLA_23973718:
        if out_sha == SHA1_PATCHED_23973718:
            print("verified: byte-identical to the known-good patched exe")
        else:
            print("ERROR: output does not match the known-good patched exe", file=sys.stderr)
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
