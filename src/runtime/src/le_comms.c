/**
 * @file le_comms.c
 * @brief Implementation of lightweight UART packet protocol.
 */

#include "le_comms.h"
#include "le_loader.h"
#include "le_storage.h"
#include <string.h>

enum {
    RX_STATE_SYNC = 0,
    RX_STATE_CMD,
    RX_STATE_SEQ,
    RX_STATE_LEN_LO,
    RX_STATE_LEN_HI,
    RX_STATE_PAYLOAD,
    RX_STATE_CRC_LO,
    RX_STATE_CRC_HI
};

static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static void send_packet(uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t len)
{
    if (!g_le_hal || !g_le_hal->uart_write) return;

    uint8_t header[5];
    header[0] = LE_COMMS_SYNC_BYTE;
    header[1] = cmd;
    header[2] = seq;
    header[3] = (uint8_t)(len & 0xFF);
    header[4] = (uint8_t)((len >> 8) & 0xFF);

    uint16_t crc = 0xFFFF;
    for (int i = 1; i < 5; i++) {
        crc = crc16_update(crc, header[i]);
    }
    for (uint16_t i = 0; i < len; i++) {
        crc = crc16_update(crc, payload[i]);
    }

    uint8_t crc_bytes[2];
    crc_bytes[0] = (uint8_t)(crc & 0xFF);
    crc_bytes[1] = (uint8_t)((crc >> 8) & 0xFF);

    g_le_hal->uart_write(header, 5);
    if (len > 0 && payload) {
        g_le_hal->uart_write(payload, len);
    }
    g_le_hal->uart_write(crc_bytes, 2);
}

void le_comms_init(le_comms_t* comms, le_vm_t* vm, le_storage_t* storage)
{
    if (!comms) return;
    memset(comms, 0, sizeof(le_comms_t));
    comms->vm = vm;
    comms->storage = storage;
    comms->rx_state = RX_STATE_SYNC;
}

