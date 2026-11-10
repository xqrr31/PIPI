import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(command, *, cwd, env=None, check=False):
    merged = os.environ.copy()
    if env:
        merged.update(env)
    return subprocess.run(
        [str(item) for item in command], cwd=cwd, env=merged, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, check=check,
    )


class GitHubPublishingScriptTests(unittest.TestCase):
    @unittest.skipUnless(subprocess.run(["git", "--version"], capture_output=True).returncode == 0,
                         "Git is required")
    def test_one_time_setup_and_repeat_publish_reuse_the_same_remote(self):
        with tempfile.TemporaryDirectory(prefix="pipi_github_publish_") as temporary:
            base = Path(temporary)
            project = base / "project"
            mockbin = base / "mockbin"
            remote = base / "github-pipi.git"
            project.mkdir()
            mockbin.mkdir()
            (project / "scripts").mkdir()
            (project / "scripts" / "github-init.sh").write_bytes(
                (ROOT / "scripts" / "github-init.sh").read_bytes())
            (project / "scripts" / "github-publish.sh").write_bytes(
                (ROOT / "scripts" / "github-publish.sh").read_bytes())
            for script in (project / "scripts").iterdir():
                script.chmod(0o755)
            (project / "PIPI_VERSION").write_text("1.0.0\n", encoding="utf-8")
            (project / "README.md").write_text("PIPI GitHub test\n", encoding="utf-8")

            run(["git", "init", "--bare", remote], cwd=base, check=True)
            state_file = base / "created-once"
            create_log = base / "create-count.txt"
            release_state = base / "release-created-once"
            release_log = base / "release-create-count.txt"
            mock_gh = mockbin / "gh"
            mock_gh.write_text(
                "#!/bin/sh\n"
                "case \"$1 $2\" in\n"
                "  'auth status') exit 0 ;;\n"
                "  'repo view') [ -f \"$PIPI_GH_TEST_STATE\" ] && exit 0 || exit 1 ;;\n"
                "  'repo create') echo create >> \"$PIPI_GH_TEST_LOG\"; touch \"$PIPI_GH_TEST_STATE\"; exit 0 ;;\n"
                "  'release view') [ -f \"$PIPI_GH_TEST_RELEASE_STATE\" ] && exit 0 || exit 1 ;;\n"
                "  'release create') echo \"$*\" >> \"$PIPI_GH_TEST_RELEASE_LOG\"; touch \"$PIPI_GH_TEST_RELEASE_STATE\"; exit 0 ;;\n"
                "  *) echo unexpected-gh-call: \"$*\" >&2; exit 90 ;;\n"
                "esac\n",
                encoding="utf-8",
            )
            mock_gh.chmod(0o755)

            git_config = base / "gitconfig"
            git_config.write_text(
                "[user]\n name = PIPI Test\n email = pipi-test@example.invalid\n"
                "[init]\n defaultBranch = main\n"
                f'[url "file://{remote}"]\n insteadOf = https://github.com/test-owner/pipi-test.git\n',
                encoding="utf-8",
            )
            environment = {
                "PATH": str(mockbin) + os.pathsep + os.environ.get("PATH", ""),
                "GIT_CONFIG_GLOBAL": str(git_config),
                "PIPI_GH_TEST_STATE": str(state_file),
                "PIPI_GH_TEST_LOG": str(create_log),
                "PIPI_GH_TEST_RELEASE_STATE": str(release_state),
                "PIPI_GH_TEST_RELEASE_LOG": str(release_log),
            }

            first = run(["./scripts/github-init.sh", "test-owner/pipi-test", "--public"],
                        cwd=project, env=environment)
            self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
            self.assertIn("repository is connected", first.stdout)
            self.assertTrue((project / ".git").is_dir())
            self.assertEqual(create_log.read_text(encoding="utf-8").splitlines(), ["create"])

            # Running the one-time command again must reuse origin and not create another repo.
            second_env = dict(environment)
            second_env["PATH"] = os.environ.get("PATH", "")  # no gh executable on the second setup call
            second = run(["./scripts/github-init.sh", "test-owner/pipi-test", "--public"],
                         cwd=project, env=second_env)
            self.assertEqual(second.returncode, 0, second.stdout + second.stderr)
            self.assertIn("Reusing existing origin", second.stdout)
            self.assertEqual(create_log.read_text(encoding="utf-8").splitlines(), ["create"])

            # A later version update must push into that same remote and add one immutable tag.
            (project / "README.md").write_text("PIPI GitHub test: updated\n", encoding="utf-8")
            publish = run(["./scripts/github-publish.sh"], cwd=project, env=environment)
            self.assertEqual(publish.returncode, 0, publish.stdout + publish.stderr)
            self.assertIn("Publish complete", publish.stdout)
            refs = run(["git", "--git-dir", remote, "show-ref"], cwd=base, check=True).stdout
            self.assertIn("refs/heads/main", refs)
            self.assertIn("refs/tags/v1.0.0", refs)
            self.assertEqual(create_log.read_text(encoding="utf-8").splitlines(), ["create"])

            # Optional release creation is also idempotent and does not create another repo.
            release = run(["./scripts/github-publish.sh", "--release"], cwd=project, env=environment)
            self.assertEqual(release.returncode, 0, release.stdout + release.stderr)
            self.assertTrue(release_state.exists())
            self.assertEqual(len(release_log.read_text(encoding="utf-8").splitlines()), 1)
            release_again = run(["./scripts/github-publish.sh", "--release"], cwd=project, env=environment)
            self.assertEqual(release_again.returncode, 0, release_again.stdout + release_again.stderr)
            self.assertIn("already exists", release_again.stdout)
            self.assertEqual(len(release_log.read_text(encoding="utf-8").splitlines()), 1)

            # Even an untracked file must not be published under an already-used version tag.
            (project / "new-unreleased-file.txt").write_text("must require a version bump\n", encoding="utf-8")
            immutable = run(["./scripts/github-publish.sh"], cwd=project, env=environment)
            self.assertNotEqual(immutable.returncode, 0)
            self.assertIn("already exists and is immutable", immutable.stderr)
            self.assertEqual(create_log.read_text(encoding="utf-8").splitlines(), ["create"])

            # Re-running publish without changes is idempotent and does not recreate or move tags.
            (project / "new-unreleased-file.txt").unlink()
            again = run(["./scripts/github-publish.sh"], cwd=project, env=environment)
            self.assertEqual(again.returncode, 0, again.stdout + again.stderr)
            self.assertEqual(create_log.read_text(encoding="utf-8").splitlines(), ["create"])

    def test_publish_refuses_to_create_a_repository_when_origin_is_missing(self):
        with tempfile.TemporaryDirectory(prefix="pipi_publish_no_origin_") as temporary:
            project = Path(temporary)
            (project / "PIPI_VERSION").write_text("1.0.0\n", encoding="utf-8")
            (project / "scripts").mkdir()
            publisher = project / "scripts" / "github-publish.sh"
            publisher.write_bytes((ROOT / "scripts" / "github-publish.sh").read_bytes())
            publisher.chmod(0o755)
            run(["git", "init"], cwd=project, check=True)
            result = run(["./scripts/github-publish.sh"], cwd=project)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("no 'origin' remote", result.stderr)
            self.assertNotIn("repo create", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
