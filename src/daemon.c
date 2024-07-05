#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/daemon.h"
#include "../include/ringbuf.h"

/* IN THE FOLLOWING IS THE CODE PROVIDED FOR YOU
 * changing the code will result in points deduction */

/********************************************************************
 * NETWORK TRAFFIC SIMULATION:
 * This section simulates incoming messages from various ports using
 * files. Think of these input files as data sent by clients over the
 * network to our computer. The data isn't transmitted in a single
 * large file but arrives in multiple small packets. This concept
 * is discussed in more detail in the advanced module:
 * Rechnernetze und Verteilte Systeme
 *
 * To simulate this parallel packet-based data transmission, we use multiple
 * threads. Each thread reads small segments of the files and writes these
 * smaller packets into the ring buffer. Between each packet, the
 * thread sleeps for a random time between 1 and 100 us. This sleep
 * simulates that data packets take varying amounts of time to arrive.
 *********************************************************************/
typedef struct {
  rbctx_t *ctx;
  connection_t *connection;
} w_thread_args_t;

void *write_packets(void *arg) {
  /* extract arguments */
  rbctx_t *ctx = ((w_thread_args_t *)arg)->ctx;
  size_t from = (size_t)((w_thread_args_t *)arg)->connection->from;
  size_t to = (size_t)((w_thread_args_t *)arg)->connection->to;
  char *filename = ((w_thread_args_t *)arg)->connection->filename;

  /* open file */
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Cannot open file with name %s\n", filename);
    exit(1);
  }

  /* read file in chunks and write to ringbuffer with random delay */
  unsigned char buf[MESSAGE_SIZE];
  size_t packet_id = 0;
  size_t read = 1;
  while (read > 0) {
    size_t msg_size = MESSAGE_SIZE - 3 * sizeof(size_t);
    read = fread(buf + 3 * sizeof(size_t), 1, msg_size, fp);
    if (read > 0) {
      memcpy(buf, &from, sizeof(size_t));
      memcpy(buf + sizeof(size_t), &to, sizeof(size_t));
      memcpy(buf + 2 * sizeof(size_t), &packet_id, sizeof(size_t));
      while (ringbuffer_write(ctx, buf, read + 3 * sizeof(size_t)) != SUCCESS) {
        usleep(((rand() % 50) +
                25)); // sleep for a random time between 25 and 75 us
      }
    }
    packet_id++;
    usleep(((rand() % (100 - 1)) +
            1)); // sleep for a random time between 1 and 100 us
  }
  fclose(fp);
  return NULL;
}

/* END OF PROVIDED CODE */

/********************************************************************/

/* YOUR CODE STARTS HERE */

// 1. read functionality
// 2. filtering functionality
// 3. (thread-safe) write to file functionality

typedef struct {
  size_t expected_packet_id;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} port_info;

port_info ports[MAXIMUM_PORT];

void *read_packets(void *arg) {
  rbctx_t *ctx = (rbctx_t *)arg;

  unsigned char buf[MESSAGE_SIZE];
  size_t buf_len = sizeof(buf);

  while (ringbuffer_read(arg, buf, &buf_len) != SUCCESS) {
    printf("Ringbuffer empty in read_packets");
    return NULL;
  }

  size_t from, to, packet_id;

  memcpy(&from, buf, sizeof(size_t));
  memcpy(&to, buf + sizeof(size_t), sizeof(size_t));
  memcpy(&packet_id, buf + 2 * sizeof(size_t), sizeof(size_t));

  if (to < MAXIMUM_PORT) {
    port_info *port = &ports[to];
    printf("\nFrom: %zu\nTo:%zu\nPacket_ID:%zu\n", from, to, packet_id);

    if (from == to || from == 42 || to == 42 || from + to == 42) {
      port->expected_packet_id += 1;
      return NULL;
    }

    char message[buf_len - 3 * sizeof(size_t)];
    memcpy(message, buf + 3 * sizeof(size_t), buf_len - 3 * sizeof(size_t));

    // scan for "malicious"
    char *malicious = "malicious";
    for (size_t i = 0; i < MESSAGE_SIZE - 3 * sizeof(size_t); i++) {
      if (message[i] == *malicious) {
        if (*malicious == 's') {
          printf("MALICOUS CONTENT FOUND!\n");
          port->expected_packet_id += 1;
          printf("Next expected packet: %zu\n", port->expected_packet_id);
          return NULL;
        }
        malicious++;
      }
    }

    // write to file

    pthread_mutex_lock(&port->mutex);

    printf("Now writing to port %zu\n", to);

    while (packet_id != port->expected_packet_id) {
      printf("Waiting for port %zu\n", to);
      pthread_cond_wait(&port->cond, &port->mutex);
    }

    char filename[10];
    snprintf(filename, sizeof(filename), "%zu.txt", to);
    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
      fprintf(stderr, "Cannot open file with name %s\n", filename);
      exit(1);
    }
    fwrite(message, 1, buf_len - 3 * sizeof(size_t), fp);
    fclose(fp);

    port->expected_packet_id += 1;
    pthread_cond_broadcast(&port->cond);

    pthread_mutex_unlock(&port->mutex);
  } else {
    printf("Unexpected port number '%zu', exiting", to);
    exit(EXIT_FAILURE);
  }

  return NULL;
}
/* YOUR CODE ENDS HERE */