static void handle_packet(le_comms_t* comms)
{
    uint8_t cmd = comms->rx_cmd;
    uint8_t seq = comms->rx_seq;
    uint8_t* p = comms->rx_payload;
    uint16_t len = comms->rx_len;

    switch (cmd)
    {
        case LE_CMD_PING: {
            /* Reply with PONG: [status=OK, version=1, capacity_info...] */
            uint8_t pong[8];
            pong[0] = 0x00; /* status OK */
            pong[1] = LE_BIN_VERSION;
            pong[2] = LE_MAX_DIGITAL_IN;
            pong[3] = LE_MAX_DIGITAL_OUT;
            pong[4] = (uint8_t)(LE_MAX_BOOL_REGS & 0xFF);
            pong[5] = (uint8_t)((LE_MAX_BOOL_REGS >> 8) & 0xFF);
            pong[6] = LE_MAX_FLOATS;
            pong[7] = (uint8_t)(LE_RAM_WORKSPACE_BYTES & 0xFF); /* unified RAM workspace (low byte) */
            send_packet(LE_CMD_PONG, seq, pong, sizeof(pong));
            break;
        }

        case LE_CMD_GET_CAPS: {
            le_caps_payload_t caps;
            memset(&caps, 0, sizeof(caps));
            caps.protocol_version = 3;
            caps.firmware_major = 1;
            caps.firmware_minor = 0;

            const char* plat = (g_le_hal && g_le_hal->get_platform_name) ?
                                g_le_hal->get_platform_name() : "Generic MCU";
            strncpy(caps.platform_name, plat, sizeof(caps.platform_name) - 1);

            caps.max_digital_in = LE_MAX_DIGITAL_IN;
            caps.max_digital_out = LE_MAX_DIGITAL_OUT;
#if LE_ENABLE_ANALOG
            caps.max_analog_in = LE_MAX_ANALOG_IN;
#else
            caps.max_analog_in = 0;
#endif
            caps.max_bool_regs = LE_MAX_BOOL_REGS;
            caps.max_floats = LE_MAX_FLOATS;
            /* The caps field is 16-bit; the Pico 2 W workspace (256 KB) exceeds it.
             * Clamp the reported value - the authoritative capacity is in the
             * board .leconfig (and the loader enforces the real LE_RAM_WORKSPACE_BYTES). */
            caps.workspace_bytes = (LE_RAM_WORKSPACE_BYTES > 0xFFFFu) ? 0xFFFFu : (uint16_t)LE_RAM_WORKSPACE_BYTES;
            caps.config_slots = LE_MAX_CONFIG_SLOTS;
            caps.slot_size_bytes = LE_SLOT_SIZE_BYTES;

            uint16_t flags = 0;
#if LE_ENABLE_COMPLEX
            flags |= LE_CAP_COMPLEX;
#endif
#if LE_ENABLE_ANALOG
            flags |= LE_CAP_ANALOG;
#endif
#if LE_ENABLE_DSP
            flags |= LE_CAP_DSP;
#endif
#if LE_ENABLE_PROTECTION
            flags |= LE_CAP_PROTECTION;
#endif
#if LE_ENABLE_SERIAL_BUS
            flags |= LE_CAP_SERIAL_BUS;
            flags |= LE_CAP_I2C;
            flags |= LE_CAP_SPI;
#endif
            caps.feature_flags = flags;

            send_packet(LE_CMD_CAPS_DATA, seq, (const uint8_t*)&caps, sizeof(caps));
            break;
        }

        case LE_CMD_GET_CUSTOM_NODES: {
            const char* json_str = (g_le_hal && g_le_hal->get_custom_nodes_json) ?
                                   g_le_hal->get_custom_nodes_json() : "[]";
            if (!json_str) {
                json_str = "[]";
            }
            uint16_t str_len = (uint16_t)strlen(json_str);
            send_packet(LE_CMD_CUSTOM_NODES_DATA, seq, (const uint8_t*)json_str, str_len);
            break;
        }

        case LE_CMD_PROG_BEGIN: {
            /* Payload: [total_size: uint32_t][target_slot: uint8_t] */
            if (len < 5 || !comms->storage) {
                uint8_t nack[1] = {0x01}; /* no storage / malformed */
                send_packet(LE_CMD_NACK, seq, nack, 1);
                break;
            }
            uint32_t total = (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
            uint8_t  tslot = p[4];
            uint8_t  err = 0x00;
            if (tslot >= LE_MAX_CONFIG_SLOTS) {
                err = 0x07; /* invalid target slot */
            } else if (tslot == le_storage_get_active_slot(comms->storage)) {
                err = 0x06; /* cannot program the active slot */
            } else if (total > LE_SLOT_SIZE_BYTES) {
                err = 0x01; /* too large */
            }
            if (err != 0x00) {
                uint8_t nack[1] = {err};
                send_packet(LE_CMD_NACK, seq, nack, 1);
                break;
            }
            comms->target_slot = tslot;
            comms->expected_size = (size_t)total;
            comms->bytes_received = 0;
            comms->transfer_active = 1;
            uint8_t ack[1] = {0x00};
            send_packet(LE_CMD_ACK, seq, ack, 1);
            break;
        }

        case LE_CMD_PROG_CHUNK: {
            /* Payload: [offset: uint16_t] [data...] — streamed into the phantom slot. */
            if (!comms->transfer_active || !comms->storage) {
                uint8_t nack[1] = {0x03}; /* no active transfer */
                send_packet(LE_CMD_NACK, seq, nack, 1);
                break;
            }
            if (len < 2) {
                break;
            }
            uint16_t offset = (uint16_t)(p[0] | (p[1] << 8));
            uint16_t data_len = (uint16_t)(len - 2);
            bool ok = false;
            if ((uint32_t)offset + data_len <= LE_SLOT_SIZE_BYTES) {
                ok = le_storage_write_chunk(comms->storage,
                                            le_storage_get_phantom_slot(comms->storage),
                                            offset, &p[2], data_len);
            }
            if (!ok) {
                uint8_t nack[1] = {0x02}; /* out of bounds */
                send_packet(LE_CMD_NACK, seq, nack, 1);
                break;
            }
            if (offset + data_len > comms->bytes_received) {
                comms->bytes_received = offset + data_len;
            }
            uint8_t ack[1] = {0x00};
            send_packet(LE_CMD_ACK, seq, ack, 1);
            break;
        }

        case LE_CMD_PROG_END: {
            /* Validate the staged phantom (full CRC32) and atomically commit to
             * the target slot. A bad or truncated transmission never touches the
             * target config. Activation remains a separate, explicit step. */
            if (!comms->transfer_active || !comms->storage) {
                uint8_t nack[1] = {0x03}; /* no program staged */
                send_packet(LE_CMD_NACK, seq, nack, 1);
                break;
            }
            uint8_t tslot = comms->target_slot;
            comms->transfer_active = 0; /* one-shot transfer */
            if (comms->bytes_received != comms->expected_size) {
                uint8_t nack[1] = {0x04}; /* truncated transfer */
                send_packet(LE_CMD_NACK, seq, nack, 1);
                break;
            }
            le_status_t status = le_storage_commit_upload(comms->storage, tslot, comms->expected_size);
            if (status == LE_OK) {
                uint8_t ack[1] = {tslot};
                send_packet(LE_CMD_ACK, seq, ack, 1);
            } else {
                uint8_t nack[2] = {0x05, (uint8_t)(-status)}; /* validation/commit failed */
                send_packet(LE_CMD_NACK, seq, nack, 2);
            }
            break;
        }

        case LE_CMD_GET_IMAGE: {
            /* Send snapshot of the process-image bit regions (%IN, %OUT, %B).
             * The bit bucket occupies the FRONT of the register arena (DIN bits,
             * then DOUT, then BOOL), so a single truncating copy suffices. */
            if (comms->vm) {
                uint8_t img_buf[32];
                uint32_t want_bool = (comms->vm->image.bool_count < 128u)
                                     ? comms->vm->image.bool_count : 128u;
                uint32_t total_bits = (uint32_t)comms->vm->image.din_count +
                                      (uint32_t)comms->vm->image.dout_count + want_bool;
                size_t total = (total_bits + 7u) / 8u;
                if (total > sizeof(img_buf)) total = sizeof(img_buf);

                if (comms->vm->image.regs) {
                    memcpy(&img_buf[0], comms->vm->image.regs, total);
                } else {
                    memset(&img_buf[0], 0, total);
                }

                send_packet(LE_CMD_IMAGE_DATA, seq, img_buf, (uint16_t)total);
            }
            break;
        }

        case LE_CMD_CONTROL: {
            /* Payload: [action: 1=START, 2=STOP, 3=RESET] */
            if (len >= 1 && comms->vm) {
                if (p[0] == 1) le_vm_start(comms->vm);
                else if (p[0] == 2) le_vm_stop(comms->vm);
                else if (p[0] == 3) le_vm_reset(comms->vm);
                uint8_t ack[1] = {0x00};
                send_packet(LE_CMD_ACK, seq, ack, 1);
            }
            break;
        }

        case LE_CMD_FORCE_IO: {
            /* Payload: [addr: uint16_t] [val: uint8_t] */
            if (len >= 3 && comms->vm) {
                uint16_t addr = (uint16_t)(p[0] | (p[1] << 8));
                bool val = (p[2] != 0);
                le_process_image_set_bool(&comms->vm->image, addr, val);
                uint8_t ack[1] = {0x00};
                send_packet(LE_CMD_ACK, seq, ack, 1);
            }
            break;
        }

        case LE_CMD_PULSE: {
            /* Payload: [addr: uint16_t] [duration_ms: uint32_t] */
            if (len >= 6 && comms->vm) {
                uint16_t addr = (uint16_t)(p[0] | (p[1] << 8));
                uint32_t dur = (uint32_t)(p[2] | ((uint32_t)p[3] << 8) |
                                          ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 24));
                if (le_vm_pulse(comms->vm, addr, dur) == LE_OK) {
                    uint8_t ack[1] = {0x00};
                    send_packet(LE_CMD_ACK, seq, ack, 1);
                } else {
                    uint8_t nack[1] = {0x04}; /* pulse rejected */
                    send_packet(LE_CMD_NACK, seq, nack, 1);
                }
            }
            break;
        }

        case LE_CMD_GET_TIMING: {
            /* Report the timing / achievability model (le_timing_t bytes). */
            if (comms->vm) {
                le_timing_t t;
                le_loader_timing(comms->vm, &t);
                uint8_t pl[sizeof(le_timing_t)];
                const uint8_t* tb = (const uint8_t*)&t;
                for (size_t i = 0; i < sizeof(le_timing_t); i++) pl[i] = tb[i];
                send_packet(LE_CMD_IMAGE_DATA, seq, pl, sizeof(le_timing_t));
            } else {
                uint8_t nack[1] = {0x01};
                send_packet(LE_CMD_NACK, seq, nack, 1);
            }
            break;
        }

        case LE_CMD_SELECT_SLOT: {
            /* Payload: [slot: uint8_t] — activate a stored config slot into the VM. */
            if (len >= 1 && comms->storage && comms->vm) {
                uint8_t slot = p[0];
                if (slot < LE_MAX_CONFIG_SLOTS &&
                    le_storage_activate_slot(comms->storage, slot, comms->vm)) {
                    uint8_t ack[1] = {slot};
                    send_packet(LE_CMD_ACK, seq, ack, 1);
                } else {
                    uint8_t nack[1] = {0x01}; /* invalid slot / load failed */
                    send_packet(LE_CMD_NACK, seq, nack, 1);
                }
            }
            break;
        }

        default: {
            uint8_t nack[1] = {0xFF};
            send_packet(LE_CMD_NACK, seq, nack, 1);
            break;
        }
    }
}

