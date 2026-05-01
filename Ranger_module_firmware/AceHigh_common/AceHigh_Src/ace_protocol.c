/*
 * AceLight_protocol.c
 *
 *  Created on: Apr 27, 2026
 *      Author: Tor Kaufmann Gjerde
 *
 *      The AceLight_protocol.c owns the protocol command decoding
 */


#include "ace_protocol.h"

void ace_decode_command(const uint8_t data[8], ace_command_frame_t *frame)
{
  frame->command_id = data[0];
  frame->parameter_id = data[1];

  for (uint8_t i = 0; i < 6U; i++)
  {
    frame->payload[i] = data[2U + i];
  }
}
