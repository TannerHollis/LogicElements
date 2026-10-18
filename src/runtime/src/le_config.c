/**
 * @file le_config.c
 * @brief Default board configuration declarations, sector descriptors,
 *        custom node dispatcher, and target profile generator.
 */

#include "le_config.h"
#include "le_storage.h"
#include "le_custom_node.h"
#include "le_hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========================================================================== */
/* Storage Sector Descriptors (Fallback defaults)                            */
/* ========================================================================== */

LE_WEAK const le_storage_sector_t g_board_storage_sectors[LE_MAX_CONFIG_SLOTS + 1] = {
#if (LE_MAX_CONFIG_SLOTS > 0)
    { .slot_id = 0, .medium = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH,
      .phys_addr = 0, .size_bytes = LE_SLOT_SIZE_BYTES,
      .erase_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 0 : LE_STORAGE_SECTOR_SIZE,
      .page_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 1 : LE_STORAGE_PAGE_SIZE,
      .hw_sector_id = 0 },
#endif
#if (LE_MAX_CONFIG_SLOTS > 1)
    { .slot_id = 1, .medium = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH,
      .phys_addr = (uint32_t)1 * LE_SLOT_SIZE_BYTES, .size_bytes = LE_SLOT_SIZE_BYTES,
      .erase_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 0 : LE_STORAGE_SECTOR_SIZE,
      .page_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 1 : LE_STORAGE_PAGE_SIZE,
      .hw_sector_id = 0 },
#endif
#if (LE_MAX_CONFIG_SLOTS > 2)
    { .slot_id = 2, .medium = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH,
      .phys_addr = (uint32_t)2 * LE_SLOT_SIZE_BYTES, .size_bytes = LE_SLOT_SIZE_BYTES,
      .erase_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 0 : LE_STORAGE_SECTOR_SIZE,
      .page_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 1 : LE_STORAGE_PAGE_SIZE,
      .hw_sector_id = 0 },
#endif
    /* Phantom slot (index LE_MAX_CONFIG_SLOTS or LE_SLOT_PHANTOM) */
    { .slot_id = LE_SLOT_PHANTOM, .medium = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH,
      .phys_addr = (uint32_t)LE_MAX_CONFIG_SLOTS * LE_SLOT_SIZE_BYTES, .size_bytes = LE_SLOT_SIZE_BYTES,
      .erase_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 0 : LE_STORAGE_SECTOR_SIZE,
      .page_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 1 : LE_STORAGE_PAGE_SIZE,
      .hw_sector_id = 0 }
};

LE_WEAK const size_t g_board_storage_sector_count = sizeof(g_board_storage_sectors) / sizeof(g_board_storage_sectors[0]);

LE_WEAK const le_storage_sector_t g_board_nvram_sector = {
    .slot_id = 0,
    .medium = (LE_NVRAM_BACKEND == LE_NVRAM_BACKEND_RAM) ? LE_MEDIUM_RAM :
              (LE_NVRAM_BACKEND == LE_NVRAM_BACKEND_FRAM) ? LE_MEDIUM_DIRECT_NVRAM : LE_MEDIUM_INTERNAL_FLASH,
    .phys_addr = (uint32_t)(LE_MAX_CONFIG_SLOTS + 1) * LE_SLOT_SIZE_BYTES,
    .size_bytes = LE_NVRAM_TOTAL_BYTES,
    .erase_size = (LE_NVRAM_BACKEND == LE_NVRAM_BACKEND_RAM || LE_NVRAM_BACKEND == LE_NVRAM_BACKEND_FRAM) ? 0 : LE_NVRAM_BLOCK_SIZE,
    .page_size = (LE_NVRAM_BACKEND == LE_NVRAM_BACKEND_RAM || LE_NVRAM_BACKEND == LE_NVRAM_BACKEND_FRAM) ? 1 : LE_NVRAM_PAGE_SIZE,
    .hw_sector_id = 0
};

const le_storage_sector_t* le_storage_get_sector(uint8_t slot)
{
    static le_storage_sector_t s_synth_sec;
    uint8_t target_slot = (slot == LE_MAX_CONFIG_SLOTS) ? LE_SLOT_PHANTOM : slot;

    for (size_t i = 0; i < g_board_storage_sector_count; i++) {
        if (g_board_storage_sectors[i].slot_id == target_slot ||
            (target_slot == LE_SLOT_PHANTOM && g_board_storage_sectors[i].slot_id == LE_MAX_CONFIG_SLOTS)) {
            return &g_board_storage_sectors[i];
        }
    }

    /* Fallback synthesized sector */
    s_synth_sec.slot_id = slot;
    s_synth_sec.medium = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? LE_MEDIUM_RAM : LE_MEDIUM_INTERNAL_FLASH;
    s_synth_sec.phys_addr = (uint32_t)slot * LE_SLOT_SIZE_BYTES;
    s_synth_sec.size_bytes = LE_SLOT_SIZE_BYTES;
    s_synth_sec.erase_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 0 : LE_STORAGE_SECTOR_SIZE;
    s_synth_sec.page_size = (LE_STORAGE_BACKEND == LE_STORAGE_BACKEND_RAM) ? 1 : LE_STORAGE_PAGE_SIZE;
    s_synth_sec.hw_sector_id = 0;
    return &s_synth_sec;
}