void le_comms_process_byte(le_comms_t* comms, uint8_t b)
{
    if (!comms) return;

    switch (comms->rx_state)
    {
        case RX_STATE_SYNC:
            if (b == LE_COMMS_SYNC_BYTE) {
                comms->rx_state = RX_STATE_CMD;
                comms->rx_crc = 0xFFFF;
            }
            break;

        case RX_STATE_CMD:
            comms->rx_cmd = b;
            comms->rx_crc = crc16_update(comms->rx_crc, b);
            comms->rx_state = RX_STATE_SEQ;
            break;

        case RX_STATE_SEQ:
            comms->rx_seq = b;
            comms->rx_crc = crc16_update(comms->rx_crc, b);
            comms->rx_state = RX_STATE_LEN_LO;
            break;

        case RX_STATE_LEN_LO:
            comms->rx_len = (uint16_t)b;
            comms->rx_crc = crc16_update(comms->rx_crc, b);
            comms->rx_state = RX_STATE_LEN_HI;
            break;

        case RX_STATE_LEN_HI:
            comms->rx_len |= (uint16_t)(b << 8);
            comms->rx_crc = crc16_update(comms->rx_crc, b);
            comms->rx_idx = 0;
            if (comms->rx_len > LE_COMMS_MAX_PAYLOAD) {
                comms->rx_state = RX_STATE_SYNC; /* overflow protection */
            } else if (comms->rx_len == 0) {
                comms->rx_state = RX_STATE_CRC_LO;
            } else {
                comms->rx_state = RX_STATE_PAYLOAD;
            }
            break;

        case RX_STATE_PAYLOAD:
            comms->rx_payload[comms->rx_idx++] = b;
            comms->rx_crc = crc16_update(comms->rx_crc, b);
            if (comms->rx_idx >= comms->rx_len) {
                comms->rx_state = RX_STATE_CRC_LO;
            }
            break;

        case RX_STATE_CRC_LO:
            if (b == (uint8_t)(comms->rx_crc & 0xFF)) {
                comms->rx_state = RX_STATE_CRC_HI;
            } else {
                comms->rx_state = RX_STATE_SYNC;
            }
            break;

        case RX_STATE_CRC_HI:
            if (b == (uint8_t)((comms->rx_crc >> 8) & 0xFF)) {
                handle_packet(comms);
            }
            comms->rx_state = RX_STATE_SYNC;
            break;

        default:
            comms->rx_state = RX_STATE_SYNC;
            break;
    }
}

void le_comms_poll(le_comms_t* comms)
{
    if (!comms || !g_le_hal || !g_le_hal->uart_available || !g_le_hal->uart_read) return;

    uint8_t buf[32];
    size_t avail = g_le_hal->uart_available();
    while (avail > 0)
    {
        size_t n = g_le_hal->uart_read(buf, (avail > sizeof(buf)) ? sizeof(buf) : avail);
        if (n == 0) break;
        for (size_t i = 0; i < n; i++) {
            le_comms_process_byte(comms, buf[i]);
        }
        avail = g_le_hal->uart_available();
    }
}

