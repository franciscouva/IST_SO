#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Locks the rwlock to write-read.
 * Exits with failure if unsuccessful.
 */
void rwl_wrlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_wrlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

/*
 * Locks the rwlock to read-only.
 * Exits with failure if unsuccessful.
 */
void rwl_rdlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_rdlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

/*
 * Unlocks the rwlock.
 * Exits with failure if unsuccessful.
 */
void rwl_unlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_unlock(rwl) != 0) {
        perror("Failed to unlock RWlock");
        exit(EXIT_FAILURE);
    }
}

/*
 * Initializes the rwlock.
 * Exits with failure if unsuccessful.
 */
void rwl_init(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_init(rwl, NULL) != 0) {
        perror("Failed to init RWlock");
        exit(EXIT_FAILURE);
    }
}

/*
 * Destroys the rwlock.
 * Exits with failure if unsuccessful.
 */
void rwl_destroy(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_destroy(rwl) != 0) {
        perror("Failed to destroy RWlock");
        exit(EXIT_FAILURE);
    }
}

/*
 * Locks the mutex.
 * Exits with failure if unsuccessful.
 */
void mutex_lock(pthread_mutex_t *mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        perror("Failed to lock Mutex");
        exit(EXIT_FAILURE);
    }
}

/*
 * Unlocks the mutex.
 * Exits with failure if unsuccessful.
 */
void mutex_unlock(pthread_mutex_t *mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        perror("Failed to unlock Mutex");
        exit(EXIT_FAILURE);
    }
}

/*
 * Initializes the mutex.
 * Exits with failure if unsuccessful.
 */
void mutex_init(pthread_mutex_t *mutex) {
    if (pthread_mutex_init(mutex, NULL) != 0) {
        perror("Failed to init Mutex");
        exit(EXIT_FAILURE);
    }
}

/*
 * Destroys the mutex.
 * Exits with failure if unsuccessful.
 */
void mutex_destroy(pthread_mutex_t *mutex) {
    if (pthread_mutex_destroy(mutex) != 0) {
        perror("Failed to destroy Mutex");
        exit(EXIT_FAILURE);
    }
}