/* ========================================================================== */
/* Simulation / Default Custom Node Handlers                                   */
/* ========================================================================== */

static le_status_t sim_hw_sqrt_handler(const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    (void)in_count; (void)out_count;
    float a = le_custom_get_float(img, args, 0);
    le_custom_set_float(img, args, in_count, 0, (a > 0.0f) ? sqrtf(a) : 0.0f);
    return LE_OK;
}

static le_status_t sim_scaled_add_handler(const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    (void)in_count; (void)out_count;
    float a = le_custom_get_float(img, args, 0);
    float b = le_custom_get_float(img, args, 1);
    le_custom_set_float(img, args, in_count, 0, (a + b) * 1.5f);
    return LE_OK;
}

static le_status_t sim_bool_pulse_handler(const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    (void)in_count; (void)out_count;
    bool a = le_custom_get_bool(img, args, 0);
    bool b = le_custom_get_bool(img, args, 1);
    le_custom_set_bool(img, args, in_count, 0, a && !b);
    return LE_OK;
}

static le_status_t sim_majority_handler(const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    (void)in_count; (void)out_count;
    int ones = 0;
    for (int i = 0; i < 3; i++) {
        if (le_custom_get_bool(img, args, i)) ones++;
    }
    le_custom_set_bool(img, args, in_count, 0, ones >= 2);
    return LE_OK;
}

/* Ports definition arrays */
static const le_custom_port_t s_sqrt_in[]  = { { "In", LE_PORT_TYPE_FLOAT, 0.0f } };
static const le_custom_port_t s_sqrt_out[] = { { "Out", LE_PORT_TYPE_FLOAT, 0.0f } };

static const le_custom_port_t s_add_in[]   = { { "A", LE_PORT_TYPE_FLOAT, 0.0f }, { "B", LE_PORT_TYPE_FLOAT, 0.0f } };
static const le_custom_port_t s_add_out[]  = { { "Out", LE_PORT_TYPE_FLOAT, 0.0f } };

static const le_custom_port_t s_pulse_in[] = { { "A", LE_PORT_TYPE_BOOL, 0.0f }, { "B", LE_PORT_TYPE_BOOL, 0.0f } };
static const le_custom_port_t s_pulse_out[]= { { "Out", LE_PORT_TYPE_BOOL, 0.0f } };

static const le_custom_port_t s_maj_in[]   = { { "A", LE_PORT_TYPE_BOOL, 0.0f }, { "B", LE_PORT_TYPE_BOOL, 0.0f }, { "C", LE_PORT_TYPE_BOOL, 0.0f } };
static const le_custom_port_t s_maj_out[]  = { { "Out", LE_PORT_TYPE_BOOL, 0.0f } };

LE_WEAK const le_custom_node_t g_board_custom_nodes[] = {
    {
        .func_id      = 0x81,
        .type_id      = "Sim_HW_Sqrt",
        .display_name = "HW Square Root",
        .category     = "Simulator",
        .description  = "Hardware-accelerated square root calculation",
        .inputs       = s_sqrt_in,
        .in_count     = 1,
        .outputs      = s_sqrt_out,
        .out_count    = 1,
        .handler      = sim_hw_sqrt_handler,
    },
    {
        .func_id      = 0x82,
        .type_id      = "Sim_Scaled_Add",
        .display_name = "Scaled Adder",
        .category     = "Simulator",
        .description  = "Adds two inputs and scales by 1.5",
        .inputs       = s_add_in,
        .in_count     = 2,
        .outputs      = s_add_out,
        .out_count    = 1,
        .handler      = sim_scaled_add_handler,
    },
    {
        .func_id      = 0x83,
        .type_id      = "Sim_Bool_Pulse",
        .display_name = "Boolean Pulse",
        .category     = "Simulator",
        .description  = "Boolean strobe generator (A and not B)",
        .inputs       = s_pulse_in,
        .in_count     = 2,
        .outputs      = s_pulse_out,
        .out_count    = 1,
        .handler      = sim_bool_pulse_handler,
    },
    {
        .func_id      = 0x84,
        .type_id      = "Sim_Majority_3",
        .display_name = "Majority Gate 3",
        .category     = "Simulator",
        .description  = "Two-of-three voting gate",
        .inputs       = s_maj_in,
        .in_count     = 3,
        .outputs      = s_maj_out,
        .out_count    = 1,
        .handler      = sim_majority_handler,
    }
};

