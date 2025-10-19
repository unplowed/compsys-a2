#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "job_queue.h"
#include "pthread.h"

int job_queue_init(struct job_queue *job_queue, int capacity) {

  //explain this part:
  if(!job_queue || capacity <= 0)return -1;


  job_queue->capacity = capacity;
  job_queue->size = 0;

  job_queue->data = calloc((size_t)capacity, sizeof(void *));
  if (!job_queue->data) return -1;

  // memory allocation
  job_queue->mutex = malloc(sizeof(pthread_mutex_t));
  job_queue->cond_job_popped = malloc(sizeof(pthread_cond_t));
  job_queue->cond_job_pushed = malloc(sizeof(pthread_cond_t));

  // some conditionals
  if(!job_queue->mutex || !job_queue->cond_job_popped || !job_queue->cond_job_pushed){
    free(job_queue->data);
    return -1;
  }

  if(pthread_mutex_init(job_queue->mutex, NULL) != 0) return -1;
  if(pthread_cond_init(job_queue->cond_job_popped, NULL)!= 0) return -1;
  if(pthread_cond_init(job_queue->cond_job_pushed, NULL)!= 0) return -1;

  job_queue->destroyed = 0;
  

/*
  if (pthread_mutex_init(job_queue->mutex, NULL) < 0)
    return -1;

  job_queue->cond_job_popped = malloc(sizeof(pthread_cond_t));
  if (pthread_cond_init(job_queue->cond_job_popped, NULL) < 0)
    return -1;

  job_queue->cond_job_pushed = malloc(sizeof(pthread_cond_t));
  if (pthread_cond_init(job_queue->cond_job_pushed, NULL) < 0)
    return -1;

  job_queue->destroyed = 0;
 */
  return 0;
}

int job_queue_destroy(struct job_queue *job_queue) {
  if(!job_queue) return -1;

  if(pthread_mutex_lock(job_queue->mutex) !=0) return -1;
  
  job_queue->destroyed = 1;

  pthread_cond_broadcast(job_queue->cond_job_pushed);
  pthread_cond_broadcast(job_queue->cond_job_popped);


  while (job_queue->size > 0)
  {
    pthread_cond_wait(job_queue->cond_job_popped, job_queue->mutex);

  }

  // unlocking
  pthread_mutex_unlock(job_queue->mutex);

  // clean up of OS ressources + heap memory allocations.
  pthread_cond_destroy(job_queue->cond_job_pushed);
  pthread_cond_destroy(job_queue->cond_job_popped);
  pthread_mutex_destroy(job_queue->mutex);

  // freeding allocated memory
  free(job_queue->cond_job_pushed);
  free(job_queue->cond_job_popped);
  free(job_queue->mutex);
  free(job_queue->data);

  job_queue->data = NULL;
  job_queue->capacity = 0;
  job_queue->size = 0;
  
  
  /*job_queue->destroyed = 1;

  while (job_queue->size != 0) {
  }

  if (pthread_cond_broadcast(job_queue->cond_job_pushed) < 0)
    return -1;
  */
  return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data) {
 if(!job_queue) return -1;
 if(pthread_mutex_lock(job_queue->mutex) != 0) return -1;

 if(job_queue->destroyed)
 {
  pthread_mutex_unlock(job_queue->mutex);
  return -1;
 }
 

 /*Wait while full -> handle wakeups & recheck destroyed */
 while (!job_queue->destroyed && job_queue->size >= job_queue->capacity)
 {
  pthread_cond_wait(job_queue->cond_job_popped, job_queue->mutex);
 }
 if(job_queue->destroyed)
 {
    pthread_mutex_unlock(job_queue->mutex);
    return -1;
 }

 // Enqueue data to job_queue (LIFO-Style)
 job_queue->data[job_queue->size++] = data;

 pthread_cond_signal(job_queue->cond_job_pushed);
 pthread_mutex_unlock(job_queue->mutex);
 return 0; 
 
  /*
  if (pthread_mutex_lock(job_queue->mutex) < 0)
    return -1;

  if (job_queue->destroyed) {
    pthread_mutex_unlock(job_queue->mutex);
    return -1;
  }

  if (job_queue->size > job_queue->capacity) {
    // block until an element is popped
    if (pthread_cond_wait(job_queue->cond_job_popped, job_queue->mutex) < 0)
      return -1;
  }

  job_queue->data[job_queue->size++] = data;

  if (pthread_mutex_unlock(job_queue->mutex) < 0)
    return -1;
  if (pthread_cond_signal(job_queue->cond_job_pushed) < 0)
    return -1;

    return 0; 
*/
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
  // checkers
  if(!job_queue || !data) return -1;
  if(pthread_mutex_lock(job_queue->mutex) != 0) return -1;


  /*Wait while empty, but bail if destroyed and still empty*/
  while (!job_queue->destroyed && job_queue->size == 0)
  {
    pthread_cond_wait(job_queue->cond_job_pushed,job_queue->mutex);
  }

  if (job_queue->destroyed && job_queue-> size == 0)
  {
    pthread_mutex_unlock(job_queue->mutex);
    return -1;
  }

  /*Dequeue Data fra job_queue (LIFO Style) */
  *data = job_queue->data[--job_queue->size];

  // Signal to pusher that it there is space & destroy() to progress towards size==0
  pthread_cond_signal(job_queue->cond_job_popped);

  pthread_mutex_unlock(job_queue->mutex);
  return 0;


  /*
  if (pthread_mutex_lock(job_queue->mutex) < 0)
    return -1;

  if (job_queue->size == 0) {
    if (job_queue->destroyed) {
      pthread_mutex_unlock(job_queue->mutex);
      return -1;
    }

    // block until an element is pushed
    if (pthread_cond_wait(job_queue->cond_job_pushed, job_queue->mutex) < 0)
      return -1;

    if (job_queue->destroyed) {
      pthread_mutex_unlock(job_queue->mutex);
      return -1;
    }
  }

  *data = job_queue->data[--job_queue->size];

  if (pthread_mutex_unlock(job_queue->mutex) < 0)
    return -1;
  if (pthread_cond_signal(job_queue->cond_job_popped) < 0)
    return -1;

  return 0;
  */
}
