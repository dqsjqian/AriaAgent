"""The selected interpreter and SDK lease reach every configure."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("agent_build", ROOT / "scripts/build.py")
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class BuildInterpreterTests(unittest.TestCase):
    def test_explicit_interpreter_reaches_both_configures_without_activation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fetch = root / "scripts/ci/fetch_aria.py"
            fetch.parent.mkdir(parents=True)
            fetch.touch()
            directory = root / "output"
            (directory / "bin").mkdir(parents=True)
            (directory / "bin/aria_agent").touch()
            interpreter = str(root / "venv with spaces/bin/python")
            with mock.patch.object(build, "ROOT", root), \
                    mock.patch.object(build.sys, "executable", interpreter), \
                    mock.patch.object(build.platform, "system", return_value="Linux"), \
                    mock.patch.object(build.shutil, "which", return_value="available"), \
                    mock.patch.object(build, "run") as run:
                self.assertEqual(build.main(["--qt-dir", str(root / "Qt"),
                                             "--build-dir", str(directory)]), 0)
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(commands[0], commands[1])
            self.assertIn("-DFORGEDEPS_PYTHON_EXECUTABLE=" + interpreter, commands[0])
            self.assertIn("-DARIA_DIR=", commands[0])
            self.assertEqual(commands[2][1], "--build")

    def test_msvc_multiconfig_deploys_qt_without_objdump(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            exe = root / "build/bin/Release/aria_agent.exe"
            exe.parent.mkdir(parents=True)
            exe.touch()
            deploy = root / "Qt/bin/windeployqt.exe"
            deploy.parent.mkdir(parents=True)
            deploy.touch()
            with mock.patch.object(build.platform, "system", return_value="Windows"), \
                    mock.patch.object(build, "find_msys2_bin", return_value=None), \
                    mock.patch.object(build.shutil, "which", return_value=None), \
                    mock.patch.object(build.subprocess, "run") as run:
                build.deploy_dlls(root / "build", None, qt_prefix=root / "Qt")
            run.assert_called_once()
            self.assertEqual(run.call_args.args[0][0], str(deploy))
            self.assertEqual(run.call_args.args[0][-1], str(exe))
            self.assertTrue(run.call_args.kwargs["check"])


if __name__ == "__main__":
    unittest.main()
