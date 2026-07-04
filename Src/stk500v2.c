#include "stk500v2.h"
#include "adc.h"
#include "app_state.h"
#include "gpio_defs.h"
#include "ringbuf.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_spi.h"
#include "usb.h"
#include "usbd_def.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MIN_COMMAND_SIZE (5)
#define MAX_COMMAND_SIZE (STK500V2_MAX_BODY_SIZE + 6)
#define DEFAULT_CLK_DURATION (0xFF)
#define AVR_WORD_SIZE_BYTES (2)

#define INSTR_LOAD_EXT_ADDRESS_BYTE(Byte) {0x4D, 0x00, Byte, 0x00}

/* ------ Command handlers ------ */
static uint8_t enter_prog_mode(STK500V2_CommandTypeDef *Stk500Command);
static void leave_prog_mode(STK500V2_CommandTypeDef *Stk500Command);

static uint8_t chip_erase(STK500V2_CommandTypeDef *Stk500Command);

static uint8_t read_fuse(STK500V2_CommandTypeDef *Stk500Command,
                         uint8_t *RxData);
static uint8_t write_fuse(STK500V2_CommandTypeDef *Stk500Command);

static uint8_t read_memory(STK500V2_CommandTypeDef *Stk500Command,
                           uint8_t *Data);
static uint8_t program_memory(STK500V2_CommandTypeDef *Stk500Command);

static uint8_t spi_multi(STK500V2_CommandTypeDef *Stk500Command,
                         uint8_t *RxData, uint8_t *RxLen);

/* ------ Utilities ------ */
static void format_response(STK500V2_CommandTypeDef *Stk500Command,
                            uint8_t *Response, uint8_t *ResponseLen,
                            uint8_t Status, uint8_t *Data, size_t Len);
static uint8_t poll_rdy_bsy(uint16_t TimeoutMs);

static uint8_t poll_value(uint8_t ReadCommand, uint16_t Address,
                          uint8_t PollValue, uint16_t TimeoutMs);

static uint8_t send_load_ext_addr_instr();

static uint8_t generate_checksum(uint8_t *Buffer, uint16_t Len);
static uint8_t validate_checksum(uint8_t *Buffer, uint16_t Len);

static uint8_t get_parameter_value(uint8_t ParamID);
static uint8_t set_parameter_value(uint8_t ParamID, uint8_t Value);

static uint8_t get_sck_period_value_from_freq(uint32_t Frequency);
static uint32_t get_freq_from_sck_period_value(uint8_t Value);

static void avr_enable_reset();
static void avr_disable_reset();

static uint8_t gCommandBuffer[MAX_COMMAND_SIZE];
static uint8_t gCurrentSckDuration = DEFAULT_CLK_DURATION;
static uint32_t gStk500Address = 0x00;
static uint8_t gStk500ExtAddress = 0x00;

// Whether the address represents a word or a single byte
static uint8_t gWordAddressing = 0x00;

STK500V2_ParamPairTypeDef Stk500V2_StaticParams[PARAMS_COUNT] = {
    {.ParamID = PARAM_HW_VER, .Value = HW_VERSION},
    {.ParamID = PARAM_SW_MAJOR, .Value = SW_MAJOR_VERSION},
    {.ParamID = PARAM_SW_MINOR, .Value = SW_MINOR_VERSION},
};

USB_CommandHandlerTypeDef STK500V2_GetHandler() {
  USB_CommandHandlerTypeDef methods = {
      .ParseCmd =
          (USB_CommandStatusTypeDef (*)(ringbuf_t, void *))STK500V2_ParseCmd,
      .HandleCmd = (void (*)(void *, uint8_t *, uint8_t *))STK500V2_HandleCmd,
  };
  return methods;
}

