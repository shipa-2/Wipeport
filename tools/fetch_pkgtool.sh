#!/usr/bin/env bash
# PkgTool for scripts/pkg_install.py (the launcher's "Install from .pkg"), when running from
# source: LibOrbisPkg 0.2.231 by Maxton (LGPL-3.0), checked against its SHA-256. The AppImage and
# the Windows package have it already.
#   tools/fetch_pkgtool.sh            tools/pkgtool/PkgTool.Core (Linux, self-contained .NET)
#   tools/fetch_pkgtool.sh --windows  tools/pkgtool-win/PkgTool.exe (.NET Framework 4, part of
#                                     Windows 10 and 11)
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
if [[ ${1:-} == --windows ]]; then
    url=https://github.com/maxton/LibOrbisPkg/releases/download/v0.2/PkgTool-0.2.231.zip
    sum=c639e591e35c2431f68410d1771541d95f7308863638f9e3a6eedf1818530097
    dir=tools/pkgtool-win files=(PkgTool.exe LibOrbisPkg.dll LICENSE.txt README.md)
else
    url=https://github.com/maxton/LibOrbisPkg/releases/download/v0.2/PkgTool.Core-linux-x64-0.2.231.zip
    sum=ac897d06a0c90256f4297712cb503eb29173fbc6d7f8a936ff476c4b72e5f36e
    dir=tools/pkgtool files=(PkgTool.Core LICENSE.txt)
fi
zip=$(mktemp --suffix=.zip)
trap 'rm -f "$zip"' EXIT
curl -fsSL -o "$zip" "$url"
echo "$sum  $zip" | sha256sum -c --quiet
mkdir -p "$dir"
unzip -o -q "$zip" "${files[@]}" -d "$dir"
[[ -f $dir/PkgTool.Core ]] && chmod +x "$dir/PkgTool.Core"
echo "$dir"
