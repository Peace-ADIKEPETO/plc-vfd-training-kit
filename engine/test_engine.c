/* test_engine.c - unit tests for the ladder engine. Build: see tests.pro or README in SPEC.md */
#include "ladder_engine.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* ---- tiny assembler ---- */
typedef struct { uint8_t buf[LD_HEADER_SIZE + LD_MAX_INSTR * LD_INSTR_SIZE]; uint16_t n; } img_t;

static void emit(img_t *im, uint8_t op, uint8_t elem, uint8_t mode, uint16_t a, uint16_t b)
{
    uint8_t *p = &im->buf[LD_HEADER_SIZE + im->n * LD_INSTR_SIZE];
    p[0] = op; p[1] = elem; p[2] = mode; p[3] = 0;
    p[4] = (uint8_t)(a & 0xFF); p[5] = (uint8_t)(a >> 8);
    p[6] = (uint8_t)(b & 0xFF); p[7] = (uint8_t)(b >> 8);
    im->n++;
}
static size_t finish(img_t *im)
{
    size_t body = (size_t)im->n * LD_INSTR_SIZE;
    uint16_t crc = ld_crc16(&im->buf[LD_HEADER_SIZE], body);
    im->buf[0] = LD_MAGIC0; im->buf[1] = LD_MAGIC1; im->buf[2] = LD_VERSION; im->buf[3] = 0;
    im->buf[4] = (uint8_t)(im->n & 0xFF); im->buf[5] = (uint8_t)(im->n >> 8);
    im->buf[6] = (uint8_t)(crc & 0xFF); im->buf[7] = (uint8_t)(crc >> 8);
    return LD_HEADER_SIZE + body;
}
#define BEGIN(im)            emit(im, LD_OP_RUNG_BEGIN, LD_ELEM_NONE, 0, 0, 0)
#define OPEN(im)             emit(im, LD_OP_BR_OPEN, LD_ELEM_NONE, 0, 0, 0)
#define NEXT(im)             emit(im, LD_OP_BR_NEXT, LD_ELEM_NONE, 0, 0, 0)
#define CLOSE(im)            emit(im, LD_OP_BR_CLOSE, LD_ELEM_NONE, 0, 0, 0)

static ld_engine_t eng;
static void scans(int n) { for (int i = 0; i < n; i++) ld_scan(&eng); }
#define I(n)  (LD_BIT_I + (n))
#define Q(n)  (LD_BIT_Q + (n))

static void load_ok(img_t *im)
{
    size_t len = finish(im);
    ld_init(&eng);
    CHECK(ld_load(&eng, im->buf, len) == LD_OK);
}

static void test_latch(void)
{
    img_t im = {0};
    BEGIN(&im); OPEN(&im);
    emit(&im, LD_OP_XIC, 0, 0, I(0), 0); NEXT(&im);
    emit(&im, LD_OP_XIC, 1, 0, Q(0), 0); CLOSE(&im);
    emit(&im, LD_OP_XIO, 2, 0, I(1), 0);
    emit(&im, LD_OP_OTE, 3, 0, Q(0), 0);
    load_ok(&im);
    scans(1);                CHECK(eng.bits[Q(0)] == 0);
    eng.bits[I(0)] = 1; scans(1); CHECK(eng.bits[Q(0)] == 1);
    eng.bits[I(0)] = 0; scans(3); CHECK(eng.bits[Q(0)] == 1);   /* sealed in */
    eng.bits[I(1)] = 1; scans(1); CHECK(eng.bits[Q(0)] == 0);   /* stop */
    eng.bits[I(1)] = 0; scans(2); CHECK(eng.bits[Q(0)] == 0);
}

