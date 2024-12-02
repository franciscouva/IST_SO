#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/select.h>
#include <errno.h>

#include "common/constants.h"
#include "eventlist.h"
#include "common/io.h"
#include "operations.h"
#include "safethreads.h"


typedef struct {
  int session_id;
  int req_pipe_fd;
  int resp_pipe_fd;
  char* req_pipe_path;
  char* resp_pipe_path;
  // pthread_t tid;
  pthread_mutex_t lock;
} client_t;

typedef struct {
  int session_id;
  bool to_execute;
  client_t client; 
  pthread_t tid;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} worker_t;

int server_init(char const* server_pipe_path);
int handle_client_requests(void* arg);
int get_available_worker();
int free_worker(int session_id);
client_t create_client(char* server_pipe_path);
void assign_worker(client_t client);
void* thread_start(void* arg);

int server_fd;
static worker_t workers[MAX_SESSION_COUNT];
static bool free_workers[MAX_SESSION_COUNT];
static pthread_mutex_t free_worker_lock;
client_t producer_consumer_buffer[10];
int prodptr=0, consptr=0, count=0;
pthread_mutex_t mutex;
pthread_cond_t podeProd = PTHREAD_COND_INITIALIZER;
pthread_cond_t podeCons = PTHREAD_COND_INITIALIZER; 

// Replace accept_client with producer
void accept_client(char* server_pipe_path) {
  client_t client = create_client(server_pipe_path);
  mutex_lock(&mutex);
  while (count == MAX_SESSION_COUNT) 
    pthread_cond_wait(&podeProd, &mutex);
  producer_consumer_buffer[prodptr] = client;
  prodptr++; 
  if(prodptr == MAX_SESSION_COUNT) 
    prodptr = 0;
  count++;
  pthread_cond_signal(&podeCons);
  mutex_unlock(&mutex);
}

// Replace assign_worker with consumer
void* consumer() {
  client_t client;
  mutex_lock(&mutex);
  while (count == 0) 
    pthread_cond_wait(&podeCons, &mutex);
  client = producer_consumer_buffer[consptr];
  consptr++; 
  if(consptr == MAX_SESSION_COUNT) 
    consptr = 0;
  count--;
  pthread_cond_signal(&podeProd);
  mutex_unlock(&mutex);
  assign_worker(client);
  return NULL;
}


int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* server_pipe_path = argv[1];

  if (server_init(server_pipe_path)) {
    fprintf(stderr, "Failed to initialize server\n");
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      unlink(server_pipe_path);
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    unlink(server_pipe_path);
    return 1;
  }

  while (1) {
    
    accept_client(server_pipe_path);
    consumer();

    // block server main thread until client connects
    close(server_fd);
    server_fd = open(server_pipe_path, O_RDONLY);
    if (server_fd == -1) {
      perror("open");
      unlink(server_pipe_path);
      return 1;
    }
    
  }

  unlink(server_pipe_path);
  ems_terminate();
}

int handle_client_requests(void* arg) {
  worker_t* worker = (worker_t*)arg;
  int req_pipe_fd = (worker->client).req_pipe_fd;
  int resp_pipe_fd = (worker->client).resp_pipe_fd;
  while (1) {

    int op_code;
    if (read(req_pipe_fd, &op_code, sizeof(char)) == -1) {
      perror("read");
      return 1;
    }

    if (op_code == EMS_OP_CODE_CREATE) {
      // Parse the event_id, num_rows, and num_cols from the request
      unsigned int event_id;
      size_t num_rows, num_cols;
      // Parse the request and perform the requested operation
      
      if ((read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) | 
          (read(req_pipe_fd, &num_rows, sizeof(size_t)) == -1) | 
          (read(req_pipe_fd, &num_cols, sizeof(size_t)) == -1)) {
        perror("read");
        return 1;
      }


      int result = ems_create(event_id, num_rows, num_cols);
      if (write(resp_pipe_fd, &result, sizeof(result)) == -1) {
        perror("write");
      }

    } else if (op_code == EMS_OP_CODE_RESERVE) {
      unsigned int event_id;
      size_t num_seats;

      if ((read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) | 
          (read(req_pipe_fd, &num_seats, sizeof(size_t)) == -1)) {
        perror("read");
        return 1;
      }

      // Allocate memory for the seat positions
      size_t xs[num_seats];
      size_t ys[num_seats];

      // Parse the seat positions from the request
      for (size_t i = 0; i < num_seats; i++) {
        if (read(req_pipe_fd, &xs[i], sizeof(size_t)) == -1) {
          perror("read");
          return 1;
        }
      }

      for (size_t i = 0; i < num_seats; i++) {
        if (read(req_pipe_fd, &ys[i], sizeof(size_t)) == -1) {
          perror("read");
          return 1;
        }
      }

      // Perform the reservation operation
      int result = ems_reserve(event_id, num_seats, xs, ys);

      // Write the response to the response pipe
      if (write(resp_pipe_fd, &result, sizeof(int)) == -1) {
        perror("write");
      }

    } else if (op_code == EMS_OP_CODE_SHOW) {
      // Parse the event_id, num_rows, and num_cols from the request
      unsigned int event_id;
      // Parse the request and perform the requested operation
      
      if (read(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
        perror("read");
        return 1;
      }

      ems_show(resp_pipe_fd, event_id);

    } else if (op_code == EMS_OP_CODE_LIST) {

      ems_list_events(resp_pipe_fd);

    } else if (op_code == EMS_OP_CODE_QUIT) {

      close(req_pipe_fd);
      close(resp_pipe_fd);
      return 0;
    }
  }
}



int server_init(char const* server_pipe_path) {
  // Create the server pipe
  if (mkfifo(server_pipe_path, 0666) == -1) {
    perror("mkfifo");
    return 1;
  }

  // Open the server pipe for reading
  server_fd = open(server_pipe_path, O_RDONLY);
  if (server_fd == -1) {
    perror("open");
    unlink(server_pipe_path); // Clean up on failure
    return 1;
  }


  for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
    workers[i].session_id = i;
    workers[i].to_execute = false;
    mutex_init(&workers[i].lock);
    if (pthread_cond_init(&workers[i].cond, NULL) != 0) {
      return 1;
    }
    if (pthread_create(&workers[i].tid, NULL, thread_start,
                      &workers[i]) != 0) {
      return 1;
    }
    free_workers[i] = false;
  }
  mutex_init(&free_worker_lock);
  return 0;
}

