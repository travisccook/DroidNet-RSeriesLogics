# Prebuilt firmware — DroidNet RSeries Logic Engine fork

Ready-to-flash images built from this repo at commit `78f8b2e`, for flashing through the
DroidNet **Flash** tab. Machine-readable details (sha256, sizes, per-file flash settings)
are in [`manifest.json`](manifest.json).

> **NEVER FLASHED TO HARDWARE.** Compiles cleanly and verified against DroidNet's flasher
> logic only. Bench-test before field use.

The RSeries hardware exists as classic ESP32 (WLogicEngine32) and ESP32-S3 (WReactor32
2023). Both are built here — **flash the file matching your board.**

| File | Chip | Kind |
| ---- | ---- | ---- |
| `DroidNet-RSeriesLogics-esp32-78f8b2e.merged.bin` | ESP32 | Full image (bootloader+partitions+app) |
| `DroidNet-RSeriesLogics-esp32-78f8b2e.app.bin` | ESP32 | App-only (update) |
| `DroidNet-RSeriesLogics-esp32s3-78f8b2e.merged.bin` | ESP32-S3 | Full image |
| `DroidNet-RSeriesLogics-esp32s3-78f8b2e.app.bin` | ESP32-S3 | App-only (update) |

**Merged vs app-only:** the merged image is the clean/first-flash/recovery choice — it
writes the whole flash and **erases NVS** (stored WiFi/settings). The app-only image
preserves NVS but only works on a board already carrying a matching bootloader and the
`min_spiffs` partition table.

## Flashing via DroidNet

- **Classic ESP32:** upload the `esp32-…merged.bin` and **leave the flash address blank** —
  DroidNet detects the merged image and writes it at `0x0`.
- **ESP32-S3:** same — upload the `esp32s3-…merged.bin` and leave the address blank. DroidNet
  recognises the merged image by its partition table at `0x8000` (same offset on every ESP32
  variant), writes it at `0x0`, and auto-detects the S3 chip from the board. Built for a 4 MB
  module — confirm yours matches. (Older DroidNet builds, before the S3 merged-image
  detection fix, needed **Chip = ESP32-S3** and **Flash address = `0x0`** set by hand.)

## Rebuild

```bash
~/.platformio/penv/bin/pio run -e wlogicengine32     # classic ESP32
~/.platformio/penv/bin/pio run -e wlogicengine32s3   # ESP32-S3
```

Merged images are assembled with `esptool merge_bin` from `.pio/build/<env>/{bootloader,
partitions,firmware}.bin` + the framework `boot_app0.bin`.