static void test_ton(void)
{
    img_t im = {0};
    BEGIN(&im); emit(&im, LD_OP_XIC, 0, 0, I(0), 0); emit(&im, LD_OP_TON, 1, 0, 0, 5);
    BEGIN(&im); emit(&im, LD_OP_XIC, 2, 0, LD_BIT_TDN + 0, 0); emit(&im, LD_OP_OTE, 3, 0, Q(1), 0);
    load_ok(&im);
    eng.bits[I(0)] = 1;
    scans(4); CHECK(eng.bits[LD_BIT_TDN] == 0 && eng.bits[Q(1)] == 0 && eng.regs[LD_REG_T] == 4);
    scans(1); CHECK(eng.bits[LD_BIT_TDN] == 1 && eng.bits[Q(1)] == 1);
    scans(5); CHECK(eng.regs[LD_REG_T] == 5);                    /* saturates at preset */
    eng.bits[I(0)] = 0; scans(1);
    CHECK(eng.bits[LD_BIT_TDN] == 0 && eng.regs[LD_REG_T] == 0 && eng.bits[Q(1)] == 0);
}

static void test_tof(void)
{
    img_t im = {0};
    BEGIN(&im); emit(&im, LD_OP_XIC, 0, 0, I(0), 0); emit(&im, LD_OP_TOF, 1, 0, 1, 3);
    load_ok(&im);
    eng.bits[I(0)] = 1; scans(1); CHECK(eng.bits[LD_BIT_TDN + 1] == 1);
    eng.bits[I(0)] = 0;
    scans(1); CHECK(eng.bits[LD_BIT_TDN + 1] == 1);
    scans(1); CHECK(eng.bits[LD_BIT_TDN + 1] == 1);
    scans(1); CHECK(eng.bits[LD_BIT_TDN + 1] == 0);              /* 3rd scan after the fall */
}

static void test_tonr(void)
{
    img_t im = {0};
    BEGIN(&im); emit(&im, LD_OP_XIC, 0, 0, I(0), 0); emit(&im, LD_OP_TONR, 1, 0, 2, 5);
    BEGIN(&im); emit(&im, LD_OP_XIC, 2, 0, I(1), 0); emit(&im, LD_OP_RES, 3, 0, 2, 0);
    load_ok(&im);
    eng.bits[I(0)] = 1; scans(3);
    eng.bits[I(0)] = 0; scans(2); CHECK(eng.regs[LD_REG_T + 2] == 3);   /* retained */
    eng.bits[I(0)] = 1; scans(1); CHECK(eng.bits[LD_BIT_TDN + 2] == 0);
    scans(1);                     CHECK(eng.bits[LD_BIT_TDN + 2] == 1 && eng.regs[LD_REG_T + 2] == 5);
    eng.bits[I(1)] = 1; eng.bits[I(0)] = 0; scans(1);
    CHECK(eng.regs[LD_REG_T + 2] == 0 && eng.bits[LD_BIT_TDN + 2] == 0);
}

static void test_ctu(void)
{
    img_t im = {0};
    BEGIN(&im); emit(&im, LD_OP_XIC, 0, 0, I(0), 0); emit(&im, LD_OP_CTU, 1, 0, 0, 3);
    BEGIN(&im); emit(&im, LD_OP_XIC, 2, 0, I(1), 0); emit(&im, LD_OP_RES, 3, LD_MODE_RES_CTR, 0, 0);
    load_ok(&im);
    eng.bits[I(0)] = 1; scans(3);                                /* held high = one count */
    CHECK(eng.regs[LD_REG_C] == 1);
    eng.bits[I(0)] = 0; scans(1); eng.bits[I(0)] = 1; scans(1);
    CHECK(eng.regs[LD_REG_C] == 2 && eng.bits[LD_BIT_CDN] == 0);
    eng.bits[I(0)] = 0; scans(1); eng.bits[I(0)] = 1; scans(1);
    CHECK(eng.regs[LD_REG_C] == 3 && eng.bits[LD_BIT_CDN] == 1);
    eng.bits[I(0)] = 0; eng.bits[I(1)] = 1; scans(1);
    CHECK(eng.regs[LD_REG_C] == 0 && eng.bits[LD_BIT_CDN] == 0);
}