void STK500V2_HandleCmd(STK500V2_CommandTypeDef *Stk500Command,
                        uint8_t *Response, uint8_t *ResponseLen) {
  uint8_t cmd_id = Stk500Command->MessageBody[0];
  if (cmd_id == CMD_SIGN_ON) {

    // Make sure MCU is not in programming mode
    avr_disable_reset();

    uint8_t data[9];
    data[0] = strlen(DEBUGGER_SIGNATURE);
    memcpy(data + 1, DEBUGGER_SIGNATURE, data[0]);
    format_response(Stk500Command, Response, ResponseLen, STATUS_CMD_OK, data,
                    sizeof(data));
  } else if (cmd_id == CMD_GET_PARAMETER) {

    uint8_t param_id = Stk500Command->MessageBody[1];
    uint8_t data = get_parameter_value(param_id);
    format_response(Stk500Command, Response, ResponseLen, STATUS_CMD_OK, &data,
                    1);

  } else if (cmd_id == CMD_SET_PARAMETER) {

    uint8_t param_id = Stk500Command->MessageBody[1];
    uint8_t param_value = Stk500Command->MessageBody[2];
    uint8_t rc = set_parameter_value(param_id, param_value);
    format_response(Stk500Command, Response, ResponseLen,
                    rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK, NULL, 0);

  } else if (cmd_id == CMD_ENTER_PROGMODE_ISP) {

    uint8_t rc = enter_prog_mode(Stk500Command);
    uint8_t resp_status = rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK;
    if (rc == 2)
      resp_status = STATUS_CMD_TOUT;
    format_response(Stk500Command, Response, ResponseLen, resp_status, NULL, 0);

  } else if (cmd_id == CMD_LEAVE_PROGMODE_ISP) {

    leave_prog_mode(Stk500Command);
    format_response(Stk500Command, Response, ResponseLen, STATUS_CMD_OK, NULL,
                    0);

  } else if (cmd_id == CMD_READ_FUSE_ISP || cmd_id == CMD_READ_SIGNATURE_ISP) {

    uint8_t fuse_byte;
    uint8_t rc = read_fuse(Stk500Command, &fuse_byte);
    // Protocol requires a second status byte which is always OK.
    uint8_t body[] = {fuse_byte, STATUS_CMD_OK};
    format_response(Stk500Command, Response, ResponseLen,
                    rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK, body,
                    rc > 0 ? 1 : 2);

  } else if (cmd_id == CMD_CHIP_ERASE_ISP) {

    uint8_t rc = chip_erase(Stk500Command);
    format_response(Stk500Command, Response, ResponseLen,
                    rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK, NULL, 0);

  } else if (cmd_id == CMD_PROGRAM_FUSE_ISP || cmd_id == CMD_PROGRAM_LOCK_ISP) {

    uint8_t rc = write_fuse(Stk500Command);
    // Protocol requires a second status byte which is always OK.
    uint8_t body[] = {STATUS_CMD_OK};
    format_response(Stk500Command, Response, ResponseLen,
                    rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK, body, 1);

  } else if (cmd_id == CMD_LOAD_ADDRESS) {

    gStk500Address = ((uint32_t)Stk500Command->MessageBody[1] << 24) |
                     ((uint32_t)Stk500Command->MessageBody[2] << 16) |
                     ((uint32_t)Stk500Command->MessageBody[3] << 8) |
                     ((uint32_t)Stk500Command->MessageBody[4]);

    // Handle memory over 65 KBytes
    if ((gStk500Address & (1UL << 31)) > 0) {
      gStk500Address &= ~(1UL << 31); // Remove the extended address flag
      gStk500ExtAddress =
          (gStk500Address >> 16) & 0xFF; // Extract the extended address
    }

    format_response(Stk500Command, Response, ResponseLen, STATUS_CMD_OK, NULL,
                    0);

  } else if (cmd_id == CMD_PROGRAM_FLASH_ISP ||
             cmd_id == CMD_PROGRAM_EEPROM_ISP) {

    gWordAddressing = cmd_id == CMD_PROGRAM_FLASH_ISP;
    uint8_t rc = program_memory(Stk500Command);
    uint8_t resp_status = rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK;

    if (rc == 2)
      resp_status = STATUS_CMD_TOUT;
    if (rc == 3)
      resp_status = STATUS_RDY_BSY_TOUT;

    format_response(Stk500Command, Response, ResponseLen, resp_status, NULL, 0);

  } else if (cmd_id == CMD_READ_FLASH_ISP || cmd_id == CMD_READ_EEPROM_ISP) {

    gWordAddressing = cmd_id == CMD_READ_FLASH_ISP;
    uint16_t data_len =
        (Stk500Command->MessageBody[1] << 8) | Stk500Command->MessageBody[2];
    uint8_t data[data_len + 1];
    uint8_t rc = read_memory(Stk500Command, data);
    // Protocol requires a second status byte which is always OK.
    data[data_len] = STATUS_CMD_OK;
    format_response(Stk500Command, Response, ResponseLen,
                    rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK, data, data_len);

  } else if (cmd_id == CMD_SPI_MULTI) {

    uint8_t rx_data[255];
    uint8_t rx_len = 0;
    uint8_t rc = spi_multi(Stk500Command, rx_data, &rx_len);
    format_response(Stk500Command, Response, ResponseLen,
                    rc > 0 ? STATUS_CMD_FAILED : STATUS_CMD_OK, rx_data,
                    rx_len);
  }
}

