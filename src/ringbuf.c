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

  for (size_t i = 1; i <= message_len; i++) {
    context->begin[i] = ((char *)message)[i];
    context->write += 1;
  }
  return SUCCESS;
}

int ringbuffer_read(rbctx_t *context, void *buffer, size_t *buffer_len) {
  /* your solution here */
}

void ringbuffer_destroy(rbctx_t *context) { /* your solution here */ }
