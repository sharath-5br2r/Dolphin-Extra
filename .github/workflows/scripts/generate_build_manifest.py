#!/usr/bin/env python3
import json
import os
from datetime import datetime, timezone
from pathlib import Path


def main():
    build_json_path = Path("build.json")
    build_data = json.loads(build_json_path.read_text())

    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    entry = build_data.get("dolphin-extra", {})
    version = os.environ.get("VERSION") or entry.get("version", "")
    files_map = {}

    for asset in entry.get("assets", []):
        filename = asset["name"]
        ext = asset.get("ext", "").lstrip(".").upper()
        file_data = {
            "name": "dolphin-extra",
            "version": version,
            "appKey": "dolphin-extra",
            "appName": entry.get("display_name", "Dolphin Emulator"),
            "arch": asset.get("arch", "universal"),
            "fileType": ext,
            "originBuild": version,
            "publishedAt": now,
        }
        if asset.get("package_name"):
            file_data["packageName"] = asset["package_name"]
        for src_key, dst_key in (
            ("min_sdk", "minSdk"),
            ("version_code", "versionCode"),
            ("densities", "densities"),
            ("native_libraries", "nativeLibraries"),
        ):
            if asset.get(src_key):
                file_data[dst_key] = asset[src_key]
        files_map[filename] = file_data

    manifest = {
        "schema": 1,
        "kind": "build",
        "meta": {"build": version, "channel": "stable", "publishedAt": now},
        "files": files_map,
    }
    build_json_path.write_text(json.dumps(manifest, separators=(",", ":")) + "\n")


if __name__ == "__main__":
    main()
