#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
} NfcTagInfo;

typedef struct {
    void* impl;
} NfcTransport;

bool nfc_transport_open(NfcTransport* nfc, NfcTagInfo* tag, uint32_t timeout_ms);
bool nfc_transport_txrx(NfcTransport* nfc, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t* rx_len, uint32_t timeout_ms);
void nfc_transport_close(NfcTransport* nfc);