static void test_mov_compare(void)
{
    img_t im = {0};
    BEGIN(&im); emit(&im, LD_OP_XIC, 0, 0, I(0), 0);
                emit(&im, LD_OP_MOV, 1, LD_MODE_A_CONST, 1234, LD_REG_D + 0);
    BEGIN(&im); emit(&im, LD_OP_GRT, 2, LD_MODE_B_CONST, LD_REG_D + 0, 1000); emit(&im, LD_OP_OTE, 3, 0, Q(2), 0);
    BEGIN(&im); emit(&im, LD_OP_LES, 4, LD_MODE_B_CONST, LD_REG_D + 0, 2000); emit(&im, LD_OP_OTE, 5, 0, Q(3), 0);
    BEGIN(&im); emit(&im, LD_OP_EQU, 6, LD_MODE_B_CONST, LD_REG_D + 0, 1234); emit(&im, LD_OP_OTE, 7, 0, Q(4), 0);
    BEGIN(&im); emit(&im, LD_OP_XIC, 8, 0, I(0), 0);
                emit(&im, LD_OP_MOV, 9, LD_MODE_A_CONST, 0xFFFF, LD_REG_D + 1);
    BEGIN(&im); emit(&im, LD_OP_LES, 10, LD_MODE_B_CONST, LD_REG_D + 1, 0); emit(&im, LD_OP_OTE, 11, 0, Q(5), 0);
    BEGIN(&im); emit(&im, LD_OP_XIC, 12, 0, I(1), 0);                       /* register to register, VFD mirror */
                emit(&im, LD_OP_MOV, 13, 0, LD_REG_D + 0, LD_REG_VFD_H + 2);
    load_ok(&im);
    scans(1);
    CHECK(eng.bits[Q(2)] == 0 && eng.bits[Q(4)] == 0 && eng.regs[LD_REG_D] == 0);
    eng.bits[I(0)] = 1; scans(1);
    CHECK(eng.regs[LD_REG_D] == 1234);
    CHECK(eng.bits[Q(2)] == 1 && eng.bits[Q(3)] == 1 && eng.bits[Q(4)] == 1);
    CHECK(eng.bits[Q(5)] == 1);                                  /* -1 < 0: signed compare */
    eng.bits[I(1)] = 1; scans(1);
    CHECK(eng.regs[LD_REG_VFD_H + 2] == 1234);
}

static void test_elem_power(void)
{
    img_t im = {0};
    BEGIN(&im); emit(&im, LD_OP_XIC, 7, 0, I(0), 0); emit(&im, LD_OP_OTE, 8, 0, Q(0), 0);
    load_ok(&im);
    eng.bits[I(0)] = 1; scans(1); CHECK(eng.elem_power[7] == 1 && eng.elem_power[8] == 1);
    eng.bits[I(0)] = 0; scans(1); CHECK(eng.elem_power[7] == 0 && eng.elem_power[8] == 0);
}

static void test_nested_branch(void)
{
    /* (I0 OR (I1 AND I2)) -> Q0 */
    img_t im = {0};
    BEGIN(&im); OPEN(&im);
    emit(&im, LD_OP_XIC, 0, 0, I(0), 0); NEXT(&im);
    emit(&im, LD_OP_XIC, 1, 0, I(1), 0); emit(&im, LD_OP_XIC, 2, 0, I(2), 0); CLOSE(&im);
    emit(&im, LD_OP_OTE, 3, 0, Q(0), 0);
    load_ok(&im);
    scans(1); CHECK(eng.bits[Q(0)] == 0);
    eng.bits[I(1)] = 1; scans(1); CHECK(eng.bits[Q(0)] == 0);
    eng.bits[I(2)] = 1; scans(1); CHECK(eng.bits[Q(0)] == 1);
    eng.bits[I(1)] = 0; eng.bits[I(0)] = 1; scans(1); CHECK(eng.bits[Q(0)] == 1);
}

