# Akuvox DESFire EV3 CID Writer for Flipper Zero

Flipper Zero app for personalising Akuvox-compatible MIFARE DESFire EV2/EV3 cards. Writes a 4-byte CID (big-endian) into an AES-128 encrypted DESFire application file.

## Features

- **Sequential Write** — Bulk-write an incrementing CID range across multiple cards. Present a card, it writes the next CID, then waits for the next card.
- **Write One CID** — Write a single hex CID to one card (useful for replacements or testing).
- **Read Test** — Read and display the CID from an existing card to verify contents.
- **Config Editor** — View and edit all parameters (app ID, file ID, key, CID range, etc.) from within the app. Changes are saved to SD card.

## Build

Requires [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
ufbt          # build
ufbt launch   # build and install to connected Flipper
```

## Configuration

The app stores its config at:

```
/ext/apps_data/desfire_seq_writer/config.txt
```

**You do not need to create this file manually.** On first launch the app generates it with sensible defaults. You can edit settings from the Config screen in the app, or edit the file directly on the SD card.

Default values:

| Field | Default | Description |
|-------|---------|-------------|
| `app_id` | `000045` | DESFire Application ID (3 bytes, hex) |
| `file_id` | `0` | File number within the application |
| `key_index` | `0` | Key slot used for authentication (0–13) |
| `app_key` | `69696969...` (16 bytes) | AES-128 application key (hex) |
| `start_cid` | `1` | First CID for sequential writing |
| `end_cid` | `1000` | Last CID for sequential writing |
| `change_key` | `true` | Whether to change the app key from the default DESFire key during app creation |
| `decimal_cid` | `false` | Decimal CID mode — treat CID range as decimal numbers encoded into hex (see below) |

## How it works

For each card (sequential write):

1. Detect ISO 14443-4A DESFire tag.
2. Select the configured application (create it if it doesn't exist).
3. Create the encrypted data file if missing (comm mode 0x03, 8 bytes).
4. Authenticate using AES AuthenticateISO (0xAA) to establish a session key.
5. Write the CID (4 bytes big-endian, padded to 8) using AES-CBC encrypted write with CRC32 integrity.
6. Re-authenticate and read back the CID to verify.
7. Increment CID and wait for the next tag.

All crypto (AES-128-CBC, AES-CMAC for IV tracking, DESFire CRC32) is handled in `desfire_akuvox.c`. The NFC transport layer is isolated in `nfc_transport_flipper.c`.

## File structure

| File | Purpose |
|------|---------|
| `desfire_seq_writer.c` | App entry point, scene/view setup |
| `desfire_seq_writer.h` | App state, enums, includes |
| `scenes.c` / `scenes.h` | UI scenes (main menu, config, running, result, read test, write one) |
| `desfire_akuvox.c` / `.h` | DESFire protocol: auth, encrypted read/write, app/file creation |
| `aes128.c` / `.h` | AES-128 block cipher, CBC encrypt/decrypt |
| `nfc_transport_flipper.c` / `.h` | Flipper NFC poller abstraction |
| `app_config.c` / `.h` | Config load/save to SD card |

## Decimal CID mode

When **Decimal CID** is set to Yes in config, the sequential counter values are treated as decimal numbers and encoded directly into hex. This is useful when your access control system expects CIDs that read as decimal numbers.

| Counter | Decimal CID Off (hex) | Decimal CID On |
|---------|----------------------|----------------|
| 10 | `0000000A` | `00000010` |
| 69 | `00000045` | `00000069` |
| 1000 | `000003E8` | `00001000` |

When toggling this option, the Start/End CID display automatically converts between hex and decimal representation.

## Changelog

### v2.0
- **UI overhaul: Config screen** — Replaced hand-rolled widget config with native Flipper `VariableItemList`. File ID, Key Index, and Change Key use left/right arrows to toggle. App ID, App Key, and CID range open a text input on OK press.
- **UI overhaul: Write One CID** — Replaced full QWERTY keyboard with Flipper's `ByteInput` hex keypad (0–9, A–F only) for entering the 4-byte CID.
- **UI fix: Read/Test Tag** — Removed raw hex file dump that was pushing the "BACK to return" text off-screen. Cleaner layout showing just the decoded CID.
- **Decimal CID mode** — New config toggle. When enabled, sequential CID counter values are encoded as decimal-in-hex (e.g. counter 69 → `0x00000069` instead of `0x00000045`). Start/End CID display and input automatically switch between hex and decimal when toggling.
- **CID fields now hex** — Start CID and End CID config input changed from decimal to hex (matching the display format). In decimal CID mode, they switch to decimal input.

### v1.0
- **Encrypted read fix** — Implemented AES-CMAC (RFC 4493) session IV tracking. Reads now compute CMAC over the ReadData command before decrypting the response, fixing garbage decryption output.
- **Encrypted write fix** — Removed erroneous CMAC call from encrypted writes. DESFire encrypted writes use CRC32 inside the encrypted payload for integrity, not CMAC (asymmetric with reads).
- **File creation fix** — Changed `CreateStdDataFile` to use comm mode 0x03 (encrypted) and 8-byte file size, matching the Akuvox card format. Previously used comm 0x00 (plain) and 32 bytes.
- **Read size fix** — Now reads the full 8-byte encrypted file instead of only 4 bytes, preventing decryption padding errors.
- **Removed GetFileSettings** — Hardcoded known file parameters (comm=0x03, size=8) to avoid CMAC tracking complexity from the extra command/response pair after auth.
- **Re-auth pattern** — Write and read operations each re-authenticate before their critical section, ensuring a clean IV=0 state.
- **Write One CID** — New feature to write a single CID to one tag (for replacements or testing).
- **Read/Test Tag** — New feature to read and display the CID from an existing card.
- **Result screen back-navigation fix** — Pressing BACK on the Done screen now returns to the main menu instead of restarting the write loop.
- **Config auto-creation** — App generates config.txt with defaults on first launch; no manual file creation needed.