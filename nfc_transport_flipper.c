#include "nfc_transport.h"
#include <furi.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <toolbox/bit_buffer.h>

#define NFC_TRANSPORT_BUF_SIZE 256

typedef enum {
    TransportCmdTxRx,
    TransportCmdClose,
} TransportCmd;

typedef struct {
    Nfc* nfc;
    NfcPoller* poller;
    FuriSemaphore* sem_ready;
    FuriSemaphore* sem_txrx;
    FuriMessageQueue* cmd_queue;
    BitBuffer* tx_buf;
    BitBuffer* rx_buf;
    bool card_ready;
    bool txrx_ok;
    uint8_t uid[10];
    uint8_t uid_len;
} TransportImpl;

static NfcCommand transport_poller_callback(NfcGenericEvent event, void* context) {
    TransportImpl* impl = context;
    Iso14443_4aPollerEvent* e = event.event_data;

    if(e->type == Iso14443_4aPollerEventTypeError) {
        impl->card_ready = false;
        furi_semaphore_release(impl->sem_ready);
        return NfcCommandStop;
    }

    if(e->type == Iso14443_4aPollerEventTypeReady) {
        if(!impl->card_ready) {
            // First activation — extract UID
            impl->card_ready = true;
            const Iso14443_4aData* data =
                (const Iso14443_4aData*)nfc_poller_get_data(impl->poller);
            size_t uid_len = 0;
            const uint8_t* uid =
                iso14443_4a_get_uid(data, &uid_len);
            if(uid_len > sizeof(impl->uid)) uid_len = sizeof(impl->uid);
            memcpy(impl->uid, uid, uid_len);
            impl->uid_len = (uint8_t)uid_len;
            furi_semaphore_release(impl->sem_ready);
        }

        // Wait for command from main thread
        TransportCmd cmd;
        if(furi_message_queue_get(impl->cmd_queue, &cmd, FuriWaitForever) !=
           FuriStatusOk) {
            return NfcCommandStop;
        }

        if(cmd == TransportCmdClose) {
            return NfcCommandStop;
        }

        // cmd == TransportCmdTxRx — send the block
        Iso14443_4aError err = iso14443_4a_poller_send_block(
            event.instance, impl->tx_buf, impl->rx_buf);
        impl->txrx_ok = (err == Iso14443_4aErrorNone);
        furi_semaphore_release(impl->sem_txrx);
        return NfcCommandContinue;
    }

    return NfcCommandContinue;
}

bool nfc_transport_open(
    NfcTransport* nfc,
    NfcTagInfo* tag,
    uint32_t timeout_ms) {
    TransportImpl* impl = malloc(sizeof(TransportImpl));
    memset(impl, 0, sizeof(TransportImpl));

    impl->nfc = nfc_alloc();
    impl->poller = nfc_poller_alloc(impl->nfc, NfcProtocolIso14443_4a);
    impl->sem_ready = furi_semaphore_alloc(1, 0);
    impl->sem_txrx = furi_semaphore_alloc(1, 0);
    impl->cmd_queue = furi_message_queue_alloc(1, sizeof(TransportCmd));
    impl->tx_buf = bit_buffer_alloc(NFC_TRANSPORT_BUF_SIZE);
    impl->rx_buf = bit_buffer_alloc(NFC_TRANSPORT_BUF_SIZE);

    nfc_poller_start(impl->poller, transport_poller_callback, impl);

    FuriStatus status = furi_semaphore_acquire(impl->sem_ready, timeout_ms);

    if(status != FuriStatusOk || !impl->card_ready) {
        // Timeout or error — if callback is blocking on cmd_queue, unblock it
        if(impl->card_ready) {
            TransportCmd cmd = TransportCmdClose;
            furi_message_queue_put(impl->cmd_queue, &cmd, FuriWaitForever);
        }
        nfc_poller_stop(impl->poller);
        nfc_poller_free(impl->poller);
        nfc_free(impl->nfc);
        bit_buffer_free(impl->tx_buf);
        bit_buffer_free(impl->rx_buf);
        furi_semaphore_free(impl->sem_ready);
        furi_semaphore_free(impl->sem_txrx);
        furi_message_queue_free(impl->cmd_queue);
        free(impl);
        return false;
    }

    // Card activated — fill tag info
    if(tag) {
        tag->uid_len = impl->uid_len;
        memcpy(tag->uid, impl->uid, impl->uid_len);
    }
    nfc->impl = impl;
    return true;
}

bool nfc_transport_txrx(
    NfcTransport* nfc,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t* rx_len,
    uint32_t timeout_ms) {
    TransportImpl* impl = nfc->impl;
    if(!impl || !impl->card_ready) return false;

    bit_buffer_reset(impl->tx_buf);
    bit_buffer_copy_bytes(impl->tx_buf, tx, tx_len);
    bit_buffer_reset(impl->rx_buf);

    TransportCmd cmd = TransportCmdTxRx;
    furi_message_queue_put(impl->cmd_queue, &cmd, FuriWaitForever);

    FuriStatus status = furi_semaphore_acquire(impl->sem_txrx, timeout_ms);
    if(status != FuriStatusOk) return false;
    if(!impl->txrx_ok) return false;

    size_t received = bit_buffer_get_size_bytes(impl->rx_buf);
    if(rx && rx_len) {
        if(received > *rx_len) received = *rx_len;
        memcpy(rx, bit_buffer_get_data(impl->rx_buf), received);
        *rx_len = received;
    }
    return true;
}

void nfc_transport_close(NfcTransport* nfc) {
    TransportImpl* impl = nfc->impl;
    if(!impl) return;

    if(impl->card_ready) {
        TransportCmd cmd = TransportCmdClose;
        furi_message_queue_put(impl->cmd_queue, &cmd, FuriWaitForever);
    }
    nfc_poller_stop(impl->poller);
    nfc_poller_free(impl->poller);
    nfc_free(impl->nfc);
    bit_buffer_free(impl->tx_buf);
    bit_buffer_free(impl->rx_buf);
    furi_semaphore_free(impl->sem_ready);
    furi_semaphore_free(impl->sem_txrx);
    furi_message_queue_free(impl->cmd_queue);
    free(impl);
    nfc->impl = NULL;
}
