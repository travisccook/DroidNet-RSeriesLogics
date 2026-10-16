# Prebuilt firmware — DroidNet RSeries Logic Engine fork

Ready-to-flash images built by [`build-images.sh`](build-images.sh) from a clean tree at the commit
named `source_commit` in [`manifest.json`](manifest.json). The manifest also has each file's sha256
and size, the flash settings, and the dependency versions the build resolved.

> **Flashed to hardware once (2026-10-16):** the classic ESP32 `.app.bin` runs on the C2-B5 dome R-Series board
> (ESP32-D0WD-V3 WROVER), flashed over WiFi through the board's own Reflash page (`http://<board-ip>/firmware`,
> or `curl --data-binary @<file>.app.bin -H "Expect:" http://<board-ip>/upload/firmware`; the reply is HTTP 200
> even on failure, so check the console for `Update Success`). The ESP32-S3 images have never been flashed.
> Over USB, flash in place per the runbook (spec section 6); take a read_flash dump first.

These images carry the one native command this fork adds, `#LEBRI[F|R]<1-255>` (RAM-only brightness,
library id `rseries.cfg.brightness`; see the main [README](../README.md)), and the WDP device
announce: every 25-30 s the board sends `@WDP1 {"type":"R-Series Logics","fw":"<sha>",...}` to the
WCB on Serial1, where `<sha>` is `source_commit`. They also bring everything else the fork changes,
none of which has run on hardware either: the WiFi connect-else-AP state machine, rendering on
core 1, the Driveable-Animation Contract layer and the 96-byte Marcduino buffer.

## Which file

The RSeries hardware exists as classic ESP32 (WLogicEngine32) and ESP32-S3 (WReactor32 2023). Flash
the file for the chip the Booster's **Detect Board Only** reports.

| File (`<sha>` = `source_commit`)                  | Chip     | Use                            |
| ------------------------------------------------- | -------- | ------------------------------ |
| `DroidNet-RSeriesLogics-esp32-<sha>.app.bin`      | ESP32    | **RECOMMENDED** — app only     |
| `DroidNet-RSeriesLogics-esp32-<sha>.merged.bin`   | ESP32    | **RECOVERY ONLY** — full image |
| `DroidNet-RSeriesLogics-esp32s3-<sha>.app.bin`    | ESP32-S3 | **RECOMMENDED** — app only     |
| `DroidNet-RSeriesLogics-esp32s3-<sha>.merged.bin` | ESP32-S3 | **RECOVERY ONLY** — full image |

**App only (`.app.bin`) is the one to flash.** Upload it in the DroidNet **Flash** tab and leave the
address the tab fills in (`0x10000`); do not clear or change it. It keeps NVS: the display types,
the WiFi setup, and the Marcduino baud and pass-through all survive. With the address at `0x10000`
the Booster also blanks otadata at `0xE000`, so app0 boots even on a board that was ever OTA- or
SD-updated. It needs the `min_spiffs` partition table (nvs `0x9000`, otadata `0xE000`, app0
`0x10000`, app1 `0x1F0000`, spiffs `0x3D0000`): check that in the read_flash dump before flashing.

**The merged image (`.merged.bin`) is for recovery only.** It writes bootloader, partition table,
boot_app0 and app from `0x0`, and its NVS region (`0x9000-0xDFFF`) is all `0xFF`, so flashing it
**erases NVS**: the display types, WiFi, and the Marcduino baud and pass-through are reset. A wrong
Marcduino baud silently breaks the WCB link. On an S3, esptool 5.x may also rewrite the image header.

Never use the Droid-tab remote or batch flash for these images: it never resets otadata.

## Not bit-reproducible

`scripts/pre_extra.py` embeds a build timestamp (`FIRMWARE_FILENAME`) in every build, so a rebuild
of the same commit has a different sha256. The sha256 in the manifest identifies this one build.

The dependencies float: Reeltwo comes from git HEAD through `lib_deps` and ArduinoJson from
`^6.21.3`. The manifest's `dependencies` object records what this build resolved (`pio pkg list`),
the Dependency Graph `pio run -v` printed, and which Reeltwo copy the firmware's headers came from
(the vendored `lib/Reeltwo`, which is first on the include path). A rebuild months later may differ.

## Rebuild

```bash
bash firmware/build-images.sh
```

It refuses to run unless the tree is clean outside `firmware/` and `HEAD` contains
`src/native/NativeBrightness.h`. It clean-builds `wlogicengine32` and `wlogicengine32s3`, merges each
image with esptool 4.5.1 `merge_bin` (flash parameters kept as built; bootloader, partitions and
`boot_app0` offsets and paths read from `pio project metadata`), checks that `app.bin` equals
`merged.bin[0x10000:]`, that the partition table is `min_spiffs` and that NVS is blank in the merged
image, then replaces the images here and rewrites `manifest.json`. Commit the result on its own,
after the commit it was built from.
