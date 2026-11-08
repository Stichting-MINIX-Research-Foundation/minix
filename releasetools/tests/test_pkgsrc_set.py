"""Regression tests for the MINIX pkgsrc release-set builder."""

from __future__ import annotations

import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pkgsrc_set import make_pkgsrc_set, parse_args, resolve_pkgsrc_commit


class PkgsrcSetTest(unittest.TestCase):
    def test_default_snapshot_resolves_repository_default_branch(self) -> None:
        branch = "pkgsrc-2026Q3"
        commit = "5c2d0e756db85b49bcfa5e507298f8f93be85fe8"
        responses = [
            io.BytesIO(json.dumps({"default_branch": branch}).encode("utf-8")),
            io.BytesIO(json.dumps({"sha": commit}).encode("utf-8")),
        ]

        with mock.patch(
            "pkgsrc_set.urllib.request.urlopen", side_effect=responses
        ) as urlopen:
            resolved_branch, resolved_commit = resolve_pkgsrc_commit()

        self.assertEqual(resolved_branch, branch)
        self.assertEqual(resolved_commit, commit)
        self.assertEqual(urlopen.call_count, 2)
        request = urlopen.call_args_list[1].args[0]
        self.assertIn(
            "/repos/eclairdosesleekman/pkgsrc/commits/pkgsrc-2026Q3",
            request.full_url,
        )

    def test_explicit_branch_skips_default_branch_lookup(self) -> None:
        branch = "pkgsrc-2026Q3"
        commit = "5c2d0e756db85b49bcfa5e507298f8f93be85fe8"
        response = io.BytesIO(json.dumps({"sha": commit}).encode("utf-8"))

        with mock.patch(
            "pkgsrc_set.urllib.request.urlopen", return_value=response
        ) as urlopen:
            resolved_branch, resolved_commit = resolve_pkgsrc_commit(branch)

        self.assertEqual((resolved_branch, resolved_commit), (branch, commit))
        self.assertEqual(urlopen.call_count, 1)

    def test_cli_does_not_pin_a_stale_default_commit(self) -> None:
        with mock.patch(
            "sys.argv",
            ["pkgsrc_set.py", "--sets-dir", "sets", "--work-dir", "work"],
        ):
            args = parse_args()

        self.assertIsNone(args.commit)
        self.assertIsNone(args.branch)
        self.assertIsNone(args.archive)

    def test_bootstrap_entry_point_is_executable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            source_path = directory / "pkgsrc-source.tar.gz"
            output_path = directory / "pkgsrc.tgz"

            source_files = (
                ("bootstrap/bootstrap", b"bootstrap test entrypoint", 0o644),
                ("bootstrap/cleanup", b"bootstrap cleanup", 0o755),
                ("README", b"pkgsrc test fixture", 0o644),
            )
            with tarfile.open(source_path, mode="w:gz") as source:
                for relative_path, contents, mode in source_files:
                    member = tarfile.TarInfo(f"pkgsrc-snapshot/{relative_path}")
                    member.mode = mode
                    member.size = len(contents)
                    source.addfile(member, io.BytesIO(contents))

            make_pkgsrc_set(source_path, output_path)

            with tarfile.open(output_path, mode="r:gz") as output:
                bootstrap = output.getmember("usr/pkgsrc/bootstrap/bootstrap")
                cleanup = output.getmember("usr/pkgsrc/bootstrap/cleanup")
                readme = output.getmember("usr/pkgsrc/README")

            self.assertEqual(bootstrap.mode, 0o755)
            self.assertEqual(cleanup.mode, 0o755)
            self.assertEqual(readme.mode, 0o644)


if __name__ == "__main__":
    unittest.main()
