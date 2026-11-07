#ifndef PLC_IO_H
#define PLC_IO_H
#include <stdint.h>

void    plc_io_init(void);
uint8_t plc_io_read_inputs(void);          /* bit i = Ii is ON  */
void    plc_io_write_outputs(uint8_t q);   /* bit i = Qi is ON  */
void    plc_io_led_toggle(void);           /* PC13 heartbeat    */

#endif