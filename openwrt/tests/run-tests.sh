#!/bin/sh
# Test the OpenWrt scripts (generator, migration) without OpenWrt.
# Needs ucode with the uci and fs modules. Set UCODE to the ucode binary
# if it is not in PATH. Exit 77 (skipped) if ucode or a module is missing.
# Optional argument: the confusedump program (tellstick.conf with libconfuse).

dir=$(cd "$(dirname "$0")/.." && pwd)
ucode=${UCODE:-$(command -v ucode)}

if [ -z "$ucode" ] || ! "$ucode" -e 'import { cursor } from "uci"; import * as fs from "fs";' >/dev/null 2>&1; then
	echo "ucode with the uci and fs modules not found => skip"
	exit 77
fi

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

# test.uc runs the scripts with "ucode", so put ours first in PATH
mkdir "$tmp/bin" && ln -s "$ucode" "$tmp/bin/ucode"
PATH="$tmp/bin:$PATH" "$ucode" "$dir/tests/test.uc" "$dir" "$tmp" "$1"
