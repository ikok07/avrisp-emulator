#include "stk500v2.h"
#include "ringbuf.h"
#include "usb.h"
#include "usbd_def.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static USB_CommandStatusTypeDef
send_response(STK500V2_CommandTypeDef *Stk500Command, uint8_t Status,
              uint8_t *Data, size_t Len);
static uint8_t generate_checksum(uint8_t *Buffer, uint16_t Len);
static uint8_t validate_checksum(uint8_t *Buffer, uint16_t Len);

static uint8_t command_buffer[MIN_COMMAND_SIZE];

USB_CommandStatusTypeDef
STK500V2_HandleCmd(STK500V2_CommandTypeDef *Stk500Command) {
  USB_CommandStatusTypeDef status = USB_COMMAND_OK;

  uint8_t cmd_id = Stk500Command->MessageBody[0];
  switch (cmd_id) {
  case CMD_SIGN_ON:
    uint8_t data[9];
    data[0] = strlen(DEBUGGER_SIGNATURE);
    memcpy(data + 1, DEBUGGER_SIGNATURE, data[0]);
    send_response(Stk500Command, STATUS_CMD_OK, data, sizeof(data));
    break;
  }

  return status;
}

USB_CommandStatusTypeDef
STK500V2_ParseCmd(ringbuf_t RingBuffer,
                  STK500V2_CommandTypeDef *Stk500Command) {
  USB_CommandStatusTypeDef status = USB_COMMAND_OK;

  STK500V2_CommandTypeDef cmd;
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

  cmd.SequenceNumber = header[1];
  cmd.MessageSize = (header[2] << 8) | header[3];
  cmd.Token = header[4];

  // After we know the whole command size we can validate the whole command
  uint16_t cmd_total_size =
      cmd.MessageSize + MIN_COMMAND_SIZE + 1; // Account for the checksum byte
  if (len < cmd_total_size) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  }

  // If command is valid, copy it from the ringbuffer
  ringbuf_memcpy_from(command_buffer, RingBuffer, cmd_total_size);

  // Validate the checksum
  if (!validate_checksum(command_buffer, cmd_total_size)) {
    status = USB_COMMAND_PARSE_ERR;
    return status;
  };

  // Extract the command payload
  cmd.MessageBody = command_buffer + 5;
  cmd.Checksum = command_buffer[cmd.MessageSize + MIN_COMMAND_SIZE];

  // Assign new command to global variable and passed pointer
  memcpy(Stk500Command, &cmd, sizeof(cmd));

  return USB_COMMAND_OK;
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

USB_CommandStatusTypeDef send_response(STK500V2_CommandTypeDef *Stk500Command,
                                       uint8_t Status, uint8_t *Data,
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

  USBD_StatusTypeDef usbd_err;
  if ((usbd_err = USB_SendData(response, response_len)) != USBD_OK) {
    return USB_COMMAND_RESPONSE_ERR;
  }

  return USB_COMMAND_OK;
}