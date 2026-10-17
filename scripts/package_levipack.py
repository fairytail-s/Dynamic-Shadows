import argparse
import json
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    library = args.library.resolve()
    header = library.read_bytes()[:64]
    if len(header) < 64 or header[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", header, 18)[0] != 183:
        raise SystemExit("Dynamic Shadows requires an Android arm64-v8a shared library")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.output, "w", ZIP_DEFLATED, compresslevel=9) as archive:
        archive.write(library, manifest["entry"])
        # config/config.json is created by the mod on first start (and kept across updates), so it is not packaged.
        for name in ["manifest.json", "icon.png"]:
            archive.write(root / name, name)
    print(args.output.resolve())


if __name__ == "__main__":
    main()
