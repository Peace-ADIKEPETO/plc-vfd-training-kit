/* ladder_engine.c - see SPEC.md */
#include "ladder_engine.h"
#include <string.h>

uint16_t ld_crc16(const uint8_t *d, size_t n)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
    return crc;
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static void decode(const uint8_t *p, ld_instr_t *o)
{
    o->op = p[0]; o->elem = p[1]; o->mode = p[2]; o->rsv = p[3];
    o->a = rd16(p + 4); o->b = rd16(p + 6);
}

void ld_init(ld_engine_t *e) { memset(e, 0, sizeof *e); }

/* Structure and address checks, decoding straight from the image bytes. */
static ld_status_t validate(const uint8_t *ins, uint16_t n)
{
    int depth = 0, in_rung = 0;
    for (uint16_t i = 0; i < n; i++) {
        ld_instr_t in;
        decode(ins + (size_t)i * LD_INSTR_SIZE, &in);
        switch (in.op) {
        case LD_OP_NOP:
            break;
        case LD_OP_RUNG_BEGIN:
            if (depth != 0) return LD_ERR_STRUCT;
            in_rung = 1;
            break;
        case LD_OP_BR_OPEN:
            if (!in_rung || ++depth > LD_MAX_BRANCH) return LD_ERR_STRUCT;
            break;
        case LD_OP_BR_NEXT:
            if (depth <= 0) return LD_ERR_STRUCT;
            break;
        case LD_OP_BR_CLOSE:
            if (depth <= 0) return LD_ERR_STRUCT;
            depth--;
            break;
        case LD_OP_XIC: case LD_OP_XIO: case LD_OP_OTE:
            if (!in_rung) return LD_ERR_STRUCT;
            if (in.a >= LD_BIT_SPACE) return LD_ERR_ADDR;
            break;
        case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR:
            if (!in_rung) return LD_ERR_STRUCT;
            if (in.a >= LD_NUM_TIMERS) return LD_ERR_ADDR;
            break;
        case LD_OP_CTU:
            if (!in_rung) return LD_ERR_STRUCT;
            if (in.a >= LD_NUM_COUNTERS) return LD_ERR_ADDR;
            break;
        case LD_OP_RES:
            if (!in_rung) return LD_ERR_STRUCT;
            if (in.a >= ((in.mode & LD_MODE_RES_CTR) ? LD_NUM_COUNTERS : LD_NUM_TIMERS))
                return LD_ERR_ADDR;
            break;
        case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU:
            if (!in_rung) return LD_ERR_STRUCT;
            if (!(in.mode & LD_MODE_A_CONST) && in.a >= LD_REG_SPACE) return LD_ERR_ADDR;
            if (!(in.mode & LD_MODE_B_CONST) && in.b >= LD_REG_SPACE) return LD_ERR_ADDR;
            break;
        case LD_OP_MOV:                  /* A = source, B = destination register */
            if (!in_rung) return LD_ERR_STRUCT;
            if (!(in.mode & LD_MODE_A_CONST) && in.a >= LD_REG_SPACE) return LD_ERR_ADDR;
            if (in.b >= LD_REG_SPACE) return LD_ERR_ADDR;
            break;
        default:
            return LD_ERR_OPCODE;
        }
    }
    return depth == 0 ? LD_OK : LD_ERR_STRUCT;
}

ld_status_t ld_load(ld_engine_t *e, const uint8_t *img, size_t len)
{
    if (len < LD_HEADER_SIZE) return LD_ERR_SIZE;
    if (img[0] != LD_MAGIC0 || img[1] != LD_MAGIC1) return LD_ERR_MAGIC;
    if (img[2] != LD_VERSION) return LD_ERR_VERSION;
    uint16_t n = rd16(img + 4);
    if (n > LD_MAX_INSTR || len != LD_HEADER_SIZE + (size_t)n * LD_INSTR_SIZE) return LD_ERR_SIZE;
    const uint8_t *ins = img + LD_HEADER_SIZE;
    if (ld_crc16(ins, (size_t)n * LD_INSTR_SIZE) != rd16(img + 6)) return LD_ERR_CRC;
    ld_status_t st = validate(ins, n);
    if (st != LD_OK) return st;

    for (uint16_t i = 0; i < n; i++) decode(ins + (size_t)i * LD_INSTR_SIZE, &e->prog[i]);
    e->n_instr = n;
    e->loaded = 1;
    /* new program: clear timers, counters and animation state */
    memset(e->elem_power, 0, sizeof e->elem_power);
    memset(e->ctr_prev, 0, sizeof e->ctr_prev);
    memset(&e->regs[LD_REG_T], 0, LD_NUM_TIMERS * sizeof(uint16_t));
    memset(&e->regs[LD_REG_C], 0, LD_NUM_COUNTERS * sizeof(uint16_t));
    memset(&e->bits[LD_BIT_TDN], 0, LD_NUM_TIMERS);
    memset(&e->bits[LD_BIT_CDN], 0, LD_NUM_COUNTERS);
    return LD_OK;
}

