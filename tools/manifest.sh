#!/bin/bash
#
# Print upstream url and commit for every git repository under root directory
#
# usage: tools/manifest.sh [root ...]
#   default root: current directory.

ROOTS=("${@:-.}")

find "${ROOTS[@]}" -type d -name .git -printf '%d %p\n' 2>/dev/null \
    | sort -n -k1,1 -k2 | cut -d' ' -f2- \
    | while read -r gitdir; do
    repo="$(dirname "$gitdir")"
    url="$(git -C "$repo" remote get-url origin 2>/dev/null)"
    sha="$(git -C "$repo" rev-parse HEAD 2>/dev/null)"
    [ -z "$url" ] && continue
    [ -z "$sha" ] && continue

    printf 'git clone %s %s && git -C %s checkout %s\n' "$url" "$repo" "$repo" "$sha"
done
