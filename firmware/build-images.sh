#!/usr/bin/env bash
# Build the prebuilt DroidNet-RSeriesLogics images in firmware/ from a clean, committed tree.
# Part of the DroidNet RSeries fork. Copyright (c) 2026 Travis Cook. LGPL-2.1-only.
#
# Usage (from anywhere):  bash firmware/build-images.sh
#
#   1. Refuses unless `git status --porcelain` is empty outside firmware/, and unless HEAD
#      contains src/native/NativeBrightness.h. source_commit = `git rev-parse --short=7 HEAD`.
#   2. Clean-builds wlogicengine32 and wlogicengine32s3 with `pio run -v`, and keeps each log,
#      `pio project metadata` and `pio pkg list` under .pio/dn-build-images/ (gitignored).
#   3. Assembles each merged image with esptool 4.5.1 merge_bin, keeping the flash parameters
#      as built. The bootloader/partitions/boot_app0 paths and offsets come from
#      `pio project metadata`, so boot_app0 is the one the resolved framework ships.
#   4. Checks every image: app.bin == merged[0x10000:], the partition table decodes as
#      min_spiffs (nvs 0x9000, otadata 0xE000, app0 0x10000, app1 0x1F0000, spiffs 0x3D0000),
#      and NVS (0x9000-0xDFFF) in the merged image is all 0xFF.
#   5. Replaces every DroidNet-RSeriesLogics-*.bin in firmware/ and writes manifest.json with
#      sha256, size, source_commit and the resolved dependencies. It does not write README.md.
#
# The images are NOT bit-reproducible: scripts/pre_extra.py embeds a build timestamp
# (FIRMWARE_FILENAME) in every build, so a rebuild of the same commit has a different sha256.
set -euo pipefail

PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
PY="$HOME/.platformio/penv/bin/python"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

DIRTY="$(git status --porcelain -- . ':(exclude)firmware')"
if [ -n "$DIRTY" ]; then
  echo "REFUSED: the tree has uncommitted or untracked changes outside firmware/:"
  echo "$DIRTY"
  exit 1
fi
if ! git cat-file -e HEAD:src/native/NativeBrightness.h 2>/dev/null; then
  echo "REFUSED: HEAD does not contain src/native/NativeBrightness.h"
  exit 1
fi
SHA="$(git rev-parse --short=7 HEAD)"
ESPTOOL_VERSION="$("$PY" "$ESPTOOL" version | tail -1)"
if [ "$ESPTOOL_VERSION" != "4.5.1" ]; then
  echo "REFUSED: expected esptool 4.5.1 at $ESPTOOL, found '$ESPTOOL_VERSION'"
  exit 1
fi
PIO_VERSION="$("$PIO" --version)"

WORK="$ROOT/.pio/dn-build-images"
rm -rf "$WORK"
mkdir -p "$WORK"
echo "source_commit $SHA; logs in $WORK"

for ENV in wlogicengine32 wlogicengine32s3; do
  echo "[$ENV] clean build"
  "$PIO" run -e "$ENV" -t clean > "$WORK/$ENV.clean.log" 2>&1
  if ! "$PIO" run -v -e "$ENV" > "$WORK/$ENV.build.log" 2>&1; then
    tail -40 "$WORK/$ENV.build.log"
    echo "FAILED: pio run -e $ENV (full log: $WORK/$ENV.build.log)"
    exit 1
  fi
  "$PIO" project metadata -e "$ENV" --json-output > "$WORK/$ENV.metadata.json" 2> "$WORK/$ENV.metadata.err"
  "$PIO" pkg list -e "$ENV" > "$WORK/$ENV.pkglist.txt" 2>&1
done

# A build must not have touched a tracked file.
DIRTY="$(git status --porcelain -- . ':(exclude)firmware')"
if [ -n "$DIRTY" ]; then
  echo "FAILED: the build changed files outside firmware/:"
  echo "$DIRTY"
  exit 1
fi

"$PY" - "$ROOT" "$SHA" "$WORK" "$ESPTOOL" "$ESPTOOL_VERSION" "$PIO_VERSION" <<'PYEOF'
import datetime, glob, hashlib, json, os, re, subprocess, sys

root, sha, work, esptool, esptool_version, pio_version = sys.argv[1:7]
fw = os.path.join(root, "firmware")
home = os.path.expanduser("~")