USB_CommandStatusTypeDef
STK500V2_ParseCmd(ringbuf_t RingBuffer,
                  STK500V2_CommandTypeDef *Stk500Command) {
  USB_CommandStatusTypeDef status = USB_COMMAND_OK;

  uint16_t len = ringbuf_bytes_used(RingBuffer);

  // Check the minimum required bytes for an STK500 command
  if (len < MIN_COMMAND_SIZE) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  }

  // Extract only the header bytes for now
  uint8_t header[MIN_COMMAND_SIZE];
  ringbuf_peek(RingBuffer, header, MIN_COMMAND_SIZE);

  // Check wheter this is STK500 command
  if (header[0] != STK500V2_START_BYTE) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  }

  Stk500Command->SequenceNumber = header[1];
  Stk500Command->MessageSize = (header[2] << 8) | header[3];
  Stk500Command->Token = header[4];

  // After we know the total size we can validate the whole command
  uint16_t cmd_total_size = Stk500Command->MessageSize + MIN_COMMAND_SIZE +
                            1; // Account for the checksum byte
  if (len < cmd_total_size) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  }

  // If command is valid, copy it from the ringbuffer
  ringbuf_memcpy_from(gCommandBuffer, RingBuffer, cmd_total_size);

  // Validate the checksum
  if (!validate_checksum(gCommandBuffer, cmd_total_size)) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  };

  // Copy body and checksum to passed structure
  memcpy(Stk500Command->MessageBody, gCommandBuffer + MIN_COMMAND_SIZE,
         Stk500Command->MessageSize);
  Stk500Command->Checksum =
      gCommandBuffer[Stk500Command->MessageSize + MIN_COMMAND_SIZE];

  return USB_COMMAND_OK;
}

uint8_t enter_prog_mode(STK500V2_CommandTypeDef *Stk500Command) {
  STK500V2_EnterProgModeBodyTypeDef body;
  memcpy(&body, Stk500Command->MessageBody,
         sizeof(STK500V2_EnterProgModeBodyTypeDef));

  // Enable SPI GPIOs
  SPI_EnableIO();

  // Enter programming mode by pulling RESET to low
  avr_enable_reset();

  // Stabilization delay
  HAL_Delay(body.StabDelay);
  HAL_Delay(body.CmdExecutionDelay);

  // Send commands
  uint8_t rx_buf[4];
  HAL_StatusTypeDef hal_err;
  uint8_t success = 0;
  for (uint8_t i = 0; i < body.SyncLoops; i++) {
    if ((hal_err =
             HAL_SPI_TransmitReceive(&gAppState.hspi1, body.Commands, rx_buf,
                                     sizeof(body.Commands), 100)) != HAL_OK) {
      SPI_DisableIO();
      return hal_err == HAL_TIMEOUT ? 2 : 1;
    };
    // PollIndex is 1-based
    if (rx_buf[body.PollIndex - 1] == body.PollValue) {
      success = 1;
      break;
    }

    // Refresh RESET line
    avr_disable_reset();
    HAL_Delay(body.StabDelay);
    avr_enable_reset();
    HAL_Delay(body.StabDelay);
  }

  if (!success)
    return 1;

  return 0;
}

void leave_prog_mode(STK500V2_CommandTypeDef *Stk500Command) {
  uint8_t pre_delay = Stk500Command->MessageBody[1];
  uint8_t post_delay = Stk500Command->MessageBody[2];

  HAL_Delay(pre_delay);

  avr_disable_reset();
  SPI_DisableIO();

  HAL_Delay(post_delay);
}

uint8_t chip_erase(STK500V2_CommandTypeDef *Stk500Command) {
  HAL_StatusTypeDef hal_err;
  STK500V2_ChipEraseBodyTypeDef body;
  memcpy(&body, Stk500Command->MessageBody, Stk500Command->MessageSize);

  if ((hal_err = HAL_SPI_Transmit(&gAppState.hspi1, body.Commands,
                                  sizeof(body.Commands), 100)) != HAL_OK) {
    return 1;
  }

  if (body.PollMethod == 1) {
    // Poll RDY/BSY command
    return poll_rdy_bsy(body.EraseDelay);
  }

  // Blindly wait for the worst case scenario
  HAL_Delay(body.EraseDelay);
  return 0;
}

