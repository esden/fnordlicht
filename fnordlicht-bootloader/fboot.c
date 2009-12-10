/* vim:ts=4 sts=4 et tw=80
 *
 *         fnordlicht firmware
 *
 *    for additional information please
 *    see http://lochraster.org/fnordlicht
 *
 * (c) by Alexander Neumann <alexander@bumpern.de>
 *     Lars Noschinski <lars@public.noschinski.de>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <string.h>
#include "../common/io.h"
#include "../common/pt/pt.h"
#include "../common/remote-proto.h"
#include "../common/common.h"
#include "uart.h"

#ifndef CONFIG_BOOTLOADER_BUFSIZE
#define CONFIG_BOOTLOADER_BUFSIZE 512
#endif

/* configure primary pwm pins (MUST be three successive pins in a port) */
#define PWM_PORT B
#define PWM_CHANNELS 3
#define PWM_CHANNEL_MASK (_BV(PB0) | _BV(PB1) | _BV(PB2))
#define PWM_SHIFT 0

/* configure INT pin */
#define REMOTE_INT_PORT D
#define REMOTE_INT_PIN PD2

/* abbreviations for port, ddr and pin */
#define P_PORT _OUTPORT(PWM_PORT)
#define P_DDR _DDRPORT(PWM_PORT)
#define R_PORT _OUTPORT(REMOTE_INT_PORT)
#define R_DDR _DDRPORT(REMOTE_INT_PORT)
#define R_PIN _INPORT(REMOTE_INT_PORT)
#define INTPIN REMOTE_INT_PIN

struct global_t
{
    uint8_t synced;
    uint8_t sync_len;

    union {
        uint8_t buf[REMOTE_MSG_LEN];
        struct remote_msg_t msg;
    };
    uint8_t len;

    uint8_t address;

    union {
        uint8_t data_buf[CONFIG_BOOTLOADER_BUFSIZE];
        uint16_t data_buf16[CONFIG_BOOTLOADER_BUFSIZE/2];
    };
    uint16_t data_len;
    uint8_t *data_address;

    uint8_t delay;

    struct pt thread;
};

struct global_t global;

static void parse_boot_config(struct remote_msg_boot_config_t *msg)
{
    /* remember flash address */
    global.data_address = (uint8_t *)msg->start_address;
}

static void clear_buffer(void)
{
    global.data_len = 0;
}

static void parse_data_cont(struct remote_msg_boot_data_t *msg)
{
    uint8_t len = sizeof(msg->data);
    if (global.data_len + len > CONFIG_BOOTLOADER_BUFSIZE)
        len = CONFIG_BOOTLOADER_BUFSIZE - global.data_len;

    memcpy(&global.data_buf[global.data_len], msg->data, len);
    global.data_len += len;
}

static void parse_crc(struct remote_msg_boot_crc_check_t *msg)
{
    /* compute crc16 over buffer */
    uint16_t checksum = 0xffff;

    uint8_t *ptr = &global.data_buf[0];
    for (uint16_t i = 0; i < sizeof(global.data_buf); i++)
        checksum = _crc16_update(checksum, *ptr++);

    if (checksum != msg->checksum) {
        global.delay = msg->delay;

        /* pull int to gnd */
        R_DDR |= _BV(INTPIN);

        P_PORT |= 0b1;
    }
}

static void flash(void)
{
    /* pull int */
    R_DDR |= _BV(INTPIN);

    uint8_t *addr = global.data_address;
    uint16_t *data = &global.data_buf16[0];

    for (uint8_t page = 0; page < global.data_len/SPM_PAGESIZE; page++) {
        /* erase page */
        boot_page_erase(addr);
        boot_spm_busy_wait();

        for (uint16_t i = 0; i < SPM_PAGESIZE; i += 2) {
            /* fill internal buffer */
            boot_page_fill(addr+i, *data++);
            boot_spm_busy_wait();
        }

        P_PORT ^= (0b100 << PWM_SHIFT);

        /* after filling the temp buffer, write the page and wait till we're done */
        boot_page_write(addr);
        boot_spm_busy_wait();

        /* re-enable application flash section, so we can read it again */
        boot_rww_enable();
        P_PORT &= ~(0b100 << PWM_SHIFT);

        addr += SPM_PAGESIZE;
    }

    global.data_address = addr;

    /* release int */
    R_DDR &= ~_BV(INTPIN);
}

