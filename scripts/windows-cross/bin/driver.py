#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Dispatch macOS tools with Windows/MSVC headers, libraries, and ABI."""
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
CONFIG = json.loads((ROOT / 'wrapper-config.json').read_text())
INCLUDES = ('crt/include', 'sdk/include/ucrt', 'sdk/include/shared',
            'sdk/include/um', 'sdk/include/winrt')


def run(tool):
    args = sys.argv[1:]
    lld = str(ROOT / CONFIG['lld_relative_path'])
    if tool == 'clang-cl':
        command = [CONFIG['clang'], '--driver-mode=cl', '--target=x86_64-pc-windows-msvc',
                   '/clang:-U__apple_build_version__']
        for include in INCLUDES:
            command += ['-imsvc', str(ROOT / 'sysroot' / include)]
    elif tool == 'lld-link':
        command = [lld, '-flavor', 'link']
        for library in ('crt/lib/x86_64', 'sdk/lib/ucrt/x86_64', 'sdk/lib/um/x86_64'):
            command.append('/libpath:' + str(ROOT / 'sysroot' / library))
    elif tool == 'llvm-lib':
        command = [lld, '-flavor', 'link', '/lib']
    elif tool == 'rc-preprocess':
        command = [CONFIG['clang'], '--target=x86_64-pc-windows-msvc', '-E', '-xc', '-DRC_INVOKED']
        for include in INCLUDES:
            command += ['-isystem', str(ROOT / 'sysroot' / include)]
    else:
        raise SystemExit('Unknown tool: ' + tool)
    os.execv(command[0], command + args)
