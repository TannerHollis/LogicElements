/**
 * @file le_comms.h
 * @brief Lightweight UART packet protocol for program upload and UI telemetry.
 */

#ifndef LE_COMMS_H
#define LE_COMMS_H

#include "le_types.h"
#include "le_vm.h"
#include "le_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Protocol framing and command opcodes                                       */
/* ========================================================================== */

/** @brief Framing synchronisation byte preceding every packet frame. */
#define LE_COMMS_SYNC_BYTE      0xAA

/** @brief Ping request sent by host. */
#define LE_CMD_PING             0x01
/** @brief Pong response emitted by runtime. */
#define LE_CMD_PONG             0x81
/** @brief Capabilities request sent by host. */
#define LE_CMD_GET_CAPS         0x02
/** @brief Capabilities payload response emitted by runtime. */
#define LE_CMD_CAPS_DATA        0x82
/** @brief Custom nodes definition request sent by host. */
#define LE_CMD_GET_CUSTOM_NODES 0x03
/** @brief Custom nodes JSON data response emitted by runtime. */
#define LE_CMD_CUSTOM_NODES_DATA 0x83
/** @brief Program transfer initiation command. */
#define LE_CMD_PROG_BEGIN       0x10
/** @brief Program payload chunk command. */
#define LE_CMD_PROG_CHUNK       0x11
/** @brief Program transfer completion and validation command. */
#define LE_CMD_PROG_END         0x12
/** @brief Generic positive acknowledgment response. */
#define LE_CMD_ACK              0x80
/** @brief Negative acknowledgment or error response. */
#define LE_CMD_NACK             0x8F
/** @brief Process image snapshot request sent by host. */
#define LE_CMD_GET_IMAGE        0x20
/** @brief Process image telemetry payload emitted by runtime. */
#define LE_CMD_IMAGE_DATA       0xA0
/** @brief Runtime execution control command (start, stop, reset). */
#define LE_CMD_CONTROL          0x30
/** @brief Force input or output override command for testing. */
#define LE_CMD_FORCE_IO         0x40
/** @brief Pulse a register (by process-image address) for a duration.
 *  Payload: [addr: uint16_t] [duration_ms: uint32_t]. */
#define LE_CMD_PULSE            0x41
/** @brief Query the timing / achievability report (le_timing_t bytes).
 *  Response: LE_CMD_IMAGE_DATA-style payload of sizeof(le_timing_t). */
#define LE_CMD_GET_TIMING       0x42

/* ========================================================================== */
/* Hardware feature flags                                                     */
/* ========================================================================== */

/** @brief Indicates protection and control elements are compiled and supported. */
#define LE_CAP_PROTECTION       (1U << 0)
/** @brief Indicates serial bus (I2C and SPI) subsystems are supported. */
#define LE_CAP_SERIAL_BUS       (1U << 1)
/** @brief Indicates hardware I2C master driver is available. */
#define LE_CAP_I2C              (1U << 2)
/** @brief Indicates hardware SPI master driver is available. */
#define LE_CAP_SPI              (1U << 3)
/** @brief Indicates complex registers/arithmetic (%C) are compiled in. */
#define LE_CAP_COMPLEX          (1U << 4)
/** @brief Indicates analog input channels (%AIN) are compiled in. */
#define LE_CAP_ANALOG           (1U << 5)

#pragma pack(push, 1)
/**
 * @brief Board capabilities payload returned to host tooling.
 *
 * Describes hardware constraints, memory limits, and enabled features so
 * compilers and IDEs can validate logic programs against target capabilities.
 */
typedef struct {
    uint8_t  protocol_version;       /**< Comms protocol version (currently 3). */
    uint8_t  firmware_major;         /**< Major firmware version. */
    uint8_t  firmware_minor;         /**< Minor firmware version. */
    char     platform_name[32];      /**< Null-terminated platform or board identifier string. */
    uint16_t max_digital_in;         /**< Maximum physical digital inputs supported. */
    uint16_t max_digital_out;        /**< Maximum physical digital outputs supported. */
    uint16_t max_analog_in;          /**< Maximum analog inputs supported. */
    uint16_t max_bool_regs;          /**< Maximum internal boolean memory coils (%M) supported. */
    uint16_t max_floats;             /**< Maximum floating-point registers (%R) supported. */
    uint16_t workspace_bytes;        /**< Unified RAM workspace capacity (LE_RAM_WORKSPACE_BYTES). */
    uint8_t  config_slots;           /**< Number of multi-program storage slots available. */
    uint16_t slot_size_bytes;        /**< Maximum byte capacity per configuration slot. */
    uint16_t feature_flags;          /**< Bitmask of enabled optional subsystems (@ref LE_CAP_PROTECTION, etc.). */
} le_caps_payload_t;
#pragma pack(pop)

/** @brief Maximum payload size in bytes per packet frame. */
#define LE_COMMS_MAX_PAYLOAD    128

/** @brief Size of the staging buffer for binary program uploads. */
#define LE_STAGING_BUFFER_SIZE  4096

/**
 * @brief Communications state machine instance.
 */
typedef struct {
    uint8_t   staging_buffer[LE_STAGING_BUFFER_SIZE]; /**< Staging area for incoming program upload. */
    size_t    program_size;                           /**< Declared incoming program size in bytes. */
    size_t    bytes_received;                         /**< Total program bytes accumulated in staging buffer. */

    /* RX Packet Parser State */
    uint8_t   rx_state;                               /**< Internal parser state index. */
    uint8_t   rx_cmd;                                 /**< Command opcode of the active packet. */
    uint8_t   rx_seq;                                 /**< Sequence number of the active packet. */
    uint16_t  rx_len;                                 /**< Declared payload length of active packet. */
    uint16_t  rx_idx;                                 /**< Number of payload bytes accumulated. */
    uint8_t   rx_payload[LE_COMMS_MAX_PAYLOAD];       /**< Buffer holding packet payload. */
    uint16_t  rx_crc;                                 /**< Calculated CCITT CRC-16 for incoming packet. */

    le_vm_t*  vm;                                     /**< Pointer to target virtual machine instance. */
} le_comms_t;

/**
 * @brief Initializes the communications state machine.
 *
 * @param comms Pointer to the communications structure to initialize.
 * @param vm Pointer to the target virtual machine instance.
 */
void le_comms_init(le_comms_t* comms, le_vm_t* vm);

/**
 * @brief Feeds a single received byte into the packet parser state machine.
 *
 * @param comms Pointer to the communications structure.
 * @param byte Incoming byte from UART receiver.
 */
void le_comms_process_byte(le_comms_t* comms, uint8_t byte);

/**
 * @brief Polls the UART peripheral through the HAL and processes available incoming bytes.
 *
 * @param comms Pointer to the communications structure.
 */
void le_comms_poll(le_comms_t* comms);

#ifdef __cplusplus
}
#endif

#endif /* LE_COMMS_H */