uint8_t read_fuse(STK500V2_CommandTypeDef *Stk500Command, uint8_t *RxData) {
  HAL_StatusTypeDef hal_err;
  STK500V2_ReadFuseBodyTypeDef body;
  memcpy(&body, Stk500Command->MessageBody, sizeof(body));

  uint8_t rx_data[sizeof(body.Commands)];

  if ((hal_err =
           HAL_SPI_TransmitReceive(&gAppState.hspi1, body.Commands, rx_data,
                                   sizeof(body.Commands), 100)) != HAL_OK) {
    return 1;
  }

  // There is only one fuse byte
  *RxData = rx_data[body.RetAddr - 1];

  return 0;
}

uint8_t write_fuse(STK500V2_CommandTypeDef *Stk500Command) {
  HAL_StatusTypeDef hal_err = HAL_SPI_Transmit(
      &gAppState.hspi1, Stk500Command->MessageBody + 1, 4, 100);
  if (hal_err != HAL_OK) {
    return 1;
  }
  return 0;
}

uint8_t read_memory(STK500V2_CommandTypeDef *Stk500Command, uint8_t *Data) {
  HAL_StatusTypeDef hal_err = HAL_OK;
  uint16_t num_bytes =
      (Stk500Command->MessageBody[1] << 8) | Stk500Command->MessageBody[2];
  uint8_t cmd_read_low = Stk500Command->MessageBody[3];
  uint8_t cmd_read_high = Stk500Command->MessageBody[3] | 0x08;

  if ((num_bytes & 1) != 0) {
    // Number of bytes should be even (each word has 2 bytes)
    return 1;
  }

  // If more than 128KB of FLASH, send extended address byte
  if (gStk500ExtAddress != 0x00) {
    if (send_load_ext_addr_instr() != 0) {
      return 1;
    }
  }

  if (num_bytes > MAX_COMMAND_SIZE)
    return 1;

  uint8_t rx_buf[num_bytes];
  uint16_t rx_buf_idx = 0;
  for (uint16_t i = 0; i < num_bytes;
       i += gWordAddressing ? AVR_WORD_SIZE_BYTES : 1) {
    uint8_t tx[4] = {
        cmd_read_low,
        (gStk500Address >> 8) & 0xFF, // HIGH Address byte
        gStk500Address & 0xFF,        // LOW Address byte
        0x00,
    };
    uint8_t rx[4];

    // Get LOW byte
    if ((hal_err = HAL_SPI_TransmitReceive(&gAppState.hspi1, tx, rx, sizeof(tx),
                                           100)) != HAL_OK) {
      return 1;
    }

    rx_buf[rx_buf_idx++] = rx[3];

    // For FLASH reads we must also read the high byte from the word
    if (gWordAddressing) {
      // Get HIGH byte
      tx[0] = cmd_read_high;
      if ((hal_err = HAL_SPI_TransmitReceive(&gAppState.hspi1, tx, rx,
                                             sizeof(tx), 100)) != HAL_OK) {
        return 1;
      }

      // Copy received byte from flash
      rx_buf[rx_buf_idx++] = rx[3];
    }

    // Increment word address
    gStk500Address++;
  }

  memcpy(Data, rx_buf, num_bytes);
  return 0;
}

