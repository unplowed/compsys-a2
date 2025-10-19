#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "job_queue.h"
#include "pthread.h"

int job_queue_init(struct job_queue *job_queue, int capacity) {
  // Invalid parameters given to function
  if (job_queue == NULL || capacity <= 0)
    return -1;

  job_queue->capacity = capacity;
  job_queue->size = 0;

  job_queue->data = calloc((size_t)capacity, sizeof(void *));
  if (job_queue->data == NULL)
    return -1;

  // memory allocation
  job_queue->mutex = malloc(sizeof(pthread_mutex_t));
  job_queue->cond_job_popped = malloc(sizeof(pthread_cond_t));
  job_queue->cond_job_pushed = malloc(sizeof(pthread_cond_t));

  // check that mutex and conditions where allocated
  if (!job_queue->mutex || !job_queue->cond_job_popped ||
      !job_queue->cond_job_pushed) {
    free(job_queue->data);
    return -1;
  }

  if (pthread_mutex_init(job_queue->mutex, NULL) != 0)
    return -1;
  if (pthread_cond_init(job_queue->cond_job_popped, NULL) != 0)
    return -1;
  if (pthread_cond_init(job_queue->cond_job_pushed, NULL) != 0)
    return -1;

  job_queue->destroyed = 0;
  return 0;
}

int job_queue_destroy(struct job_queue *job_queue) {
  if (job_queue == NULL)
    return -1;

  if (pthread_mutex_lock(job_queue->mutex) != 0)
    return -1;

  job_queue->destroyed = 1;

  // Wake any threads waiting for conditions
  pthread_cond_broadcast(job_queue->cond_job_pushed);
  pthread_cond_broadcast(job_queue->cond_job_popped);

  while (job_queue->size > 0) {
    if (pthread_cond_wait(job_queue->cond_job_popped, job_queue->mutex) != 0)
      return -1;
  }

  // unlocking
  if (pthread_mutex_unlock(job_queue->mutex) != 0)
    return -1;

  // clean up of OS ressources + heap memory allocations.
  // asserts are used since if any conditions or mutexes are locked
  // at this point the jobs werent cleaned up properly previously
  assert(pthread_cond_destroy(job_queue->cond_job_pushed) == 0);
  assert(pthread_cond_destroy(job_queue->cond_job_popped) == 0);
  assert(pthread_mutex_destroy(job_queue->mutex) == 0);

  // freeding allocated memory
  free(job_queue->cond_job_pushed);
  free(job_queue->cond_job_popped);
  free(job_queue->mutex);
  free(job_queue->data);

  job_queue->data = NULL;
  job_queue->capacity = 0;
  job_queue->size = 0;

  return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data) {
  if (job_queue == NULL)
    return -1;

  if (pthread_mutex_lock(job_queue->mutex) != 0)
    return -1;

  if (job_queue->destroyed) {
    pthread_mutex_unlock(job_queue->mutex);
    return -1;
  }

  // Wait while full -> handle wakeups & recheck destroyed
  while (!job_queue->destroyed && job_queue->size >= job_queue->capacity) {
    if (pthread_cond_wait(job_queue->cond_job_popped, job_queue->mutex) != 0)
      return -1;
  }
  if (job_queue->destroyed) {
    pthread_mutex_unlock(job_queue->mutex);
    return -1;
  }

  // Enqueue data to job_queue (LIFO-Style)
  job_queue->data[job_queue->size++] = data;

  if (pthread_cond_signal(job_queue->cond_job_pushed) != 0)
    return -1;
  if (pthread_mutex_unlock(job_queue->mutex) != 0)
    return -1;

  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
  // checkers
  if (job_queue == NULL || data == NULL)
    return -1;

  if (pthread_mutex_lock(job_queue->mutex) != 0)
    return -1;

  // Wait while empty, but bail if destroyed and still empty
  while (!job_queue->destroyed && job_queue->size == 0) {
    if (pthread_cond_wait(job_queue->cond_job_pushed, job_queue->mutex) != 0)
      return -1;
  }

  if (job_queue->destroyed && job_queue->size == 0) {
    pthread_mutex_unlock(job_queue->mutex);
    return -1;
  }

  // Dequeue Data fra job_queue (LIFO Style)
  *data = job_queue->data[--job_queue->size];

  // Signal to pusher that it there is space & destroy() to progress towards
  // size==0
  if (pthread_cond_signal(job_queue->cond_job_popped) != 0)
    return -1;
  if (pthread_mutex_unlock(job_queue->mutex) != 0)
    return -1;

  return 0;
}
