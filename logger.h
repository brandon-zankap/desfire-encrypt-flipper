#pragma once
#include "nfc_transport.h"
#include <stdint.h>
#include <stdbool.h>
void akuvox_log_result(const NfcTagInfo* tag, uint32_t cid, bool ok, const char* message);