ENVS = [
    # env, chip, mcu, bootloader offset
    ("wlogicengine32", "esp32", "ESP32 (WROOM)", 0x1000),
    ("wlogicengine32s3", "esp32s3", "ESP32-S3 (WReactor32 2023)", 0x0),
]
APP_OFFSET = 0x10000
MIN_SPIFFS = {"nvs": 0x9000, "otadata": 0xE000, "app0": 0x10000, "app1": 0x1F0000, "spiffs": 0x3D0000}


def die(msg):
    print("FAILED: " + msg)
    sys.exit(1)


def tidy(text):
    # Paths in the manifest are relative to the repo or to ~, never this Mac's absolute paths.
    return text.replace(root + "/", "").replace(home, "~")


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def dependency_graph(log):
    lines, inside = [], False
    for line in open(log, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        if line.strip() == "Dependency Graph":
            inside = True
            continue
        if inside:
            if not line.startswith("|"):
                break
            lines.append(tidy(line))
    if not lines:
        die("no Dependency Graph in " + log)
    return lines


def pkg(lines, name):
    for line in lines:
        m = re.search(r"\b" + re.escape(name) + r" @ (\S+)", line)
        if m:
            return m.group(1)
    die(f"{name} not in pio pkg list")


# Remove the previous images; manifest.json is rewritten below.
for old in glob.glob(os.path.join(fw, "DroidNet-RSeriesLogics-*.bin")):
    os.remove(old)

artifacts, dependencies = [], {}
for env, chip, mcu, boot_offset in ENVS:
    meta = json.load(open(os.path.join(work, env + ".metadata.json")))[env]
    images = {int(i["offset"], 16): i["path"] for i in meta["extra"]["flash_images"]}
    if int(meta["extra"]["application_offset"], 16) != APP_OFFSET:
        die(f"{env}: application offset is {meta['extra']['application_offset']}")
    if sorted(images) != sorted([boot_offset, 0x8000, 0xE000]):
        die(f"{env}: unexpected flash image offsets {[hex(o) for o in sorted(images)]}")
    boot_app0 = images[0xE000]
    if not boot_app0.endswith("/tools/partitions/boot_app0.bin"):
        die(f"{env}: unexpected boot_app0 path {boot_app0}")
    build = os.path.join(root, ".pio", "build", env)
    app_src = os.path.join(build, "firmware.bin")

    base = f"DroidNet-RSeriesLogics-{chip}-{sha}"
    app = os.path.join(fw, base + ".app.bin")
    merged = os.path.join(fw, base + ".merged.bin")
    with open(app_src, "rb") as src, open(app, "wb") as dst:
        dst.write(src.read())
    cmd = [sys.executable, esptool, "--chip", chip, "merge_bin", "-o", merged,
           "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep",
           hex(boot_offset), images[boot_offset], "0x8000", images[0x8000],
           "0xe000", boot_app0, hex(APP_OFFSET), app_src]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)

    data_app = open(app, "rb").read()
    data_merged = open(merged, "rb").read()
    if data_merged[APP_OFFSET:] != data_app:
        die(f"{env}: app.bin != merged.bin[0x10000:]")
    if set(data_merged[0x9000:0xE000]) != {0xFF}:
        die(f"{env}: NVS region 0x9000-0xDFFF of the merged image is not blank")
    gen = os.path.join(os.path.dirname(os.path.dirname(boot_app0)), "gen_esp32part.py")
    pt = os.path.join(work, env + ".partitions.bin")
    with open(pt, "wb") as f:
        f.write(data_merged[0x8000:0x8000 + 0xC00])
    csv = subprocess.run([sys.executable, gen, pt], check=True, capture_output=True, text=True).stdout
    table = {}
    for line in csv.splitlines():
        cols = [c.strip() for c in line.split(",")]
        if len(cols) >= 4 and not line.startswith("#"):
            table[cols[0]] = int(cols[3], 16)
    if table != MIN_SPIFFS:
        die(f"{env}: partition table is not min_spiffs: {table}")

    pkglist = open(os.path.join(work, env + ".pkglist.txt"), encoding="utf-8").read().splitlines()
    first_reeltwo = next(p for p in meta["includes"]["build"] if "Reeltwo" in p)
    reeltwo_dir = os.path.dirname(first_reeltwo)
    props = open(os.path.join(reeltwo_dir, "library.properties"), encoding="utf-8").read()
    linked_version = re.search(r"^version=(\S+)", props, re.M).group(1)
    dependencies[env] = {
        "platform": "espressif32 @ " + pkg(pkglist, "Platform espressif32"),
        "framework": "framework-arduinoespressif32 @ " + pkg(pkglist, "framework-arduinoespressif32"),
        "reeltwo_git": "Reeltwo @ " + pkg(pkglist, "Reeltwo") + " (lib_deps https://github.com/reeltwo/Reeltwo, unpinned)",
        "arduinojson": "ArduinoJson @ " + pkg(pkglist, "ArduinoJson") + " (lib_deps ^6.21.3)",
        "reeltwo_linked": (f"{tidy(reeltwo_dir)} (version={linked_version}): its src/ is first on the include path, "
                           "so every Reeltwo header the firmware includes comes from it. The lib_deps git copy is "
                           "also compiled; both appear in dependency_graph."),
        "pio_pkg_list": [tidy(l) for l in pkglist if l.strip()],
        "dependency_graph": dependency_graph(os.path.join(work, env + ".build.log")),
    }

    s3 = chip == "esp32s3"
    artifacts.append({
        "file": os.path.basename(app),
        "role": f"RECOMMENDED - {'ESP32-S3' if s3 else 'classic ESP32'}, app only",
        "chip": chip, "mcu": mcu, "env": env,
        "droidnet_flash": {
            "file_type": "esp32",
            "select_chip": "the chip Detect Board Only reported",
            "flash_address": "0x10000 (auto; do not clear or change it)",
            "note": ("RECOMMENDED. Flash tab, auto 0x10000, keeps NVS (display types, WiFi, Marcduino baud and "
                     "pass-through). At 0x10000 the Booster also blanks otadata at 0xE000, so app0 boots even on a "
                     "board that was OTA- or SD-updated. Needs the min_spiffs partition table: check it in the "
                     "read_flash dump first."),
        },
        "size_bytes": len(data_app), "sha256": sha256(app),
    })
    artifacts.append({
        "file": os.path.basename(merged),
        "role": f"RECOVERY ONLY - {'ESP32-S3' if s3 else 'classic ESP32'}, full image",
        "chip": chip, "mcu": mcu, "env": env,
        "droidnet_flash": {
            "file_type": "esp32",
            "select_chip": "the chip Detect Board Only reported",
            "flash_address": "(leave blank; detected as merged, flashed at 0x0)",
            "note": ("RECOVERY ONLY. Bootloader + min_spiffs partition table + boot_app0 + app. Erases NVS "
                     "(0x9000-0xDFFF is all 0xFF), which resets the display types, WiFi, and Marcduino baud and "
                     "pass-through." + (" On an S3, esptool 5.x may also rewrite the header." if s3 else "")),
        },
        "size_bytes": len(data_merged), "sha256": sha256(merged),
    })

