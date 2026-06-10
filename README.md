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

## Security

Only use this with tags and Akuvox systems you own or are authorised to administer.
