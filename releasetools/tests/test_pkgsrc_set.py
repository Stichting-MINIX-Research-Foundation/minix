"""Regression tests for the MINIX pkgsrc release-set builder."""

from __future__ import annotations

import io
from pathlib import Path
import re
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pkgsrc_set import DEFAULT_COMMIT, make_pkgsrc_set


class PkgsrcSetTest(unittest.TestCase):
    def test_default_snapshot_matches_uefi_iso_pin(self) -> None:
        iso_script = Path(__file__).resolve().parents[1] / "x86_64_uefi_cdimage.sh"
        match = re.search(
            r"^: \$\{PKGSRC_COMMIT:=([0-9a-f]{40})\}$",
            iso_script.read_text(encoding="utf-8"),
            re.MULTILINE,
        )

        self.assertIsNotNone(match, "UEFI ISO script must define a pkgsrc commit")
        assert match is not None
        self.assertEqual(DEFAULT_COMMIT, match.group(1))

    def test_bootstrap_entry_point_is_executable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            source_path = directory / "pkgsrc-source.tar.gz"
            output_path = directory / "pkgsrc.tgz"

            source_files = (
                ("bootstrap/bootstrap", b"#!/bin/sh\nexit 0\n", 0o644),
                ("bootstrap/cleanup", b"#!/bin/sh\nexit 0\n", 0o755),
                ("README", b"pkgsrc test fixture\n", 0o644),
            )
            with tarfile.open(source_path, mode="w:gz") as source:
                for relative_path, contents, mode in source_files:
                    member = tarfile.TarInfo(
                        f"pkgsrc-snapshot/{relative_path}"
                    )
                    member.mode = mode
                    member.size = len(contents)
                    source.addfile(member, io.BytesIO(contents))

            make_pkgsrc_set(source_path, output_path)

            with tarfile.open(output_path, mode="r:gz") as output:
                bootstrap = output.getmember(
                    "usr/pkgsrc/bootstrap/bootstrap"
                )
                cleanup = output.getmember("usr/pkgsrc/bootstrap/cleanup")
                readme = output.getmember("usr/pkgsrc/README")

            self.assertEqual(bootstrap.mode, 0o755)
            self.assertEqual(cleanup.mode, 0o755)
            self.assertEqual(readme.mode, 0o644)


if __name__ == "__main__":
    unittest.main()