uint8_t program_memory(STK500V2_CommandTypeDef *Stk500Command) {
  STK500V2_CmdProgramFLASHBodyTypeDef body;
  body.CommandID = Stk500Command->MessageBody[0];
  body.NumBytes =
      (Stk500Command->MessageBody[1] << 8) | Stk500Command->MessageBody[2];

  if (body.NumBytes > MAX_COMMAND_SIZE)
    return 1;

  body.Mode = (STK500V2_CmdProgramFlashModeTypeDef){
      .PageModeEnabled = (Stk500Command->MessageBody[3] & (1 << 0)) > 0,
      .TimedDelayWordMode = (Stk500Command->MessageBody[3] & (1 << 1)) > 0,
      .ValuePollingWordMode = (Stk500Command->MessageBody[3] & (1 << 2)) > 0,
      .RdyBsyPollingWordMode = (Stk500Command->MessageBody[3] & (1 << 3)) > 0,
      .TimedDelayPageMode = (Stk500Command->MessageBody[3] & (1 << 4)) > 0,
      .ValuePollingPageMode = (Stk500Command->MessageBody[3] & (1 << 5)) > 0,
      .RdyBsyPollingPageMode = (Stk500Command->MessageBody[3] & (1 << 6)) > 0,
      .WritePage = (Stk500Command->MessageBody[3] & (1 << 7)) > 0,
  };

  body.Delay = Stk500Command->MessageBody[4];
  body.CmdLoadProgramMemory = Stk500Command->MessageBody[5];
  body.CmdWriteProgramMemory = Stk500Command->MessageBody[6];
  body.CmdReadProgramMemory = Stk500Command->MessageBody[7];
  memcpy(body.PollValues, &Stk500Command->MessageBody[8], 2);
  body.Data = &(Stk500Command->MessageBody[10]);

  if ((body.NumBytes & 1) != 0) {
    // Number of bytes should be even (each word has 2 bytes)
    return 1;
  }

  // If more than 128KB of FLASH, send extended address byte
  if (gStk500ExtAddress != 0x00) {
    if (send_load_ext_addr_instr() != 0) {
      return 1;
    }
  }

  HAL_StatusTypeDef hal_err;
  uint16_t page_addr = gStk500Address;
  for (uint16_t i = 0; i < body.NumBytes;
       i += gWordAddressing ? AVR_WORD_SIZE_BYTES : 1) {
    uint8_t data_low = body.Data[i];
    uint8_t data_high = body.Data[i + 1];
    uint8_t data[4] = {
        body.CmdLoadProgramMemory,    // Use the provided LOW byte command
        (gStk500Address >> 8) & 0xFF, // HIGH Address byte
        gStk500Address & 0xFF,        // LOW Address byte
        data_low,
    };

    // Transmit LOW
    if ((hal_err = HAL_SPI_Transmit(&gAppState.hspi1, data, sizeof(data),
                                    100)) != HAL_OK) {
      return 1;
    }

    // For FLASH reads we must also write the high byte to the word
    if (gWordAddressing) {
      // Transmit HIGH
      data[0] =
          body.CmdLoadProgramMemory | 0x08; // Convert to HIGH byte command
      data[3] = data_high;
      if ((hal_err = HAL_SPI_Transmit(&gAppState.hspi1, data, sizeof(data),
                                      100)) != HAL_OK) {
        return 1;
      }
    }

    if (!body.Mode.PageModeEnabled) {
      // Delay after each word in "word mode"
      if (body.Mode.TimedDelayWordMode) {
        HAL_Delay(body.Delay);
      } else if (body.Mode.RdyBsyPollingWordMode) {
        if (poll_rdy_bsy(body.Delay) != 0)
          return 2;
      } else if (body.Mode.ValuePollingWordMode) {
        // FLASH programming has only one poll value
        if (poll_value(body.CmdReadProgramMemory, gStk500Address,
                       body.PollValues[0], body.Delay) != 0)
          return 2;
      }
    }

    // Increment word address
    gStk500Address++;
  }

  if (body.Mode.PageModeEnabled && body.Mode.WritePage) {
    // Commit data to FLASH after writing all data to RAM
    uint8_t data[] = {
        body.CmdWriteProgramMemory,
        (page_addr >> 8) & 0xFF, // HIGH Address byte
        page_addr & 0xFF,        // LOW Address byte
        0x00,
    };
    if ((hal_err = HAL_SPI_Transmit(&gAppState.hspi1, data, sizeof(data),
                                    100)) != HAL_OK) {
      return 1;
    }

    if (body.Mode.TimedDelayPageMode) {
      HAL_Delay(body.Delay);
    } else if (body.Mode.RdyBsyPollingPageMode) {
      if (poll_rdy_bsy(body.Delay) != 0)
        return 2;
    } else if (body.Mode.ValuePollingPageMode) {
      // FLASH programming has only one poll value
      if (poll_value(body.CmdReadProgramMemory, page_addr, body.PollValues[0],
                     body.Delay) != 0)
        return 2;
    }
  }

  return 0;
}

