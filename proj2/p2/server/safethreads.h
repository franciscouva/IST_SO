#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

void rwl_wrlock(pthread_rwlock_t *rwl);
void rwl_rdlock(pthread_rwlock_t *rwl);
void rwl_unlock(pthread_rwlock_t *rwl);
void rwl_init(pthread_rwlock_t *rwl);
void rwl_destroy(pthread_rwlock_t *rwl);

void mutex_lock(pthread_mutex_t *mutex);
void mutex_unlock(pthread_mutex_t *mutex);
void mutex_init(pthread_mutex_t *mutex);
void mutex_destroy(pthread_mutex_t *mutex);

#endif // UTILS_H