LE_WEAK const size_t g_board_custom_node_count = sizeof(g_board_custom_nodes) / sizeof(g_board_custom_nodes[0]);

/* ========================================================================== */
/* Custom Node Dispatch Engine                                                */
/* ========================================================================== */

const le_custom_node_t* le_custom_nodes_find(uint8_t func_id)
{
    for (size_t i = 0; i < g_board_custom_node_count; i++) {
        uint8_t fid = g_board_custom_nodes[i].func_id;
        if (fid == func_id ||
            (fid < 0x80 && (fid + 0x80) == func_id) ||
            (fid >= 0x80 && (fid - 0x80) == func_id)) {
            return &g_board_custom_nodes[i];
        }
    }
    return NULL;
}

le_status_t le_custom_nodes_dispatch(uint8_t func_id, const uint16_t* args,
                                     int in_count, int out_count, le_process_image_t* img)
{
    if (!img) return LE_ERR_NULL_PTR;
    const le_custom_node_t* node = le_custom_nodes_find(func_id);
    if (!node) {
        return LE_ERR_UNKNOWN_OPCODE;
    }
    if (in_count < (int)node->in_count || out_count < (int)node->out_count) {
        return LE_ERR_OUT_OF_BOUNDS;
    }
    if (!node->handler) {
        return LE_OK;
    }
    return node->handler(args, in_count, out_count, img);
}

/* ========================================================================== */
/* Metadata JSON & Target Profile Generator                                   */
/* ========================================================================== */

const char* le_custom_nodes_get_json(void)
{
    static char s_custom_nodes_json_buf[2048];
    if (g_board_custom_node_count == 0) {
        return "[]";
    }

    size_t offset = 0;
    offset += snprintf(s_custom_nodes_json_buf + offset, sizeof(s_custom_nodes_json_buf) - offset, "[\n");
    for (size_t i = 0; i < g_board_custom_node_count; i++) {
        const le_custom_node_t* node = &g_board_custom_nodes[i];
        offset += snprintf(s_custom_nodes_json_buf + offset, sizeof(s_custom_nodes_json_buf) - offset,
            "  {\n"
            "    \"type_id\": \"%s\",\n"
            "    \"display_name\": \"%s\",\n"
            "    \"category\": \"%s\",\n"
            "    \"description\": \"%s\",\n"
            "    \"function_id\": %u,\n"
            "    \"inputs\": [",
            node->type_id ? node->type_id : "CUSTOM",
            node->display_name ? node->display_name : (node->type_id ? node->type_id : "Custom Node"),
            node->category ? node->category : "Custom",
            node->description ? node->description : "",
            (unsigned int)node->func_id);

        for (uint8_t j = 0; j < node->in_count; j++) {
            const le_custom_port_t* p = &node->inputs[j];
            const char* type_str = (p->type == LE_PORT_TYPE_FLOAT) ? "float" :
                                   (p->type == LE_PORT_TYPE_INT) ? "int" :
                                   (p->type == LE_PORT_TYPE_COMPLEX) ? "complex" : "bool";
            offset += snprintf(s_custom_nodes_json_buf + offset, sizeof(s_custom_nodes_json_buf) - offset,
                "%s{\"name\": \"%s\", \"type\": \"%s\", \"default\": %.1f}",
                (j > 0) ? ", " : "",
                p->name ? p->name : "in",
                type_str,
                (double)p->default_val);
        }

        offset += snprintf(s_custom_nodes_json_buf + offset, sizeof(s_custom_nodes_json_buf) - offset,
            "],\n"
            "    \"outputs\": [");

        for (uint8_t j = 0; j < node->out_count; j++) {
            const le_custom_port_t* p = &node->outputs[j];
            const char* type_str = (p->type == LE_PORT_TYPE_FLOAT) ? "float" :
                                   (p->type == LE_PORT_TYPE_INT) ? "int" :
                                   (p->type == LE_PORT_TYPE_COMPLEX) ? "complex" : "bool";
            offset += snprintf(s_custom_nodes_json_buf + offset, sizeof(s_custom_nodes_json_buf) - offset,
                "%s{\"name\": \"%s\", \"type\": \"%s\"}",
                (j > 0) ? ", " : "",
                p->name ? p->name : "out",
                type_str);
        }

        offset += snprintf(s_custom_nodes_json_buf + offset, sizeof(s_custom_nodes_json_buf) - offset,
            "]\n"
            "  }%s\n",
            (i + 1 < g_board_custom_node_count) ? "," : "");
    }
    offset += snprintf(s_custom_nodes_json_buf + offset, sizeof(s_custom_nodes_json_buf) - offset, "]\n");
    return s_custom_nodes_json_buf;
}

