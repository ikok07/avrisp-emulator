#ifndef INCLUDE_STK500V2
#define INCLUDE_STK500V2

#include "usb.h"
#include <stdint.h>
#include <sys/cdefs.h>

#define DEBUGGER_SIGNATURE              "AVRISP_2"
#define HW_VERSION                      0x01
#define SW_MAJOR_VERSION                0x00
#define SW_MINOR_VERSION                0x01
// TODO: Read actual voltage via ADC
#define VOLTAGE_TARGET                  50                  // Hard-coded 5V for now

#define STK500V2_START_BYTE             (0x1B)
#define STK500V2_TOKEN                  (0x0E)
#define STK500V2_XTAL                   (7372800U)

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

/* ── Parameters (AVRISP mkII) ───────────────────────────────────────────── */
#define PARAM_HW_VER                    (0x90) /* hardware version */
#define PARAM_SW_MAJOR                  (0x91) /* firmware major version */
#define PARAM_SW_MINOR                  (0x92) /* firmware minor version */
#define PARAM_VTARGET                   (0x94) /* target supply voltage (units of 10 mV) */
#define PARAM_SCK_DURATION              (0x98) /* SCK period index — used to configure SPI clock speed */
#define PARAMS_COUNT                    7

/* ── ISP Clock Constants ───────────────────────────────────────────── */
#define T_AVRISP                        271.27e-9 
#define B_AVRISP                        12.0

/* ── Command payloads ───────────────────────────────────────────── */
#define CMD_PAYLOAD_RDY_BSY_POLL        {0xFF, 0x00, 0x00, 0x00}

typedef struct {
    uint8_t SequenceNumber;
    uint16_t MessageSize;
    uint8_t Token;
    uint8_t *MessageBody;
    uint8_t Checksum;
} STK500V2_CommandTypeDef;

typedef struct __packed {
    uint8_t CommandID;
    uint8_t TimeoutMS;                  // Command time-out (in ms)
    uint8_t StabDelay;                  // Delay (in ms) used for pin stabilization
    uint8_t CmdExecutionDelay;          // Delay (in ms) in connection with the EnterProgMode command execution 
    uint8_t SyncLoops;                  // Number of synchronization loop
    uint8_t ByteDelay;                  // Delay (in ms) between each byte in the EnterProgMode command.
    uint8_t PollValue;                  // Poll value: 0x53 for AVR, 0x69 for AT89xx 
    uint8_t PollIndex;                  // Start address, received byte: 0 = no polling, 3 = AVR, 4 = AT89xx 
    uint8_t Commands[4];                // Command bytes to be transmit
} STK500V2_EnterProgModeBodyTypeDef;

typedef struct __packed {
    uint8_t CommandID;
    uint8_t EraseDelay;                 // Delay (in ms) to ensure that the erase of the device is finished
    uint8_t PollMethod;                 // Poll method, 0 = use delay 1 = use RDY/BSY command
    uint8_t Commands[4];                // Command bytes to be transmit
} STK500V2_ChipEraseBodyTypeDef;
typedef struct __packed {
    uint8_t CommandID;
    uint8_t RetAddr;                    // Return address - indicates after which of the transmitted bytes on the SPI interface to store the return byte
    uint8_t Commands[4];                // Command bytes to be transmit
} STK500V2_ReadFuseBodyTypeDef;

typedef struct {
    uint8_t PageModeEnabled;
    uint8_t TimedDelayWordMode;
    uint8_t ValuePollingWordMode;
    uint8_t RdyBsyPollingWordMode;
    uint8_t TimedDelayPageMode;
    uint8_t ValuePollingPageMode;
    uint8_t RdyBsyPollingPageMode;
} STK500V2_CmdProgramFlashModeTypeDef;
typedef struct {
    uint8_t CommandID;
    uint16_t NumBytes;                                              // Total number of bytes to program
    STK500V2_CmdProgramFlashModeTypeDef Mode;                       // Mode byte
    uint8_t Delay;                                                  // Delay, used for different types of programming termination, according to mode byte 
    uint8_t CmdLoadProgramMemory;
    uint8_t CmdWriteProgramMemory;
    uint8_t CmdReadProgramMemory;
    uint8_t PollValues[2];
    uint8_t *Data;
} STK500V2_CmdProgramFLASHBodyTypeDef;
typedef struct {
    uint8_t CommandID;
    uint8_t NumTx;                      // Number of bytes to transmit
    uint8_t NumRx;                      // Number of bytes to receive
    uint8_t RxStartAddr;                // Start address of returned data. Specifies on what transmitted byte the response is to be stored and returned. 
    uint8_t *TxData;                    // The data  be transmitted. The size is specified by NumTx
} STK500V2_CmdSPIMultiBodyTypeDef;

typedef struct {
    uint8_t ParamID;
    uint8_t Value;
} STK500V2_ParamPairTypeDef;

extern STK500V2_ParamPairTypeDef Stk500V2_StaticParams[PARAMS_COUNT];

USB_CommandStatusTypeDef STK500V2_HandleCmd(STK500V2_CommandTypeDef *Stk500Command);
USB_CommandStatusTypeDef STK500V2_ParseCmd(ringbuf_t RingBuffer, STK500V2_CommandTypeDef *Stk500Command);

#endif /* INCLUDE_STK500V2 */
