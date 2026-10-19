#!/usr/bin/env bash
# Puts FILES into the GitHub release of the tag $GITHUB_REF_NAME (CI, GH_TOKEN set): the
# AppImage and the Windows workflows both run on a tag push, so whichever comes first creates the
# release (packaging/release-notes.md) and the other adds its files.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
tag=$GITHUB_REF_NAME
for attempt in 1 2 3 4 5; do
    if gh release view "$tag" >/dev/null 2>&1 ||
       gh release create "$tag" --title "Wipeport $tag" --notes-file packaging/release-notes.md; then
        gh release upload "$tag" "$@" --clobber
        exit 0
    fi
    sleep $((attempt * 10)) # the other workflow was creating it at the same moment
done
echo "release $tag: neither found nor created" >&2
exit 1
