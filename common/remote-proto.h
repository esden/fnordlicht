/* vim:ts=4 sts=4 et tw=80
 *
 *         fnordlicht firmware
 *
 *    for additional information please
 *    see http://lochraster.org/fnordlicht
 *
 * (c) by Alexander Neumann <alexander@bumpern.de>
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

#ifndef __REMOTE_PROTO_COMMON
#define __REMOTE_PROTO_COMMON 1

#define REMOTE_MSG_LEN 15
#define REMOTE_SYNC_LEN 15

/* normal commands */
#define REMOTE_CMD_FADE_RGB         0x02
#define REMOTE_CMD_FADE_HSV         0x03
#define REMOTE_CMD_SAVE_RGB         0x04
#define REMOTE_CMD_SAVE_HSV         0x05
#define REMOTE_CMD_SAVE_CURRENT     0x06
#define REMOTE_CMD_CONFIG_OFFSETS   0x07
#define REMOTE_CMD_START_PROGRAM    0x09
#define REMOTE_CMD_STOP             0x0A
#define REMOTE_CMD_MODIFY_CURRENT   0x0B
#define REMOTE_CMD_PULL_INT         0x0C
#define REMOTE_CMD_CONFIG_STARTUP   0x0D

#define REMOTE_CMD_RESYNC           0x1b

/* max mode for startup configuration is 1 */
#define REMOTE_STARTUP_MAX_MODE     1
/* maximum parameter size (for copy loop), size of structure storage_config_t,
 * minus 1 for startup_mode enum */
#define REMOTE_STARTUP_MAX_PARAMSIZE 11

/* bootloader commands */
#define REMOTE_CMD_BOOTLOADER       0x80

#define REMOTE_ADDR_BROADCAST 0xff

/* normal commands */
struct remote_msg_t
{
    uint8_t address;
    uint8_t cmd;
    uint8_t data[REMOTE_MSG_LEN-2];
};

/* bootloader commands */
enum mem_type_t
{
    MEM_FLASH = 0,
    MEM_EEPROM = 1,
};

struct remote_msg_boot_config_t
{
    uint8_t address;
    uint8_t cmd;
    enum mem_type_t mem;
    uint16_t start_address;
    uint16_t length;
};

struct remote_msg_boot_flash_t
{
    uint8_t address;
    uint8_t cmd;
    uint8_t data[REMOTE_MSG_LEN-2];
};

#endif