uint8_t spi_multi(STK500V2_CommandTypeDef *Stk500Command, uint8_t *RxData,
                  uint8_t *RxLen) {
  STK500V2_CmdSPIMultiBodyTypeDef body;
  body.CommandID = Stk500Command->MessageBody[0];
  body.NumTx = Stk500Command->MessageBody[1];
  body.NumRx = Stk500Command->MessageBody[2];
  body.RxStartAddr = Stk500Command->MessageBody[3];
  body.TxData = &(Stk500Command->MessageBody[4]);

  uint8_t rx_data[255];

  // Clear RxData buffer
  memset(RxData, 0, 255);

  HAL_StatusTypeDef hal_err;
  if ((hal_err = HAL_SPI_TransmitReceive(&gAppState.hspi1, body.TxData, rx_data,
                                         body.NumTx, 100)) != HAL_OK) {
    return 1;
  }

  if (body.NumRx > 0) {
    uint16_t end = (uint16_t)body.RxStartAddr + (uint16_t)body.NumRx;
    if (end > body.NumTx) {
      // Received bytes count should never exceed tx bytes count
      return 1;
    }
    memcpy(RxData, rx_data + body.RxStartAddr, body.NumRx);
  }

  *RxLen = body.NumRx;

  return 0;
}

void format_response(STK500V2_CommandTypeDef *Stk500Command, uint8_t *Response,
                     uint8_t *ResponseLen, uint8_t Status, uint8_t *Data,
                     size_t Len) {
  size_t body_len = Len + 2;
  uint8_t body[body_len];
  body[0] = Stk500Command->MessageBody[0]; // Answer ID = Command ID
  body[1] = Status;
  memcpy(body + 2, Data, Len);

  // Header + Body + Checksum
  uint16_t response_len = MIN_COMMAND_SIZE + body_len + 1;
  uint8_t response[response_len];
  response[0] = STK500V2_START_BYTE;
  response[1] = Stk500Command->SequenceNumber; // Sequence number
  response[2] = (body_len >> 8) & 0xFF;
  response[3] = body_len & 0xFF;
  response[4] = Stk500Command->Token;
  memcpy(response + 5, body, body_len);
  response[MIN_COMMAND_SIZE + body_len] =
      generate_checksum(response, response_len);

  // Copy to passed buffer
  memcpy(Response, response, response_len);
  *ResponseLen = response_len;
}

uint8_t poll_rdy_bsy(uint16_t TimeoutMs) {
  HAL_StatusTypeDef hal_err;
  uint32_t now = HAL_GetTick();
  uint8_t tx_buf[] = CMD_PAYLOAD_RDY_BSY_POLL;
  uint8_t rx_buf[sizeof(tx_buf)];

  while (HAL_GetTick() - now < TimeoutMs) {
    memset(rx_buf, 0, sizeof(rx_buf));

    if ((hal_err = HAL_SPI_TransmitReceive(&gAppState.hspi1, tx_buf, rx_buf,
                                           sizeof(tx_buf), 100)) != HAL_OK) {
      return 1;
    }

    if (rx_buf[sizeof(rx_buf) - 1] & 0x01) {
      HAL_Delay(1);
      continue;
    }

    return 0;
  }
  // Timeout
  return 1;
}

uint8_t poll_value(uint8_t ReadCommand, uint16_t Address, uint8_t PollValue,
                   uint16_t TimeoutMs) {
  HAL_StatusTypeDef hal_err;
  uint8_t data[] = {
      ReadCommand,
      (Address >> 8) & 0xFF,
      Address & 0xFF,
      0x00, // Add dummy byte on which the poll value will be received
  };

  uint8_t rx_buf[sizeof(data)];

  uint32_t now = HAL_GetTick();
  while (HAL_GetTick() - now < TimeoutMs) {
    if ((hal_err = HAL_SPI_TransmitReceive(&gAppState.hspi1, data, rx_buf,
                                           sizeof(data), 100)) != HAL_OK) {
      return 1;
    }

    // Check whether the data is written
    if (rx_buf[3] == PollValue)
      return 0;
  }

  return 2;
}

uint8_t send_load_ext_addr_instr() {
  HAL_StatusTypeDef hal_err;
  uint8_t data[] = INSTR_LOAD_EXT_ADDRESS_BYTE(gStk500ExtAddress);
  if ((hal_err = HAL_SPI_Transmit(&gAppState.hspi1, data, sizeof(data), 100)) !=
      HAL_OK) {
    return 1;
  }
  return 0;
}

