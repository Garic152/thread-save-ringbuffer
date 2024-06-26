#include "../include/ringbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void ringbuffer_init(rbctx_t *context, void *buffer_location,
                     size_t buffer_size) {
  context->begin = (uint8_t *)buffer_location;
  context->end = context->begin + buffer_size;
  context->read = context->begin;
  context->write = context->begin;
}

int ringbuffer_write(rbctx_t *context, void *message, size_t message_len) {
  if (message_len >= context->end - context->begin) {
    return OUTPUT_BUFFER_TOO_SMALL;
  }
  context->write[0] = (uint8_t)message_len;
  context->write += 1;

  for (size_t i = 0; i < message_len; i++) {
    context->write[0] = ((char *)message)[i];
    context->write += 1;

    if (context->write >= context->end) {
      context->write = context->begin;
    }
  }
  return SUCCESS;
}

int ringbuffer_read(rbctx_t *context, void *buffer, size_t *buffer_len) {
  if (context->read == context->write) {
    return RINGBUFFER_EMPTY; // temporary solution
  }

  uint8_t message_len = context->read[0];
  if (message_len > *buffer_len) {
    return OUTPUT_BUFFER_TOO_SMALL;
  }
  context->read += 1;

  for (size_t i = 0; i < message_len; i++) {
    ((uint8_t *)buffer)[i] = context->read[0];
    context->read += 1;

    if (context->read >= context->end) {
      context->read = context->begin;
    }
  }
  return SUCCESS;
}

void ringbuffer_destroy(rbctx_t *context) { free(context); }
