#ifndef INCLUDE_STK500V2
#define INCLUDE_STK500V2

#include "usb.h"
#include <stdint.h>

#define MAX_BODY_SIZE                   (275)
#define MIN_COMMAND_SIZE                (5)
#define MAX_COMMAND_SIZE                (MAX_BODY_SIZE + 6)
#define DEBUGGER_SIGNATURE              "AVRISP_2"


#define STK500V2_START_BYTE             (0x1B)
#define STK500V2_TOKEN                  (0x0E)

/* ── General commands ───────────────────────────────────────────────────── */
#define CMD_SIGN_ON                     (0x01)
#define CMD_SET_PARAMETER               (0x02)
#define CMD_GET_PARAMETER               (0x03)
#define CMD_LOAD_ADDRESS                (0x06)
#define CMD_FIRMWARE_UPGRADE            (0x07)

/* ── ISP commands ───────────────────────────────────────────────────────── */
#define CMD_ENTER_PROGMODE_ISP          (0x10)
#define CMD_LEAVE_PROGMODE_ISP          (0x11)
#define CMD_CHIP_ERASE_ISP              (0x12)
#define CMD_PROGRAM_FLASH_ISP           (0x13)
#define CMD_READ_FLASH_ISP              (0x14)
#define CMD_PROGRAM_EEPROM_ISP          (0x15)
#define CMD_READ_EEPROM_ISP             (0x16)
#define CMD_PROGRAM_FUSE_ISP            (0x17)
#define CMD_READ_FUSE_ISP               (0x18)
#define CMD_PROGRAM_LOCK_ISP            (0x19)
#define CMD_READ_LOCK_ISP               (0x1A)
#define CMD_READ_SIGNATURE_ISP          (0x1B)
#define CMD_READ_OSCCAL_ISP             (0x1C)
#define CMD_SPI_MULTI                   (0x1D)

/* ── PP (parallel programming) commands ─────────────────────────────────── */
#define CMD_ENTER_PROGMODE_PP           (0x20)
#define CMD_LEAVE_PROGMODE_PP           (0x21)
#define CMD_CHIP_ERASE_PP               (0x22)
#define CMD_PROGRAM_FLASH_PP            (0x23)
#define CMD_READ_FLASH_PP               (0x24)
#define CMD_PROGRAM_EEPROM_PP           (0x25)
#define CMD_READ_EEPROM_PP              (0x26)
#define CMD_PROGRAM_FUSE_PP             (0x27)
#define CMD_READ_FUSE_PP                (0x28)
#define CMD_PROGRAM_LOCK_PP             (0x29)
#define CMD_READ_LOCK_PP                (0x2A)
#define CMD_READ_SIGNATURE_PP           (0x2B)
#define CMD_READ_OSCCAL_PP              (0x2C)
#define CMD_SET_CONTROL_STACK           (0x2D)

/* ── HVSP (high-voltage serial programming) commands ────────────────────── */
#define CMD_ENTER_PROGMODE_HVSP         (0x30)
#define CMD_LEAVE_PROGMODE_HVSP         (0x31)
#define CMD_CHIP_ERASE_HVSP             (0x32)
#define CMD_PROGRAM_FLASH_HVSP          (0x33)
#define CMD_READ_FLASH_HVSP             (0x34)
#define CMD_PROGRAM_EEPROM_HVSP         (0x35)
#define CMD_READ_EEPROM_HVSP            (0x36)
#define CMD_PROGRAM_FUSE_HVSP           (0x37)
#define CMD_READ_FUSE_HVSP              (0x38)
#define CMD_PROGRAM_LOCK_HVSP           (0x39)
#define CMD_READ_LOCK_HVSP              (0x3A)
#define CMD_READ_SIGNATURE_HVSP         (0x3B)
#define CMD_READ_OSCCAL_HVSP            (0x3C)

/* ── Status codes ───────────────────────────────────────────────────────── */
/* General response status (body byte 1 of every reply) */
#define STATUS_CMD_OK                   (0x00) /* command executed successfully */
#define STATUS_CMD_TOUT                 (0x80) /* command timed out */
#define STATUS_RDY_BSY_TOUT             (0x81) /* RDY/BSY line timed out */
#define STATUS_SET_PARAM_MISSING        (0x82) /* parameter not set before use */
#define STATUS_CMD_FAILED               (0xC0) /* command execution failed */
#define STATUS_CKSUM_ERROR              (0xC1) /* checksum mismatch in received message */
#define STATUS_CMD_UNKNOWN              (0xC9) /* unrecognised command ID */

/* Connection / target detection status */
#define STATUS_CONN_FAIL_MOSI           (0x01) /* MOSI line fault */
#define STATUS_CONN_FAIL_RST            (0x02) /* RESET line fault */
#define STATUS_CONN_FAIL_SCK            (0x04) /* SCK line fault */
#define STATUS_TGT_NOT_DETECTED         (0x10) /* no target device detected */
#define STATUS_TGT_REVERSE_INSERTED     (0x20) /* target inserted backwards */

/* ISP-specific status (returned in ISP response bytes) */
#define STATUS_ISP_READY                (0x00) /* ISP interface ready */

/* ── Parameters ─────────────────────────────────────────────────────────── */
#define PARAM_HW_VER                    (0x90)
#define PARAM_SW_MAJOR                  (0x91)
#define PARAM_SW_MINOR                  (0x92)
#define PARAM_VTARGET                   (0x94)
#define PARAM_CONTROLLER_INIT           (0x9F)
#define PARAM_STATUS_TGT_CONN           (0xA1)
#define PARAM_DISCHARGEDELAY            (0xA4)


typedef struct {
    uint8_t SequenceNumber;
    uint16_t MessageSize;
    uint8_t Token;
    uint8_t *MessageBody;
    uint8_t Checksum;
} STK500V2_CommandTypeDef;

USB_CommandStatusTypeDef STK500V2_HandleCmd(STK500V2_CommandTypeDef *Stk500Command);
USB_CommandStatusTypeDef STK500V2_ParseCmd(ringbuf_t RingBuffer, STK500V2_CommandTypeDef *Stk500Command);

#endif /* INCLUDE_STK500V2 */
