/**
 * @file le_host_comms.c
 * @brief Host-side framed binary packet protocol and MCU communication engine.
 */

#include "le_host_comms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <conio.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

struct le_host_comms {
    le_serial_t* serial;
    uint8_t seq;
};

static uint32_t get_current_time_ms(void)
{
#if defined(_WIN32)
    return (uint32_t)GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

le_host_comms_t* le_host_open(const char* port_name, uint32_t baud_rate)
{
    le_serial_t* ser = le_serial_open(port_name, baud_rate);
    if (!ser) return NULL;

    le_host_comms_t* client = (le_host_comms_t*)malloc(sizeof(le_host_comms_t));
    if (!client) {
        le_serial_close(ser);
        return NULL;
    }
    client->serial = ser;
    client->seq = 0;
    return client;
}

void le_host_close(le_host_comms_t* client)
{
    if (!client) return;
    if (client->serial) {
        le_serial_close(client->serial);
        client->serial = NULL;
    }
    free(client);
}

uint16_t le_host_crc16(uint16_t crc, const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

int le_host_send_packet(le_host_comms_t* client, uint8_t cmd, const uint8_t* payload, uint16_t len)
{
    if (!client || !client->serial) return -1;
    if (len > 1024) return -2;

    uint8_t header[5];
    header[0] = LE_COMMS_SYNC_BYTE;
    header[1] = cmd;
    header[2] = ++client->seq;
    header[3] = (uint8_t)(len & 0xFF);
    header[4] = (uint8_t)((len >> 8) & 0xFF);

    uint16_t crc = 0xFFFF;
    crc = le_host_crc16(crc, &header[1], 4);
    if (len > 0 && payload) {
        crc = le_host_crc16(crc, payload, len);
    }

    uint8_t crc_bytes[2];
    crc_bytes[0] = (uint8_t)(crc & 0xFF);
    crc_bytes[1] = (uint8_t)((crc >> 8) & 0xFF);

    if (le_serial_write(client->serial, header, 5) != 5) return -3;
    if (len > 0 && payload) {
        if (le_serial_write(client->serial, payload, len) != (int)len) return -4;
    }
    if (le_serial_write(client->serial, crc_bytes, 2) != 2) return -5;

    return 0;
}

enum {
    PARSE_SYNC = 0,
    PARSE_CMD,
    PARSE_SEQ,
    PARSE_LEN_LO,
    PARSE_LEN_HI,
    PARSE_PAYLOAD,
    PARSE_CRC_LO,
    PARSE_CRC_HI
};

int le_host_recv_packet(
    le_host_comms_t* client,
    uint8_t* out_cmd,
    uint8_t* out_seq,
    uint8_t* out_payload,
    uint16_t max_payload_len,
    uint16_t* out_payload_len,
    uint32_t timeout_ms
) {
    if (!client || !client->serial) return -1;

    uint32_t start_ms = get_current_time_ms();
    int state = PARSE_SYNC;
    uint8_t cmd = 0, seq = 0;
    uint16_t len = 0, payload_idx = 0;
    uint16_t rx_crc = 0;

    while (get_current_time_ms() - start_ms < timeout_ms) {
        uint8_t ch = 0;
        int n = le_serial_read(client->serial, &ch, 1, 50);
        if (n <= 0) continue;

        switch (state) {
            case PARSE_SYNC:
                if (ch == LE_COMMS_SYNC_BYTE) {
                    state = PARSE_CMD;
                }
                break;

            case PARSE_CMD:
                cmd = ch;
                state = PARSE_SEQ;
                break;

            case PARSE_SEQ:
                seq = ch;
                state = PARSE_LEN_LO;
                break;

            case PARSE_LEN_LO:
                len = ch;
                state = PARSE_LEN_HI;
                break;

            case PARSE_LEN_HI:
                len |= ((uint16_t)ch << 8);
                payload_idx = 0;
                if (len > max_payload_len) {
                    state = PARSE_SYNC; // Exceeds caller buffer
                } else if (len == 0) {
                    state = PARSE_CRC_LO;
                } else {
                    state = PARSE_PAYLOAD;
                }
                break;

            case PARSE_PAYLOAD:
                if (out_payload && payload_idx < max_payload_len) {
                    out_payload[payload_idx++] = ch;
                }
                if (payload_idx >= len) {
                    state = PARSE_CRC_LO;
                }
                break;

            case PARSE_CRC_LO:
                rx_crc = ch;
                state = PARSE_CRC_HI;
                break;

            case PARSE_CRC_HI:
                rx_crc |= ((uint16_t)ch << 8);
                state = PARSE_SYNC;

                // Validate CRC
                uint8_t hdr[4] = {cmd, seq, (uint8_t)(len & 0xFF), (uint8_t)((len >> 8) & 0xFF)};
                uint16_t calc_crc = 0xFFFF;
                calc_crc = le_host_crc16(calc_crc, hdr, 4);
                if (len > 0 && out_payload) {
                    calc_crc = le_host_crc16(calc_crc, out_payload, len);
                }

                if (calc_crc == rx_crc) {
                    if (out_cmd) *out_cmd = cmd;
                    if (out_seq) *out_seq = seq;
                    if (out_payload_len) *out_payload_len = len;
                    return 0;
                }
                break;
        }
    }

    return -10; // Timeout
}

int le_host_ping(le_host_comms_t* client, uint32_t timeout_ms)
{
    if (le_host_send_packet(client, LE_CMD_PING, NULL, 0) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint8_t payload[32];
    uint16_t payload_len = 0;

    int res = le_host_recv_packet(client, &resp_cmd, &resp_seq, payload, sizeof(payload), &payload_len, timeout_ms);
    if (res != 0) return res;

    return (resp_cmd == LE_CMD_PONG) ? 0 : -2;
}

int le_host_get_caps(le_host_comms_t* client, le_caps_payload_t* out_caps, uint32_t timeout_ms)
{
    if (!out_caps) return -1;
    if (le_host_send_packet(client, LE_CMD_GET_CAPS, NULL, 0) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint16_t payload_len = 0;
    int res = le_host_recv_packet(
        client,
        &resp_cmd,
        &resp_seq,
        (uint8_t*)out_caps,
        (uint16_t)sizeof(le_caps_payload_t),
        &payload_len,
        timeout_ms
    );
    if (res != 0) return res;

    return (resp_cmd == LE_CMD_CAPS_DATA && payload_len >= sizeof(le_caps_payload_t)) ? 0 : -2;
}

int le_host_get_custom_nodes(le_host_comms_t* client, char* out_json, size_t max_json_len, uint32_t timeout_ms)
{
    if (!out_json || max_json_len == 0) return -1;
    if (le_host_send_packet(client, LE_CMD_GET_CUSTOM_NODES, NULL, 0) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint16_t payload_len = 0;
    int res = le_host_recv_packet(
        client,
        &resp_cmd,
        &resp_seq,
        (uint8_t*)out_json,
        (uint16_t)(max_json_len - 1),
        &payload_len,
        timeout_ms
    );
    if (res != 0) return res;

    if (resp_cmd == LE_CMD_CUSTOM_NODES_DATA) {
        out_json[payload_len] = '\0';
        return (int)payload_len;
    }
    return -2;
}

int le_host_get_timing(le_host_comms_t* client, le_timing_t* out_timing, uint32_t timeout_ms)
{
    if (!out_timing) return -1;
    if (le_host_send_packet(client, LE_CMD_GET_TIMING, NULL, 0) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint16_t payload_len = 0;
    int res = le_host_recv_packet(
        client,
        &resp_cmd,
        &resp_seq,
        (uint8_t*)out_timing,
        (uint16_t)sizeof(le_timing_t),
        &payload_len,
        timeout_ms
    );
    if (res != 0) return res;

    return (resp_cmd == LE_CMD_IMAGE_DATA && payload_len >= sizeof(le_timing_t)) ? 0 : -2;
}

int le_host_control(le_host_comms_t* client, uint8_t action, uint32_t timeout_ms)
{
    uint8_t payload[1] = {action};
    if (le_host_send_packet(client, LE_CMD_CONTROL, payload, 1) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint8_t ack_payload[4];
    uint16_t payload_len = 0;

    int res = le_host_recv_packet(client, &resp_cmd, &resp_seq, ack_payload, sizeof(ack_payload), &payload_len, timeout_ms);
    if (res != 0) return res;

    return (resp_cmd == LE_CMD_ACK) ? 0 : -2;
}

int le_host_activate_slot(le_host_comms_t* client, uint8_t slot, uint32_t timeout_ms)
{
    uint8_t payload[1] = {slot};
    if (le_host_send_packet(client, LE_CMD_SELECT_SLOT, payload, 1) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint8_t ack_payload[4];
    uint16_t payload_len = 0;

    int res = le_host_recv_packet(client, &resp_cmd, &resp_seq, ack_payload, sizeof(ack_payload), &payload_len, timeout_ms);
    if (res != 0) return res;

    return (resp_cmd == LE_CMD_ACK) ? 0 : -2;
}

int le_host_force_io(le_host_comms_t* client, uint16_t addr, uint8_t value, uint32_t timeout_ms)
{
    uint8_t payload[3];
    payload[0] = (uint8_t)(addr & 0xFF);
    payload[1] = (uint8_t)((addr >> 8) & 0xFF);
    payload[2] = value ? 1 : 0;

    if (le_host_send_packet(client, LE_CMD_FORCE_IO, payload, 3) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint8_t ack_payload[4];
    uint16_t payload_len = 0;

    int res = le_host_recv_packet(client, &resp_cmd, &resp_seq, ack_payload, sizeof(ack_payload), &payload_len, timeout_ms);
    if (res != 0) return res;

    return (resp_cmd == LE_CMD_ACK) ? 0 : -2;
}

int le_host_pulse(le_host_comms_t* client, uint16_t addr, uint32_t duration_ms, uint32_t timeout_ms)
{
    uint8_t payload[6];
    payload[0] = (uint8_t)(addr & 0xFF);
    payload[1] = (uint8_t)((addr >> 8) & 0xFF);
    payload[2] = (uint8_t)(duration_ms & 0xFF);
    payload[3] = (uint8_t)((duration_ms >> 8) & 0xFF);
    payload[4] = (uint8_t)((duration_ms >> 16) & 0xFF);
    payload[5] = (uint8_t)((duration_ms >> 24) & 0xFF);

    if (le_host_send_packet(client, LE_CMD_PULSE, payload, 6) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint8_t ack_payload[4];
    uint16_t payload_len = 0;

    int res = le_host_recv_packet(client, &resp_cmd, &resp_seq, ack_payload, sizeof(ack_payload), &payload_len, timeout_ms);
    if (res != 0) return res;

    return (resp_cmd == LE_CMD_ACK) ? 0 : -2;
}

int le_host_get_image(
    le_host_comms_t* client,
    uint8_t* out_buf,
    size_t max_len,
    size_t* out_len,
    uint32_t timeout_ms
) {
    if (!out_buf || max_len == 0) return -1;
    if (le_host_send_packet(client, LE_CMD_GET_IMAGE, NULL, 0) != 0) return -1;

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint16_t payload_len = 0;
    int res = le_host_recv_packet(client, &resp_cmd, &resp_seq, out_buf, (uint16_t)max_len, &payload_len, timeout_ms);
    if (res != 0) return res;

    if (resp_cmd == LE_CMD_IMAGE_DATA) {
        if (out_len) *out_len = payload_len;
        return 0;
    }
    return -2;
}

int le_host_upload_program(
    le_host_comms_t* client,
    uint8_t target_slot,
    const uint8_t* lebin_data,
    size_t lebin_size,
    uint16_t chunk_size,
    le_upload_progress_cb progress_cb,
    void* user_data,
    char* error_buf,
    size_t error_buf_size
) {
    if (!client || !lebin_data || lebin_size == 0) {
        if (error_buf) snprintf(error_buf, error_buf_size, "Invalid arguments for upload.");
        return -1;
    }

    if (chunk_size == 0 || chunk_size > 120) {
        chunk_size = 64;
    }

    // Step 1: Send LE_CMD_PROG_BEGIN with [4-byte size][1-byte target slot]
    uint8_t begin_payload[5];
    begin_payload[0] = (uint8_t)(lebin_size & 0xFF);
    begin_payload[1] = (uint8_t)((lebin_size >> 8) & 0xFF);
    begin_payload[2] = (uint8_t)((lebin_size >> 16) & 0xFF);
    begin_payload[3] = (uint8_t)((lebin_size >> 24) & 0xFF);
    begin_payload[4] = (uint8_t)target_slot;

    if (le_host_send_packet(client, LE_CMD_PROG_BEGIN, begin_payload, 5) != 0) {
        if (error_buf) snprintf(error_buf, error_buf_size, "Failed to send PROG_BEGIN packet.");
        return -2;
    }

    uint8_t resp_cmd = 0, resp_seq = 0;
    uint8_t ack_payload[16];
    uint16_t payload_len = 0;

    int res = le_host_recv_packet(client, &resp_cmd, &resp_seq, ack_payload, sizeof(ack_payload), &payload_len, 3000);
    if (res != 0 || resp_cmd != LE_CMD_ACK) {
        if (error_buf) snprintf(error_buf, error_buf_size, "Device rejected PROG_BEGIN (code: %d).", resp_cmd);
        return -3;
    }

    // Step 2: Stream chunks
    size_t offset = 0;
    uint8_t chunk_buf[128];

    while (offset < lebin_size) {
        size_t current_chunk = lebin_size - offset;
        if (current_chunk > chunk_size) current_chunk = chunk_size;

        chunk_buf[0] = (uint8_t)(offset & 0xFF);
        chunk_buf[1] = (uint8_t)((offset >> 8) & 0xFF);
        memcpy(&chunk_buf[2], &lebin_data[offset], current_chunk);

        uint16_t chunk_pkt_len = (uint16_t)(2 + current_chunk);
        if (le_host_send_packet(client, LE_CMD_PROG_CHUNK, chunk_buf, chunk_pkt_len) != 0) {
            if (error_buf) snprintf(error_buf, error_buf_size, "Failed sending chunk at offset %zu.", offset);
            return -4;
        }

        res = le_host_recv_packet(client, &resp_cmd, &resp_seq, ack_payload, sizeof(ack_payload), &payload_len, 3000);
        if (res != 0 || resp_cmd != LE_CMD_ACK) {
            if (error_buf) snprintf(error_buf, error_buf_size, "Device rejected chunk at offset %zu (cmd: %d).", offset, resp_cmd);
            return -5;
        }

        offset += current_chunk;
        if (progress_cb) {
            progress_cb(offset, lebin_size, user_data);
        }
    }

    // Step 3: Send LE_CMD_PROG_END
    if (le_host_send_packet(client, LE_CMD_PROG_END, NULL, 0) != 0) {
        if (error_buf) snprintf(error_buf, error_buf_size, "Failed to send PROG_END packet.");
        return -6;
    }

    res = le_host_recv_packet(client, &resp_cmd, &resp_seq, ack_payload, sizeof(ack_payload), &payload_len, 5000);
    if (res != 0 || resp_cmd != LE_CMD_ACK) {
        uint8_t err_code = (payload_len > 0) ? ack_payload[0] : 0;
        if (error_buf) snprintf(error_buf, error_buf_size, "Microcontroller validation/commit failed (NACK code %d).", err_code);
        return -7;
    }

    return 0;
}

void le_host_run_terminal(le_host_comms_t* client)
{
    if (!client || !client->serial) return;

    printf("\n--- LogicElements Interactive Terminal (Press Ctrl+] or Ctrl+C to exit) ---\n\n");

#if defined(_WIN32)
    int prev_mode = _setmode(_fileno(stdout), _O_BINARY);
    while (1) {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 29 || ch == 3) { // Ctrl+] or Ctrl+C
                break;
            }
            uint8_t byte = (uint8_t)ch;
            le_serial_write(client->serial, &byte, 1);
        }

        uint8_t rx_buf[64];
        int n = le_serial_read(client->serial, rx_buf, sizeof(rx_buf), 20);
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                putchar((char)rx_buf[i]);
            }
            fflush(stdout);
        }
    }
    _setmode(_fileno(stdout), prev_mode);
#else
    while (1) {
        uint8_t rx_buf[64];
        int n = le_serial_read(client->serial, rx_buf, sizeof(rx_buf), 20);
        if (n > 0) {
            write(STDOUT_FILENO, rx_buf, n);
        }
    }
#endif
    printf("\n--- Terminal session closed ---\n");
}