uint8_t generate_checksum(uint8_t *Buffer, uint16_t Len) {
  uint16_t correct_checksum = 0;
  // Last byte is the checksum, so exclude it
  for (uint16_t i = 0; i < Len - 1; i++) {
    correct_checksum ^= Buffer[i];
  }

  return correct_checksum;
}

uint8_t validate_checksum(uint8_t *Buffer, uint16_t Len) {
  uint16_t correct_checksum = generate_checksum(Buffer, Len);
  return correct_checksum == Buffer[Len - 1];
}

/**
  @brief Get parameter value by parameter id
  @returns Parameter value. (if parameter is unsupported, a zero is returned)
*/
uint8_t get_parameter_value(uint8_t ParamID) {
  if (ParamID <= PARAM_SW_MINOR) {
    for (uint8_t i = 0;
         i < sizeof(Stk500V2_StaticParams) / sizeof(Stk500V2_StaticParams[0]);
         i++) {
      STK500V2_ParamPairTypeDef pair = Stk500V2_StaticParams[i];
      if (pair.ParamID == ParamID) {
        return pair.Value;
      }
    }
  } else if (ParamID == PARAM_VTARGET) {
    uint32_t millivolts;
    HAL_StatusTypeDef hal_err;

    if ((hal_err = ADC_GetMV(&millivolts)) == HAL_OK) {
      return millivolts / 100;
    }
  } else if (ParamID == PARAM_SCK_DURATION) {
    if (gCurrentSckDuration == 0xFF) {
      gCurrentSckDuration =
          get_sck_period_value_from_freq(SPI1_DEFAULT_TARGET_FREQUENCY_HZ);
    }
    return gCurrentSckDuration;
  }

  return 0x00;
}

uint8_t set_parameter_value(uint8_t ParamID, uint8_t Value) {
  HAL_StatusTypeDef hal_err;
  if (ParamID == PARAM_SCK_DURATION) {
    gCurrentSckDuration = Value;

    if ((hal_err = SPI_SetFrequency(get_freq_from_sck_period_value(Value))) !=
        HAL_OK) {
      return 1;
    };
    return 0;
  }

  return 0; // Ok if parameter is not supported
}

uint8_t get_sck_period_value_from_freq(uint32_t Frequency) {
  // Algorithm directly inferred from avrdude's source code (stk500v2.c,
  // stk500v2_set_sck_period)

  uint32_t value;

  if (Frequency >= STK500V2_XTAL / 4.0f) {
    value = 0;
  } else if (Frequency >= STK500V2_XTAL / 16.0f) {
    value = 1;
  } else if (Frequency >= STK500V2_XTAL / 64.0f) {
    value = 2;
  } else if (Frequency >= STK500V2_XTAL / 128.0f) {
    value = 3;
  } else {
    // Rearange of this equation (
    // (unsigned int) ceil(1/(24*f/(double) my.xtal) - 10.0/12.0)
    // ) to make it integer only:
    value = (1843200 + Frequency - 1) / (6 * Frequency);
  }

  if (value >= 255)
    return 254;

  return value;
}
uint32_t get_freq_from_sck_period_value(uint8_t Value) {
  if (Value == 0) {
    return STK500V2_XTAL / 4.0f;
  } else if (Value == 1) {
    return STK500V2_XTAL / 16.0f;
  } else if (Value == 2) {
    return STK500V2_XTAL / 64.0f;
  } else if (Value == 3) {
    return STK500V2_XTAL / 128.0f;
  } else {
    return (1843200 + (3 * Value) + 2) / ((6 * Value) + 5);
  }
}

void avr_enable_reset() {
  GPIO_InitTypeDef gpio_conf = {
      .Mode = GPIO_MODE_OUTPUT_PP,
      .Pin = GPIO_PIN_AVR_RESET,
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_LOW,
  };
  HAL_GPIO_Init(GPIO_PORT_AVR_RESET, &gpio_conf);
  HAL_GPIO_WritePin(GPIO_PORT_AVR_RESET, GPIO_PIN_AVR_RESET, GPIO_PIN_RESET);
}

void avr_disable_reset() {
  GPIO_InitTypeDef gpio_conf = {
      .Mode = GPIO_MODE_INPUT,
      .Pin = GPIO_PIN_AVR_RESET,
      .Pull = GPIO_PULLUP,
      .Speed = GPIO_SPEED_FREQ_LOW,
  };
  // Reset into input mode pull-up
  HAL_GPIO_Init(GPIO_PORT_AVR_RESET, &gpio_conf);
}