manifest = {
    "built": datetime.date.today().isoformat(),
    "toolchain": (f"{pio_version}; images merged with esptool {esptool_version} "
                  "(~/.platformio/packages/tool-esptoolpy)"),
    "status": ("NEVER FLASHED TO HARDWARE — built and verified against DroidNet's flasher logic only. "
               "Flash in place per the runbook (spec section 6); take a read_flash dump first."),
    "flash_via": "DroidNet web UI -> Flash tab (upload file, board on that Booster's USB).",
    "board": "RSeries Logic Engine (Reeltwo WLogicEngine32 / WReactor32-S3)",
    "source_repo": "https://github.com/travisccook/DroidNet-RSeriesLogics",
    "source_commit": sha,
    "native_commands": ["#LEBRI[F|R]<1-255> - RAM-only brightness (library id rseries.cfg.brightness)"],
    "reproducibility": ("Not bit-reproducible: scripts/pre_extra.py embeds a build timestamp (FIRMWARE_FILENAME) "
                        "in every build, so each sha256 identifies this one build, not the source commit. "
                        "Rebuild with: bash firmware/build-images.sh"),
    "artifacts": artifacts,
    "dependencies": dependencies,
}
with open(os.path.join(fw, "manifest.json"), "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2, ensure_ascii=False)
    f.write("\n")
for a in artifacts:
    print(f"{a['file']}  {a['size_bytes']} B  sha256 {a['sha256']}")
print(f"wrote firmware/manifest.json (source_commit {sha})")
PYEOF
