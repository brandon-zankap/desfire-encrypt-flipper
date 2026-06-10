# Akuvox DESFire EV3 Bulk Writer for Flipper Zero

First-pass external app scaffold for bulk-personalising Akuvox-compatible MIFARE DESFire EV3 cards by writing an incrementing CID into a DESFire application file.

## Important status

This is a starter project, not a finished tested binary. Flipper's public NFC APIs change across firmware releases, and authenticated DESFire EV3 writing requires AES CMAC/session-key handling plus ISO14443-4 transceive access. The UI, config, APDU builders, logging, and command flow are included; the low-level Flipper NFC transport is isolated in `nfc_transport_flipper.c` and must be wired to the exact firmware SDK version you build against.

## Build target

- Firmware: latest official Flipper firmware
- Build tool: uFBT
- Output: `.fap` app

Install uFBT:

```bash
python3 -m pip install --upgrade ufbt
```

Build:

```bash
ufbt
```

Install to connected Flipper:

```bash
ufbt launch
```

## Config file

Create this file on the SD card:

```text
/ext/apps_data/akuvox_desfire_bulk_writer/config.txt
```

Example:

```ini
app_id=A1B2C3
file_id=01
key_id=00
key_type=AES
app_key=00000000000000000000000000000000
start_cid=1001
step=1
cid_endian=BE
create_missing=true
verify=true
```

## What the app should do per card

1. Detect ISO14443-4A DESFire EV3 card.
2. Select PICC (`000000`).
3. Create/select configured application.
4. Authenticate with configured app key.
5. Create/select configured data file.
6. Write the current CID as up to 4 bytes.
7. Read back and verify.
8. Append UID/CID/result to CSV log.
9. Increment CID and wait for the next tag.

## Security / permissions

Only use this with tags and Akuvox systems you own or are authorised to administer. Do not use this to alter third-party access credentials.
