/* ladder_engine.h - portable ladder engine (no HAL, no OS). Spec: SPEC.md
 * Same code runs on the STM32F103 (PLC) and inside the Qt simulator.
 * Call ld_scan() once every 10 ms: timers count scans. */
#ifndef LADDER_ENGINE_H
#define LADDER_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Image format */
#define LD_MAGIC0        0x4C            /* 'L' */
#define LD_MAGIC1        0x44            /* 'D' */
#define LD_VERSION       1
#define LD_HEADER_SIZE   8
#define LD_INSTR_SIZE    8
#define LD_MAX_INSTR     512
#define LD_ELEM_NONE     0xFF            /* instruction without animation id */
#define LD_STACK_DEPTH   8               /* 2 entries per branch level */
#define LD_MAX_BRANCH    4

/* Bit address space (same numbers as Modbus coil / discrete input addresses) */
#define LD_BIT_I         0x0000          /* I0..I7   inputs            */
#define LD_BIT_Q         0x0100          /* Q0..Q5   outputs           */
#define LD_BIT_M         0x0200          /* M0..M63  internal bits     */
#define LD_BIT_TDN       0x0300          /* T0..T15  done bits         */
#define LD_BIT_CDN       0x0310          /* C0..C15  done bits         */
#define LD_BIT_VFD_ST    0x0400          /* VFD status bits (from PLC) */
#define LD_BIT_VFD_CT    0x0410          /* VFD control bits (to PLC)  */
#define LD_BIT_SPACE     0x0420

/* Register address space (same numbers as PLC holding registers) */
#define LD_REG_D         0x0010          /* D0..D15                    */
#define LD_REG_T         0x0020          /* T0..T15 accumulators (x10 ms) */
#define LD_REG_C         0x0030          /* C0..C15 accumulators       */
#define LD_REG_VFD_H     0x0100          /* mirror of VFD holding regs */
#define LD_REG_VFD_I     0x0200          /* mirror of VFD input regs   */
#define LD_REG_SPACE     0x0210

#define LD_NUM_TIMERS    16
#define LD_NUM_COUNTERS  16
#define LD_NUM_ELEM      256

enum {
    LD_OP_NOP = 0x00,
    LD_OP_RUNG_BEGIN = 0x01,
    LD_OP_XIC = 0x02,
    LD_OP_XIO = 0x03,
    LD_OP_BR_OPEN = 0x04,
    LD_OP_BR_NEXT = 0x05,
    LD_OP_BR_CLOSE = 0x06,
    LD_OP_OTE = 0x10,
    LD_OP_TON = 0x11,
    LD_OP_TOF = 0x12,
    LD_OP_TONR = 0x13,
    LD_OP_CTU = 0x14,
    LD_OP_RES = 0x15,
    LD_OP_GRT = 0x20,
    LD_OP_LES = 0x21,
    LD_OP_EQU = 0x22,
    LD_OP_MOV = 0x30
};

#define LD_MODE_A_CONST  0x01            /* operand A is a constant      */
#define LD_MODE_B_CONST  0x02            /* operand B is a constant      */
#define LD_MODE_RES_CTR  0x01            /* RES targets a counter        */

typedef enum {
    LD_OK = 0,
    LD_ERR_SIZE,
    LD_ERR_MAGIC,
    LD_ERR_VERSION,
    LD_ERR_CRC,
    LD_ERR_OPCODE,
    LD_ERR_ADDR,
    LD_ERR_STRUCT,
    LD_ERR_NOPROG
} ld_status_t;

typedef struct {
    uint8_t  op;
    uint8_t  elem;
    uint8_t  mode;
    uint8_t  rsv;
    uint16_t a;
    uint16_t b;
} ld_instr_t;

typedef struct {
    uint8_t    bits[LD_BIT_SPACE];       /* 0 or 1 */
    uint16_t   regs[LD_REG_SPACE];
    uint8_t    elem_power[LD_NUM_ELEM];  /* power flow after each element */
    uint8_t    ctr_prev[LD_NUM_COUNTERS];
    ld_instr_t prog[LD_MAX_INSTR];
    uint16_t   n_instr;
    uint8_t    loaded;
} ld_engine_t;

void        ld_init(ld_engine_t *e);
/* Validates the whole image first; the running program is untouched on error. */
ld_status_t ld_load(ld_engine_t *e, const uint8_t *image, size_t len);
ld_status_t ld_scan(ld_engine_t *e);
uint16_t    ld_crc16(const uint8_t *data, size_t len);   /* Modbus CRC-16 */

#ifdef __cplusplus
}
#endif
#endif
