/**
 * @file test_host_comms.c
 * @brief Unit tests for LogicElements host communications library.
 */

#include "le_host_comms.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int g_passed = 0;
static int g_failed = 0;

#define TEST_CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] Line %d: %s\n", __LINE__, msg); \
        g_failed++; \
    } else { \
        g_passed++; \
    } \
} while (0)

static void test_crc16(void)
{
    printf("Running test_crc16...\n");
    // Standard CCITT 0x1021 test vector
    const uint8_t data[] = "123456789";
    uint16_t crc = 0xFFFF;
    crc = le_host_crc16(crc, data, 9);
    // CCITT with init 0xFFFF for "123456789" is 0x29B1
    TEST_CHECK(crc == 0x29B1, "CRC16 CCITT for '123456789' must match 0x29B1");
}

static void test_packet_framing(void)
{
    printf("Running test_packet_framing...\n");
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t cmd = LE_CMD_PING;
    uint8_t seq = 1;
    uint16_t len = sizeof(payload);

    uint8_t header[5];
    header[0] = LE_COMMS_SYNC_BYTE;
    header[1] = cmd;
    header[2] = seq;
    header[3] = (uint8_t)(len & 0xFF);
    header[4] = (uint8_t)((len >> 8) & 0xFF);

    uint16_t crc = 0xFFFF;
    crc = le_host_crc16(crc, &header[1], 4);
    crc = le_host_crc16(crc, payload, len);

    TEST_CHECK(header[0] == 0xAA, "Sync byte must be 0xAA");
    TEST_CHECK(header[1] == LE_CMD_PING, "Command byte must match PING");
    TEST_CHECK(header[2] == 1, "Seq must match 1");
    TEST_CHECK(crc != 0, "CRC must be non-zero");
}

int main(void)
{
    printf("============================================================\n");
    printf(" LOGICELEMENTS HOST COMMS UNIT TESTS\n");
    printf("============================================================\n");

    test_crc16();
    test_packet_framing();

    printf("============================================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
