#include "plc_io.h"
#include "main.h"

/* Polarity. During bring-up: buttons between the pin and GND (pull-up, pressed = 0).
 * OUTPUT_ACTIVE_LOW 1 = relay module that switches ON when its input is pulled to GND,
 *                       wired directly to the Blue Pill.
 * Set it to 0 if a ULN2003 (which inverts) sits between the pin and the module. */
#define INPUT_ACTIVE_LOW   1
#define OUTPUT_ACTIVE_LOW  1

static GPIO_TypeDef *const in_port[8] = {GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA};
static const uint16_t in_pin[8] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3,
                                   GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7};
static GPIO_TypeDef *const out_port[6] = {GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB};
static const uint16_t out_pin[6] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_10,
                                    GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13};

static GPIO_PinState level(int on)
{
    return (on != 0) != (OUTPUT_ACTIVE_LOW != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void plc_io_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* outputs: write the OFF level first, so the relays do not click at start-up */
    for (int i = 0; i < 6; i++) HAL_GPIO_WritePin(out_port[i], out_pin[i], level(0));
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    for (int i = 0; i < 6; i++) { g.Pin = out_pin[i]; HAL_GPIO_Init(out_port[i], &g); }

    /* inputs */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = INPUT_ACTIVE_LOW ? GPIO_PULLUP : GPIO_PULLDOWN;
    for (int i = 0; i < 8; i++) { g.Pin = in_pin[i]; HAL_GPIO_Init(in_port[i], &g); }

    /* on-board LED, PC13 (lit when low) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);
}

uint8_t plc_io_read_inputs(void)
{
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) {
        const int high = HAL_GPIO_ReadPin(in_port[i], in_pin[i]) == GPIO_PIN_SET;
        if (high != (INPUT_ACTIVE_LOW != 0)) v |= (uint8_t)(1u << i);
    }
    return v;
}

void plc_io_write_outputs(uint8_t q)
{
    for (int i = 0; i < 6; i++) HAL_GPIO_WritePin(out_port[i], out_pin[i], level((q >> i) & 1));
}

void plc_io_led_toggle(void) { HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); }