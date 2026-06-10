#pragma once
#include "app_config.h"
#include "nfc_transport.h"

typedef enum {
    DesfireResultOk = 0,
    DesfireResultTransport,
    DesfireResultSelectAppFailed,
    DesfireResultCreateAppFailed,
    DesfireResultCreateFileFailed,
    DesfireResultAuthFailed,
    DesfireResultChangeKeyFailed,
    DesfireResultWriteFailed,
    DesfireResultVerifyFailed,
    DesfireResultAlreadyPersonalised,
} DesfireResult;

DesfireResult desfire_akuvox_write_cid(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint32_t cid);

DesfireResult desfire_akuvox_read_data(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint8_t* data_out,
    size_t len);

const char* desfire_result_name(DesfireResult r);

DesfireResult desfire_akuvox_personalise(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint32_t cid);

DesfireResult desfire_akuvox_read_cid(
    NfcTransport* nfc,
    const DesfireSeqConfig* cfg,
    uint32_t* cid_out);
