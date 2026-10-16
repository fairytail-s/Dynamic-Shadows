import argparse
import csv
import hashlib
import json
import mmap
import re
import struct
from pathlib import Path


def current_pattern(root):
    source = (root / "src" / "GameTime.cpp").read_text(encoding="utf-8")
    return re.search(r'kGetTimeSignature\s*=\s*"([?A-Fa-f0-9 ]+)"', source).group(1)


def inspect(path, pattern):
    with path.open("rb") as file:
        with mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
            if data[:6] != b"\x7fELF\x02\x01":
                raise ValueError(f"{path}: expected little-endian ELF64")
            header = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
            if header[2] != 183:
                raise ValueError(f"{path}: expected AArch64")
            expression = re.compile(b"".join(b"." if token in ("?", "??") else re.escape(bytes([int(token, 16)]))
                                             for token in pattern.split()), re.DOTALL)
            segments = []
            matches = []
            build_id = None
            for i in range(header[10]):
                kind, flags, offset, address, _, file_size, memory_size, alignment = struct.unpack_from(
                    "<IIQQQQQQ", data, header[5] + i * header[9])
                if kind == 1:
                    segments.append({"file_offset": hex(offset), "rva": hex(address), "file_size": hex(file_size),
                                     "memory_size": hex(memory_size), "flags": flags, "alignment": hex(alignment)})
                    if flags & 1:
                        for match in expression.finditer(data, offset, offset + file_size):
                            rva = address + match.start() - offset
                            if rva % 4 == 0:
                                matches.append(hex(rva))
                if kind == 4:
                    cursor = offset
                    while cursor + 12 <= offset + file_size:
                        name_size, description_size, note_type = struct.unpack_from("<III", data, cursor)
                        cursor += 12
                        name = bytes(data[cursor:cursor + name_size]).rstrip(b"\0")
                        cursor += (name_size + 3) & ~3
                        description = bytes(data[cursor:cursor + description_size])
                        cursor += (description_size + 3) & ~3
                        if name == b"GNU" and note_type == 3:
                            build_id = description.hex()
            versions = sorted(set(match.group().rstrip(b"\0").decode("ascii") for match in re.finditer(
                rb"(?<![0-9.])1\.26\.[0-9]+\.[0-9]+(?:_[A-Za-z0-9]+)?\x00", data)))
            names = sorted(set(match.group(1).decode("ascii") for match in re.finditer(
                rb"(?<![A-Za-z0-9_])((?:gl|egl)[A-Z][A-Za-z0-9_]{2,80})\x00", data)))
            uniforms = {name: (name.encode("ascii") + b"\0") in data for name in
                        ["u_view", "u_proj", "u_viewProj", "u_modelView", "u_modelViewProj"]}
            return {"filename": path.name, "size_bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                    "gnu_build_id": build_id, "machine": "AArch64", "elf_class": 64, "version_strings": versions,
                    "load_segments": segments, "get_time_pattern": pattern, "get_time_matches": matches,
                    "get_time_status": "unique_pattern_candidate" if len(matches) == 1 else "missing_or_ambiguous",
                    "gl_name_strings": names, "camera_uniform_strings": uniforms,
                    "semantic_validation": "Pattern counts and embedded strings only; inspect AArch64 control flow before accepting a new build."}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--old", type=Path)
    parser.add_argument("--new", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--csv", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    pattern = current_pattern(root)
    result = {"new": inspect(args.new, pattern)}
    if args.old:
        result["old"] = inspect(args.old, pattern)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", newline="", encoding="utf-8") as file:
            writer = csv.writer(file)
            writer.writerow(["name", "old_rvas", "new_rvas", "old_matches", "new_matches", "status", "pattern"])
            old = result.get("old", {}).get("get_time_matches", [])
            new = result["new"]["get_time_matches"]
            writer.writerow(["Level::getTime", ";".join(old), ";".join(new), len(old) if args.old else "", len(new),
                             result["new"]["get_time_status"], pattern])
    print(args.output.resolve())


if __name__ == "__main__":
    main()