void le_config_stream_profile_json(le_json_write_fn write_fn, void* user_data)
{
    if (!write_fn) return;
    char buf[128];
    const char* plat = (g_le_hal && g_le_hal->get_platform_name) ? g_le_hal->get_platform_name() : "Generic MCU";

    write_fn("{\n  \"device\": {\n", user_data);
    snprintf(buf, sizeof(buf), "    \"name\": \"%s\",\n", plat); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"firmware_version\": \"%d.0\",\n", LE_BIN_VERSION); write_fn(buf, user_data);
    write_fn("    \"protocol_version\": 1\n  },\n", user_data);

    write_fn("  \"limits\": {\n", user_data);
    snprintf(buf, sizeof(buf), "    \"digital_inputs\": %d,\n", LE_MAX_DIGITAL_IN); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"digital_outputs\": %d,\n", LE_MAX_DIGITAL_OUT); write_fn(buf, user_data);
#if LE_ENABLE_ANALOG
    snprintf(buf, sizeof(buf), "    \"analog_inputs\": %d,\n", LE_MAX_ANALOG_IN); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"analog_outputs\": %d,\n", LE_MAX_ANALOG_OUT); write_fn(buf, user_data);
#endif
    snprintf(buf, sizeof(buf), "    \"workspace_bytes\": %d,\n", LE_RAM_WORKSPACE_BYTES); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"config_slots\": %d,\n", LE_MAX_CONFIG_SLOTS); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"slot_size_bytes\": %d,\n", LE_SLOT_SIZE_BYTES); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"ns_per_abstract_cycle\": %d,\n", LE_NS_PER_ABSTRACT_CYCLE); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"nvram_total_bytes\": %d,\n", LE_NVRAM_TOTAL_BYTES); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"nvram_block_size\": %d,\n", LE_NVRAM_BLOCK_SIZE); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"nvram_page_size\": %d,\n", LE_NVRAM_PAGE_SIZE); write_fn(buf, user_data);
    snprintf(buf, sizeof(buf), "    \"nvram_max_erase_cycles\": %lu\n", (unsigned long)LE_NVRAM_MAX_ERASE_CYCLES); write_fn(buf, user_data);
    write_fn("  },\n", user_data);

    write_fn("  \"features\": {\n", user_data);
#if LE_ENABLE_PROTECTION
    write_fn("    \"protection\": true,\n", user_data);
#else
    write_fn("    \"protection\": false,\n", user_data);
#endif
#if LE_ENABLE_SERIAL_BUS
    write_fn("    \"serial_bus\": true,\n", user_data);
#else
    write_fn("    \"serial_bus\": false,\n", user_data);
#endif
#if LE_ENABLE_COMPLEX
    write_fn("    \"complex\": true,\n", user_data);
#else
    write_fn("    \"complex\": false,\n", user_data);
#endif
#if LE_ENABLE_DSP
    write_fn("    \"dsp\": true\n", user_data);
#else
    write_fn("    \"dsp\": false\n", user_data);
#endif
    write_fn("  },\n", user_data);

    write_fn("  \"pin_map\": {\n"
             "    \"digital\": {\"inputs\": {}, \"outputs\": {}},\n"
             "    \"analog\": {\"inputs\": {}, \"outputs\": {}}\n"
             "  },\n", user_data);

    /* Custom Nodes */
    write_fn("  \"custom_nodes\": ", user_data);
    const char* cnodes = (g_le_hal && g_le_hal->get_custom_nodes_json) ?
                         g_le_hal->get_custom_nodes_json() : le_custom_nodes_get_json();
    write_fn(cnodes ? cnodes : "[]", user_data);
    write_fn("\n}\n", user_data);
}

typedef struct {
    char*  buf;
    size_t max_len;
    size_t written;
} buf_writer_ctx_t;

static void buf_write_chunk(const char* chunk, void* user_data)
{
    buf_writer_ctx_t* ctx = (buf_writer_ctx_t*)user_data;
    if (!ctx || !ctx->buf || ctx->written >= ctx->max_len) return;
    size_t clen = strlen(chunk);
    size_t avail = ctx->max_len - ctx->written - 1;
    size_t to_copy = (clen < avail) ? clen : avail;
    if (to_copy > 0) {
        memcpy(ctx->buf + ctx->written, chunk, to_copy);
        ctx->written += to_copy;
        ctx->buf[ctx->written] = '\0';
    }
}

size_t le_config_generate_profile_json(char* buf, size_t max_len)
{
    if (!buf || max_len == 0) return 0;
    buf_writer_ctx_t ctx = { buf, max_len, 0 };
    buf[0] = '\0';
    le_config_stream_profile_json(buf_write_chunk, &ctx);
    return ctx.written;
}
