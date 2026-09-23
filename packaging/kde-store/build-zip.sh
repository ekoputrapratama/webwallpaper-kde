#!/usr/bin/env bash
#
# Package the WebWallpaper KDE Plasma 6 wallpaper plugin for submission to the
# KDE Store (https://store.kde.org).
#
# Produces a zip named <archiveName>.zip at the zip root containing:
#   contents/     the QML wallpaper package (ui/, config/, themes/)
#   LICENSE
#   metadata.json (KPlugin.Id stamped to <pluginID>, legacy "webwallpaper")
#
# Usage:
#   ./packaging/kde-store/build-zip.sh                 # -> com.mixaline.webwallpaper.zip
#   ./packaging/kde-store/build-zip.sh --out DIR       # write the zip into DIR
#   ./packaging/kde-store/build-zip.sh --archive-name x  # override archive file name
#   ./packaging/kde-store/build-zip.sh --plugin-id y     # override the stamped plugin id
#
# The KDE Store archive is expected to be a flat zip: metadata.json, LICENSE
# and contents/ at the top level (no wrapping parent folder). See the reference
# archive plugin/package/com.mixaline.webwallpaper.zip.
#
# Note: PLUGIN_ID is what the store/plasma matches for updates. It is kept as
# the legacy "webwallpaper" on purpose so existing installs keep working and
# can update in place; do not switch it to a reverse-DNS id unless you accept
# breaking the update path for current users.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="${SCRIPT_DIR}/../../plugin/package"
OUT_DIR="${SCRIPT_DIR}/dist"
ARCHIVE_NAME="com.mixaline.webwallpaper"
PLUGIN_ID="webwallpaper"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --out)          OUT_DIR="${2:?--out needs a directory}"; shift 2 ;;
        --archive-name) ARCHIVE_NAME="${2:?--archive-name needs a name}"; shift 2 ;;
        --plugin-id)    PLUGIN_ID="${2:?--plugin-id needs an id}"; shift 2 ;;
        *)     echo "Usage: $0 [--out DIR] [--archive-name NAME] [--plugin-id ID]" >&2; exit 2 ;;
    esac
done

if [ ! -f "${PKG_DIR}/metadata.json" ]; then
    echo "ERROR: package directory not found at ${PKG_DIR}" >&2
    exit 1
fi

STAGE="$(mktemp -d)"
trap 'rm -rf "${STAGE}"' EXIT

echo "==> Staging package files"
cp -r "${PKG_DIR}/contents" "${STAGE}/contents"
cp "${PKG_DIR}/LICENSE" "${STAGE}/LICENSE"

# Stamp the KPlugin.Id with the legacy plugin id so existing installs keep
# working and can update in place (see note above about PLUGIN_ID).
python3 - "${PKG_DIR}/metadata.json" "${STAGE}/metadata.json" "${PLUGIN_ID}" <<'PY'
import json, sys
src, dst, plugin_id = sys.argv[1], sys.argv[2], sys.argv[3]
with open(src, encoding="utf-8") as f:
    meta = json.load(f)
meta.setdefault("KPlugin", {})["Id"] = plugin_id
meta["X-KDE-PluginInfo-Name"] = plugin_id
with open(dst, "w", encoding="utf-8") as f:
    json.dump(meta, f, indent=4, ensure_ascii=False)
    f.write("\n")
print("   metadata.json Id = " + plugin_id)
PY

mkdir -p "${OUT_DIR}"
OUT="${OUT_DIR}/${ARCHIVE_NAME}.zip"
rm -f "${OUT}"

echo "==> Zipping ${OUT}"
if command -v zip >/dev/null 2>&1; then
    (cd "${STAGE}" && zip -r "${OUT}" contents LICENSE metadata.json >/dev/null)
    ZIPTOOL_HINT=" (zip)"
else
    python3 - "${STAGE}" "${OUT}" <<'PY'
import os, sys, zipfile
stage, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    count = 0
    for root, _dirs, files in os.walk(stage):
        for name in files:
            full = os.path.join(root, name)
            arc = os.path.relpath(full, stage)
            z.write(full, arc)
            count += 1
    print(f"   {count} files (written via Python zipfile)")
PY
    ZIPTOOL_HINT=" (python zipfile)"
fi

echo "==> Done. Archive contents${ZIPTOOL_HINT:-}:"
unzip -l "${OUT}" 2>/dev/null || python3 - "${OUT}" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    for i in z.infolist():
        print(f"  {i.file_size:>9}  {i.filename}")
PY