ld_status_t ld_scan(ld_engine_t *e)
{
    if (!e->loaded) return LD_ERR_NOPROG;
    uint8_t acc = 1;
    uint8_t stack[LD_STACK_DEPTH];
    int sp = 0;

    for (uint16_t i = 0; i < e->n_instr; i++) {
        const ld_instr_t *in = &e->prog[i];
        switch (in->op) {
        case LD_OP_RUNG_BEGIN: acc = 1; sp = 0; break;
        case LD_OP_XIC: acc &= e->bits[in->a]; break;
        case LD_OP_XIO: acc &= (uint8_t)!e->bits[in->a]; break;
        case LD_OP_BR_OPEN:
            stack[sp++] = acc;           /* incoming power */
            stack[sp++] = 0;             /* OR of the branches so far */
            break;
        case LD_OP_BR_NEXT:
            stack[sp - 1] |= acc;
            acc = stack[sp - 2];
            break;
        case LD_OP_BR_CLOSE:
            stack[sp - 1] |= acc;
            acc = stack[sp - 1];
            sp -= 2;
            break;
        case LD_OP_OTE: e->bits[in->a] = acc; break;
        case LD_OP_TON: {
            uint16_t *v = &e->regs[LD_REG_T + in->a];
            uint8_t *dn = &e->bits[LD_BIT_TDN + in->a];
            if (acc) { if (*v < in->b) (*v)++; *dn = (*v >= in->b); }
            else { *v = 0; *dn = 0; }
            break;
        }
        case LD_OP_TOF: {
            uint16_t *v = &e->regs[LD_REG_T + in->a];
            uint8_t *dn = &e->bits[LD_BIT_TDN + in->a];
            if (acc) { *v = 0; *dn = 1; }
            else if (*dn) { if (*v < in->b) (*v)++; if (*v >= in->b) *dn = 0; }
            break;
        }
        case LD_OP_TONR: {               /* retentive: only RES clears it */
            uint16_t *v = &e->regs[LD_REG_T + in->a];
            uint8_t *dn = &e->bits[LD_BIT_TDN + in->a];
            if (acc) { if (*v < in->b) (*v)++; *dn = (*v >= in->b); }
            break;
        }
        case LD_OP_CTU: {                /* counts rising edges of the rung */
            uint16_t *v = &e->regs[LD_REG_C + in->a];
            uint8_t *dn = &e->bits[LD_BIT_CDN + in->a];
            if (acc && !e->ctr_prev[in->a]) { if (*v < 0xFFFF) (*v)++; *dn = (*v >= in->b); }
            e->ctr_prev[in->a] = acc;
            break;
        }
        case LD_OP_RES:
            if (acc) {
                if (in->mode & LD_MODE_RES_CTR) { e->regs[LD_REG_C + in->a] = 0; e->bits[LD_BIT_CDN + in->a] = 0; }
                else { e->regs[LD_REG_T + in->a] = 0; e->bits[LD_BIT_TDN + in->a] = 0; }
            }
            break;
        case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU: {
            int16_t a = (int16_t)((in->mode & LD_MODE_A_CONST) ? in->a : e->regs[in->a]);
            int16_t b = (int16_t)((in->mode & LD_MODE_B_CONST) ? in->b : e->regs[in->b]);
            int r = (in->op == LD_OP_GRT) ? (a > b) : (in->op == LD_OP_LES) ? (a < b) : (a == b);
            acc &= (uint8_t)r;
            break;
        }
        case LD_OP_MOV:
            if (acc) e->regs[in->b] = (in->mode & LD_MODE_A_CONST) ? in->a : e->regs[in->a];
            break;
        default: break;                  /* NOP */
        }
        if (in->elem != LD_ELEM_NONE) e->elem_power[in->elem] = acc;
    }
    return LD_OK;
}
