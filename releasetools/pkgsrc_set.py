#!/usr/bin/env python3
"""Build a MINIX release set containing a pinned pkgsrc source snapshot."""

from __future__ import annotations

import argparse
import copy
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile
import urllib.request


DEFAULT_REPOSITORY = "eclairdosesleekman/pkgsrc"
DEFAULT_COMMIT = "18dfea989391bf165dfe51a1667f5df44ec94863"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create pkgsrc.tgz under the MINIX release sets directory."
    )
    parser.add_argument("--sets-dir", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--commit", default=DEFAULT_COMMIT)
    source.add_argument("--archive", type=Path)
    return parser.parse_args()


def cached_source_archive(work_dir: Path, commit: str) -> Path:
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("--commit must be a 40-character lowercase hexadecimal SHA")

    work_dir.mkdir(parents=True, exist_ok=True)
    cached = work_dir / f"pkgsrc-{commit}.tar.gz"
    if cached.is_file() and cached.stat().st_size:
        return cached

    url = f"https://codeload.github.com/{DEFAULT_REPOSITORY}/tar.gz/{commit}"
    temporary = cached.with_name(f"{cached.name}.tmp.{os.getpid()}")
    request = urllib.request.Request(
        url, headers={"User-Agent": "MINIX-pkgsrc-ISO-builder"}
    )
    print(f"Downloading pkgsrc snapshot {commit} from {DEFAULT_REPOSITORY}...")
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            with temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
        if not temporary.is_file() or not temporary.stat().st_size:
            raise RuntimeError("downloaded pkgsrc archive is empty")
        os.replace(temporary, cached)
    finally:
        temporary.unlink(missing_ok=True)
    return cached


def safe_member_parts(member: tarfile.TarInfo) -> tuple[str, ...]:
    path = PurePosixPath(member.name)
    if path.is_absolute() or not path.parts or ".." in path.parts:
        raise ValueError(f"unsafe path in pkgsrc source archive: {member.name!r}")
    return path.parts


def add_directory(archive: tarfile.TarFile, name: str) -> None:
    info = tarfile.TarInfo(name)
    info.type = tarfile.DIRTYPE
    info.mode = 0o755
    info.uid = 0
    info.gid = 0
    info.uname = "root"
    info.gname = "wheel"
    archive.addfile(info)


def make_pkgsrc_set(source_path: Path, output_path: Path) -> None:
    temporary_output = output_path.with_name(
        f"{output_path.name}.tmp.{os.getpid()}"
    )
    try:
        with tarfile.open(source_path, mode="r:*") as source:
            members = source.getmembers()
            if not members:
                raise ValueError("pkgsrc source archive is empty")

            roots = {safe_member_parts(member)[0] for member in members}
            if len(roots) != 1:
                raise ValueError(
                    "expected a GitHub source archive with one top-level directory"
                )
            source_root = next(iter(roots))

            has_bootstrap = any(
                len(parts := safe_member_parts(member)) == 3
                and parts[0] == source_root
                and parts[1:] == ("bootstrap", "bootstrap")
                and member.isfile()
                for member in members
            )
            if not has_bootstrap:
                raise ValueError(
                    "not a pkgsrc source archive: bootstrap/bootstrap is missing"
                )

            with tarfile.open(
                temporary_output, mode="w:gz", format=tarfile.PAX_FORMAT
            ) as destination:
                add_directory(destination, "usr/")
                add_directory(destination, "usr/pkgsrc/")

                for member in members:
                    parts = safe_member_parts(member)
                    relative_parts = parts[1:]
                    if not relative_parts:
                        continue

                    new_member = copy.copy(member)
                    new_member.name = str(
                        PurePosixPath("usr/pkgsrc").joinpath(*relative_parts)
                    )
                    new_member.uid = 0
                    new_member.gid = 0
                    new_member.uname = "root"
                    new_member.gname = "wheel"
                    # Do not carry the source archive's old path override into
                    # the newly rooted usr/pkgsrc archive.
                    new_member.pax_headers = {}

                    if member.islnk():
                        link_parts = PurePosixPath(member.linkname).parts
                        if link_parts and link_parts[0] == source_root:
                            new_member.linkname = str(
                                PurePosixPath("usr/pkgsrc").joinpath(*link_parts[1:])
                            )

                    if member.isfile():
                        source_file = source.extractfile(member)
                        if source_file is None:
                            raise ValueError(
                                f"could not read pkgsrc archive member: {member.name}"
                            )
                        with source_file:
                            destination.addfile(new_member, source_file)
                    elif member.isdir() or member.issym() or member.islnk():
                        destination.addfile(new_member)
                    else:
                        raise ValueError(
                            f"unsupported special file in pkgsrc archive: {member.name}"
                        )

        os.replace(temporary_output, output_path)
    finally:
        temporary_output.unlink(missing_ok=True)


def update_checksums(sets_dir: Path, archive_path: Path) -> None:
    algorithms = (("MD5", "md5"), ("SHA512", "sha512"))
    manifests = [(sets_dir / name, name, algorithm) for name, algorithm in algorithms]
    for path, _, _ in manifests:
        if not path.is_file():
            raise FileNotFoundError(f"release checksum manifest is missing: {path}")

    for path, label, algorithm in manifests:
        digest = hashlib.new(algorithm)
        with archive_path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(chunk)

        lines = path.read_text(encoding="ascii").splitlines()
        lines = [
            line for line in lines
            if not line.startswith(f"{label} (pkgsrc.tgz) =")
        ]
        lines.append(f"{label} (pkgsrc.tgz) = {digest.hexdigest()}")

        temporary = path.with_name(f"{path.name}.tmp.{os.getpid()}")
        try:
            temporary.write_text("\n".join(lines) + "\n", encoding="ascii")
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def main() -> int:
    args = parse_args()
    sets_dir = args.sets_dir.resolve()
    work_dir = args.work_dir.resolve()
    sets_dir.mkdir(parents=True, exist_ok=True)

    if args.archive is not None:
        source_path = args.archive.expanduser().resolve()
        if not source_path.is_file():
            raise FileNotFoundError(f"pkgsrc archive not found: {source_path}")
    else:
        source_path = cached_source_archive(work_dir, args.commit)

    output_path = sets_dir / "pkgsrc.tgz"
    print(f"Creating {output_path}...")
    make_pkgsrc_set(source_path, output_path)
    update_checksums(sets_dir, output_path)
    print("pkgsrc release set and checksum manifests are ready.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, tarfile.TarError, ValueError) as error:
        raise SystemExit(f"pkgsrc-set: {error}")