/********************************************************************/

int simpledaemon(connection_t *connections, int nr_of_connections) {
  /* initialize ringbuffer */
  rbctx_t rb_ctx;
  size_t rbuf_size = 1024;
  void *rbuf = malloc(rbuf_size);
  if (rbuf == NULL) {
    fprintf(stderr, "Error allocation ringbuffer\n");
  }

  ringbuffer_init(&rb_ctx, rbuf, rbuf_size);

  /****************************************************************
   * WRITER THREADS
   * ***************************************************************/

  /* prepare writer thread arguments */
  w_thread_args_t w_thread_args[nr_of_connections];
  for (int i = 0; i < nr_of_connections; i++) {
    w_thread_args[i].ctx = &rb_ctx;
    w_thread_args[i].connection = &connections[i];
    /* guarantee that port numbers range from MINIMUM_PORT (0) - MAXIMUMPORT */
    if (connections[i].from > MAXIMUM_PORT ||
        connections[i].to > MAXIMUM_PORT ||
        connections[i].from < MINIMUM_PORT ||
        connections[i].to < MINIMUM_PORT) {
      fprintf(stderr, "Port numbers %d and/or %d are too large\n",
              connections[i].from, connections[i].to);
      exit(1);
    }
  }

  /* start writer threads */
  pthread_t w_threads[nr_of_connections];
  for (int i = 0; i < nr_of_connections; i++) {
    pthread_create(&w_threads[i], NULL, write_packets, &w_thread_args[i]);
  }

  /****************************************************************
   * READER THREADS
   * ***************************************************************/
  pthread_t r_threads[NUMBER_OF_PROCESSING_THREADS];

  /* END OF PROVIDED CODE */

  /********************************************************************/

  /* YOUR CODE STARTS HERE */

  // 1. think about what arguments you need to pass to the processing threads
  // 2. start the processing threads

  for (int i = 0; i < MAXIMUM_PORT; i++) {
    ports[i].expected_packet_id = 0;
    pthread_mutex_init(&ports[i].mutex, NULL);
    pthread_cond_init(&ports[i].cond, NULL);
  }

  for (int i = 0; i < NUMBER_OF_PROCESSING_THREADS; i++) {
    pthread_create(&r_threads[i], NULL, read_packets, &rb_ctx);
  }
  /* YOUR CODE ENDS HERE */

  /********************************************************************/

  /* IN THE FOLLOWING IS THE CODE PROVIDED FOR YOU
   * changing the code will result in points deduction */

  /****************************************************************
   * CLEANUP
   * ***************************************************************/

  /* after 5 seconds JOIN all threads (we should definitely have received all
   * messages by then) */
  printf("daemon: waiting for 5 seconds before canceling reading threads\n");
  sleep(5);
  for (int i = 0; i < NUMBER_OF_PROCESSING_THREADS; i++) {
    pthread_cancel(r_threads[i]);
  }

  /* wait for all threads to finish */
  for (int i = 0; i < nr_of_connections; i++) {
    pthread_join(w_threads[i], NULL);
  }

  /* join all threads */
  for (int i = 0; i < NUMBER_OF_PROCESSING_THREADS; i++) {
    pthread_join(r_threads[i], NULL);
  }

  /* END OF PROVIDED CODE */

  /********************************************************************/

  /* YOUR CODE STARTS HERE */

  // use this section to free any memory, destory mutexe etc.
  for (int i = 0; i < MAXIMUM_PORT; i++) {
    pthread_mutex_destroy(&ports[i].mutex);
    pthread_cond_destroy(&ports[i].cond);
  }

  /* YOUR CODE ENDS HERE */

  /********************************************************************/

  /* IN THE FOLLOWING IS THE CODE PROVIDED FOR YOU
   * changing the code will result in points deduction */

  free(rbuf);
  ringbuffer_destroy(&rb_ctx);

  return 0;

  /* END OF PROVIDED CODE */
}