void* thread_start(void* arg) {
  worker_t* worker = (worker_t*)arg;
  pthread_cond_wait(&worker->cond, &worker->lock);
  handle_client_requests(worker);
  free_worker(worker->session_id);
  return NULL;
}

int get_available_worker() {
  mutex_lock(&mutex);
  for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
    if (free_workers[i] == false) {
      free_workers[i] = 1;
      mutex_unlock(&mutex);
      return i;
    }
  }
  mutex_unlock(&free_worker_lock);
  return -1;
}

int free_worker(int session_id) {
  mutex_lock(&free_worker_lock);
  if (free_workers[session_id] == false) {
    mutex_unlock(&free_worker_lock);
    return 1;
  }
  free_workers[session_id] = false;
  workers[session_id].client = (client_t){0};
  pthread_cond_wait(&workers[session_id].cond, &workers[session_id].lock);
  mutex_unlock(&free_worker_lock);
  return 0;
}

client_t create_client(char* server_pipe_path) {
  char op_code;
  client_t client = {0};
  
  if (read(server_fd, &op_code, sizeof(char)) == -1) {
    perror("read");
    close(server_fd);
    unlink(server_pipe_path); // Clean up on failure
    return client;
  }

  if (op_code == EMS_OP_CODE_SETUP) {
    // Read the client pipe paths
    char req_pipe_path[40] = ""; // Initialize with an empty string
    char resp_pipe_path[40] = ""; // Initialize with an empty string

    if (read(server_fd, req_pipe_path, sizeof(req_pipe_path)) == -1) {
      perror("read");
      close(server_fd);
      unlink(server_pipe_path); // Clean up on failure
      return client;
    }
    if (read(server_fd, resp_pipe_path, sizeof(resp_pipe_path)) == -1) {
      perror("read");
      close(server_fd);
      unlink(server_pipe_path); // Clean up on failure
      return client;
    }
    
    if (req_pipe_path[0] == '\0' || resp_pipe_path[0] == '\0') {
      return client;
    }

    client.req_pipe_path = req_pipe_path;
    client.resp_pipe_path = resp_pipe_path;
    

    // Open the request and response pipes
    int req_pipe_fd = open(req_pipe_path, O_RDONLY);
    int resp_pipe_fd = open(resp_pipe_path, O_WRONLY);

    // Check for errors in opening pipes
    if (req_pipe_fd == -1 || resp_pipe_fd == -1) {
      perror("open");
      unlink(req_pipe_path);  // Clean up on failure
      unlink(resp_pipe_path); // Clean up on failure
      return client;
    }

    client.req_pipe_fd = req_pipe_fd;
    client.resp_pipe_fd = resp_pipe_fd;
 
  }
  return client;
}

void assign_worker(client_t client) {
  // Assign the worker to the client
  int worker_id = get_available_worker();
  if (worker_id == -1) {
    perror("get_available_worker");
    return;
  }
  write(client.resp_pipe_fd, &worker_id, sizeof(int));
  mutex_lock(&workers[worker_id].lock);
  workers[worker_id].client = client;
  workers[worker_id].to_execute = true;
  pthread_cond_signal(&workers[worker_id].cond);
  free_workers[worker_id] = true;
  mutex_unlock(&workers[worker_id].lock);
}