static void remote_parse_msg(struct remote_msg_t *msg)
{
    /* verify address */
    if (msg->address != global.address && msg->address != REMOTE_ADDR_BROADCAST)
        return;

    /* parse command */
    switch (msg->cmd) {
        case REMOTE_CMD_BOOT_CONFIG: parse_boot_config((struct remote_msg_boot_config_t *)msg);
                                     break;
        case REMOTE_CMD_DATA_INITIAL: clear_buffer();
        case REMOTE_CMD_DATA_CONT:   parse_data_cont((struct remote_msg_boot_data_t *)msg);
                                     break;
        case REMOTE_CMD_CRC_CHECK:   parse_crc((struct remote_msg_boot_crc_check_t *)msg);
                                     break;
        case REMOTE_CMD_FLASH:       flash();
                                     break;
        case REMOTE_CMD_ENTER_APP:   /* let the watchdog restart the program */
                                     wdt_enable(WDTO_60MS);
                                     while(1);
                                     break;
    }
}

static PT_THREAD(remote_thread(struct pt *thread))
{
    PT_BEGIN(thread);

    while (1) {
        PT_WAIT_UNTIL(thread, global.len == REMOTE_MSG_LEN);

        remote_parse_msg(&global.msg);
        global.len = 0;
    }

    PT_END(thread);
}

static void remote_poll(void)
{
    /* wait for a byte */
    if (uart_receive_complete())
    {
        uint8_t data = uart_getc();

        /* check if sync sequence has been received before */
        if (global.sync_len == REMOTE_SYNC_LEN) {
            /* synced, safe address and send next address to following device */
            global.address = data;
            uart_putc(data+1);

            /* reset buffer */
            global.len = 0;

            /* enable remote command thread */
            global.synced = 1;
            PT_INIT(&global.thread);
        } else {
            /* just pass through data */
            uart_putc(data);

            /* put data into remote buffer
             * (just processed if remote.synced == 1) */
            if (global.len < sizeof(global.buf))
                global.buf[global.len++] = data;
        }

        /* remember the number of sync bytes received so far */
        if (data == REMOTE_CMD_RESYNC)
            global.sync_len++;
        else
            global.sync_len = 0;
    }

    if (global.synced)
        PT_SCHEDULE(remote_thread(&global.thread));

}

/* NEVER CALL DIRECTLY! */
void disable_watchdog(void) \
  __attribute__((naked)) \
  __attribute__((section(".init3")));
void disable_watchdog(void)
{
    MCUSR = 0;
    wdt_disable();
}


int main(void)
{
    /* configure and enable uart */
    uart_init();

    /* configure outputs */
    P_DDR |= PWM_CHANNEL_MASK;

    /* initialize timer1, CTC at 50ms, prescaler 1024 */
    OCR1A = F_CPU/1024/20;
    TCCR1B = _BV(WGM12) | _BV(CS12) | _BV(CS10);

    while(1) {
        remote_poll();

        if (TIFR & _BV(OCF1A)) {
            TIFR = _BV(OCF1A);

            static uint8_t c;
            if (c == 20) {
                /* blink */
                P_PORT ^= 0b10 << PWM_SHIFT;
                c = 0;
            } else
                c++;

            /* if int is pulled, decrement delay */
            if (R_DDR & _BV(INTPIN)) {
                if (--global.delay == 0) {
                    /* release int */
                    R_DDR &= ~_BV(INTPIN);
                    P_PORT &= ~1;
                }
            }
        }
    }
}