static void test_validation(void)
{
    img_t im = {0}; size_t len;
    ld_engine_t e2;

    BEGIN(&im); emit(&im, LD_OP_XIC, 0, 0, I(0), 0); emit(&im, LD_OP_OTE, 1, 0, Q(0), 0);
    len = finish(&im);
    ld_init(&e2);
    CHECK(ld_load(&e2, im.buf, len) == LD_OK);

    uint8_t bad[sizeof im.buf];
    memcpy(bad, im.buf, len); bad[LD_HEADER_SIZE + 4] ^= 0x01;
    CHECK(ld_load(&e2, bad, len) == LD_ERR_CRC);
    memcpy(bad, im.buf, len); bad[0] = 'X';
    CHECK(ld_load(&e2, bad, len) == LD_ERR_MAGIC);
    memcpy(bad, im.buf, len); bad[2] = 9;
    CHECK(ld_load(&e2, bad, len) == LD_ERR_VERSION);
    CHECK(ld_load(&e2, im.buf, len - 1) == LD_ERR_SIZE);
    CHECK(ld_load(&e2, im.buf, 3) == LD_ERR_SIZE);

    memset(&im, 0, sizeof im);
    BEGIN(&im); emit(&im, LD_OP_XIC, 0, 0, LD_BIT_SPACE, 0); len = finish(&im);
    CHECK(ld_load(&e2, im.buf, len) == LD_ERR_ADDR);

    memset(&im, 0, sizeof im);
    BEGIN(&im); emit(&im, LD_OP_TON, 0, 0, 16, 5); len = finish(&im);
    CHECK(ld_load(&e2, im.buf, len) == LD_ERR_ADDR);

    memset(&im, 0, sizeof im);
    BEGIN(&im); emit(&im, 0x7E, 0, 0, 0, 0); len = finish(&im);
    CHECK(ld_load(&e2, im.buf, len) == LD_ERR_OPCODE);

    memset(&im, 0, sizeof im);
    BEGIN(&im); OPEN(&im); emit(&im, LD_OP_OTE, 0, 0, Q(0), 0); len = finish(&im);   /* never closed */
    CHECK(ld_load(&e2, im.buf, len) == LD_ERR_STRUCT);

    memset(&im, 0, sizeof im);
    emit(&im, LD_OP_XIC, 0, 0, I(0), 0); len = finish(&im);                            /* no RUNG_BEGIN */
    CHECK(ld_load(&e2, im.buf, len) == LD_ERR_STRUCT);

    memset(&im, 0, sizeof im);
    BEGIN(&im);
    for (int i = 0; i < 5; i++) OPEN(&im);                                              /* depth 5 > 4 */
    len = finish(&im);
    CHECK(ld_load(&e2, im.buf, len) == LD_ERR_STRUCT);
}

static void test_bad_load_keeps_program(void)
{
    img_t good = {0}, bad = {0};
    BEGIN(&good); emit(&good, LD_OP_XIC, 0, 0, I(0), 0); emit(&good, LD_OP_OTE, 1, 0, Q(0), 0);
    load_ok(&good);
    BEGIN(&bad); emit(&bad, LD_OP_XIC, 0, 0, LD_BIT_SPACE, 0);
    size_t blen = finish(&bad);
    CHECK(ld_load(&eng, bad.buf, blen) == LD_ERR_ADDR);
    eng.bits[I(0)] = 1; scans(1);
    CHECK(eng.bits[Q(0)] == 1);                                  /* old program still runs */
}

static void test_noprog_and_crc(void)
{
    ld_engine_t e2; ld_init(&e2);
    CHECK(ld_scan(&e2) == LD_ERR_NOPROG);
    const uint8_t v[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x01 };  /* known Modbus frame: CRC bytes 84 0A */
    CHECK(ld_crc16(v, sizeof v) == 0x0A84);
}

int main(void)
{
    test_latch(); test_ton(); test_tof(); test_tonr(); test_ctu();
    test_mov_compare(); test_elem_power(); test_nested_branch();
    test_validation(); test_bad_load_keeps_program(); test_noprog_and_crc();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
