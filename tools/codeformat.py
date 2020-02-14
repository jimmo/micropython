#!/usr/bin/env python3
#
# This file is part of the MicroPython project, http://micropython.org/
#
# The MIT License (MIT)
#
# Copyright (c) 2020 Damien P. George
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import argparse
import glob
import os
import sys

# Relative to top-level repo dir.
PATHS = [
    # C
    "extmod/*.[ch]",
    "lib/netutils/*.[ch]",
    "lib/timeutils/*.[ch]",
    "lib/utils/*.[ch]",
    "mpy-cross/*.[ch]",
    "ports/*/*.[ch]",
    "py/*.[ch]",
    # Python
    "drivers/**/*.py",
    "examples/**/*.py",
    "extmod/**/*.py",
    "ports/**/*.py",
    "py/**/*.py",
    "tools/**/*.py",
]

EXCLUSIONS = [
    # STM32 build includes generated Python code.
    "ports/*/build*",
    # gitignore in ports/unix ignores *.py, so also do it here.
    "ports/unix/*.py",
]

# Path to repo top-level dir.
TOP = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

UNCRUSTIFY_CFG = os.path.join(TOP, "tools/uncrustify.cfg")

C_EXTS = (
    ".c",
    ".h",
)
PY_EXTS = (".py",)


def list_files(paths, exclusions=None, prefix=""):
    files = set()
    for pattern in paths:
        files.update(glob.glob(os.path.join(prefix, pattern), recursive=True))
    for pattern in exclusions or []:
        files.difference_update(
            glob.fnmatch.filter(files, os.path.join(prefix, pattern))
        )
    return sorted(files)


def main():
    cmd_parser = argparse.ArgumentParser(description="Auto-format C and Python files.")
    cmd_parser.add_argument("-c", action="store_true", help="Format C code only")
    cmd_parser.add_argument("-p", action="store_true", help="Format Python code only")
    cmd_parser.add_argument("files", nargs="*", help="Run on specific globs")
    args = cmd_parser.parse_args()

    # Setting only one of -c or -p disables the other. If both or neither are set, then do both.
    format_c = args.c or not args.p
    format_py = args.p or not args.c

    # Expand the globs passed on the command line, or use the default globs above.
    files = []
    if args.files:
        files = list_files(args.files)
    else:
        files = list_files(PATHS, EXCLUSIONS, TOP)

    # Extract files matching a specific language.
    def lang_files(exts):
        for file in files:
            if os.path.splitext(file)[1].lower() in exts:
                yield "'" + file + "'"

    # Format C files with uncrustify.
    os.system(
        "uncrustify -c '{}' -l C --no-backup {}".format(
            UNCRUSTIFY_CFG, " ".join(lang_files(C_EXTS))
        )
    )

    # Format Python files with black.
    os.system("black -q --fast {}".format(" ".join(lang_files(PY_EXTS))))


if __name__ == "__main__":
    main()
