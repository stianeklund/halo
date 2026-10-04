from typing import Optional
import importlib.util
import os
import tempfile
import subprocess
import shutil
from pathlib import Path

from .helpers import try_remove

_splice_module = None


def _halo_splicer():
    """[halo] In-process splice compiler (tools/permuter/splice_compile.py)
    when run.py selected the raw objective.  Each worker process loads it
    once instead of starting compile.sh + Python for every candidate."""
    global _splice_module
    source = os.environ.get("PERMUTER_SPLICE_SOURCE")
    func = os.environ.get("PERMUTER_SPLICE_FUNC")
    if not source or not func:
        return None
    if _splice_module is None:
        for parent in Path(__file__).resolve().parents:
            path = parent / "tools" / "permuter" / "splice_compile.py"
            if path.is_file():
                spec = importlib.util.spec_from_file_location("splice_compile", str(path))
                mod = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(mod)
                _splice_module = mod
                break
        else:
            raise RuntimeError("[halo] splice_compile.py not found")
    return _splice_module.splicer(Path(source), func)


class Compiler:
    def __init__(
        self, compile_cmd: str, *, show_errors: bool, debug_mode: bool
    ) -> None:
        self.compile_cmd = compile_cmd
        self.show_errors = show_errors
        self.debug_mode = debug_mode

    def compile(self, source: str, *, show_errors: bool = False) -> Optional[str]:
        """Try to compile a piece of C code. Returns the filename of the resulting .o
        temp file if it succeeds."""
        show_errors = show_errors or self.show_errors or self.debug_mode
        splicer = _halo_splicer()
        if splicer is not None and not self.debug_mode:
            with tempfile.NamedTemporaryFile(
                prefix="permuter", suffix=".o", delete=False
            ) as f2:
                o_name = f2.name
            if splicer.compile_candidate(source, Path(o_name)):
                return o_name
            try_remove(o_name)
            return None
        with tempfile.NamedTemporaryFile(
            prefix="permuter", suffix=".c", mode="w", delete=False
        ) as f:
            c_name = f.name
            f.write(source)

        if self.debug_mode:
            debug_filepath = "./debug_source.c"
            print(
                "DEBUG MODE: Saving a full copy of base candidate source to ",
                debug_filepath,
            )
            with open(debug_filepath, "w") as f_copy:
                f_copy.write(source)

        with tempfile.NamedTemporaryFile(
            prefix="permuter", suffix=".o", delete=False
        ) as f2:
            o_name = f2.name

        try:
            stderr = 2 if show_errors else subprocess.DEVNULL
            subprocess.check_call(
                [self.compile_cmd, c_name, "-o", o_name],
                stdout=stderr,
                stderr=stderr,
            )
        except subprocess.CalledProcessError:
            if not show_errors:
                try_remove(c_name)
            try_remove(o_name)
            return None
        except KeyboardInterrupt:
            # If Ctrl+C happens during this call, make a best effort in
            # removing the .c and .o files. This is totally racy, but oh well...
            try_remove(c_name)
            try_remove(o_name)
            raise

        if self.debug_mode:
            debug_filepath = "./debug_compiled_object.o"
            print(
                "DEBUG MODE: Saving the base candidate o file to ", debug_filepath, "\n"
            )
            shutil.copyfile(o_name, debug_filepath)

        try_remove(c_name)
        return o_name
