#include "api.h"
#include "common/constants.h"
#include "common/io.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <endian.h>

int req_pipe_fd;
int resp_pipe_fd;
const char* req_path;
const char* resp_path;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  // Open the server pipe for writing
  int server_fd = open(server_pipe_path, O_WRONLY);
  if (server_fd == -1) {
    perror("open");
    return 1;
  }

  req_path = req_pipe_path;
  resp_path = resp_pipe_path;

  // Create the request pipe
  if (mkfifo(req_pipe_path, 0666) == -1) {
    perror("mkfifo");
    return 1;
  }

  // Create the response pipe
  if (mkfifo(resp_pipe_path, 0666) == -1) {
    perror("mkfifo");
    unlink(req_pipe_path); // Clean up on failure
    return 1;
  }

  // Write the paths of the request and response pipes to the server pipe
  char op = EMS_OP_CODE_SETUP;
  if (write(server_fd, &op, sizeof(char)) == -1) {
    perror("write");
    close(server_fd); // Clean up on failure
    return 1;
  }

  if (write(server_fd, req_pipe_path, sizeof(req_pipe_path)) == -1) {
    perror("write");
    close(server_fd); // Clean up on failure
    return 1;
  }
  sleep(3);
  if (write(server_fd, resp_pipe_path, sizeof(resp_pipe_path)) == -1) {
    perror("write");
    close(server_fd); // Clean up on failure
    return 1;
  }

  // Open the pipe
  req_pipe_fd = open(req_pipe_path, O_WRONLY); 
  if (req_pipe_fd == -1) {
    perror("Error opening request pipe");
    return 1;
  }
  
  // Open the pipe
  resp_pipe_fd = open(resp_pipe_path, O_RDONLY);
  if (resp_pipe_fd == -1) {
    perror("Error opening response pipe");
    return 1;
  }

  // Close the server pipe
  close(server_fd);
  
  return 0;
}

int ems_quit(void) {
  char op = EMS_OP_CODE_QUIT;
  if (write(req_pipe_fd, &op, sizeof(char)) == -1) {
    perror("write");
    return 1;
  }
  // Close the request pipe
  if (close(req_pipe_fd) == -1) {
    perror("close");
    return 1;
  }

  // Close the response pipe
  if (close(resp_pipe_fd) == -1) {
    perror("close");
    return 1;
  }

  unlink(req_path);
  unlink(resp_path);

  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char op = EMS_OP_CODE_CREATE;
  if (write(req_pipe_fd, &op, sizeof(char)) == -1) {
    perror("write");
    return 1;
  }

  if ((write(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) |
      (write(req_pipe_fd, &num_rows, sizeof(size_t)) == -1) |
      (write(req_pipe_fd, &num_cols, sizeof(size_t)) == -1)) {
    perror("write");
    return 1;
  }

  // Wait for the response
  int returned;
  if (read(resp_pipe_fd, &returned, sizeof(int)) == -1) {
    perror("read");
    return 1;
  }

  if (returned == 1) {
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  // Prepare the reserve request
  char op = EMS_OP_CODE_RESERVE;
  if ((write(req_pipe_fd, &op, sizeof(char)) == -1) |
      (write(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) |
      (write(req_pipe_fd, &num_seats, sizeof(size_t)) == -1)) {
    perror("write");
    return 1;
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (write(req_pipe_fd, &xs[i], sizeof(size_t)) == -1) {
      perror("write");
      return 1;
    }
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (write(req_pipe_fd, &ys[i], sizeof(size_t)) == -1) {
      perror("write");
      return 1;
    }
  }

  int returned;

  if (read(resp_pipe_fd, &returned, sizeof(int)) == -1) {
    perror("read");
    return 1;
  }

  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  char op = EMS_OP_CODE_SHOW;
  if (write(req_pipe_fd, &op, sizeof(char)) == -1) {
    perror("write");
    return 1;
  }

  if (write(req_pipe_fd, &event_id, sizeof(unsigned int)) == -1) {
    perror("write");
    return 1;
  }

  // Wait for the response
  int returned;
  if (read(resp_pipe_fd, &returned, sizeof(int)) == -1) {
    perror("read");
    return 1;
  }

  if (returned == 0) {
    int rows, cols, test;

    // fixes error we don't understand where it comes from
    if ((read(resp_pipe_fd, &test, sizeof(int)) == -1)) {
      perror("read");
      return 1;
    }

    if ((read(resp_pipe_fd, &rows, sizeof(int)) == -1)) {
      perror("read");
      return 1;
    }
    if ((read(resp_pipe_fd, &cols, sizeof(int)) == -1)) {
      perror("read");
      return 1;
    }
    
    unsigned int value;

    for (int i = 0; i < rows; i++) {
      for (int j = 0; j < cols; j++) {
        char buff[256];
        if (read(resp_pipe_fd, &value, sizeof(unsigned int)) == -1) {
          perror("read");
          return 1;
        }

        snprintf(buff, sizeof(buff), "%u", value);

        if (print_str(out_fd, buff) == -1) {
          perror("write");
          return 1;
        }

        if (j != cols - 1) {
          if (print_str(out_fd, " ")) {
            perror("write");
            return 1;
          }
        }
      }
      
      if (print_str(out_fd, "\n")) {
        perror("write");
        return 1;
      }
    }
  }
  return 0;
}

int ems_list_events(int out_fd) {
  char op = EMS_OP_CODE_LIST;
  
  if (write(req_pipe_fd, &op, sizeof(char)) == -1) {
    perror("read");
    return 1;
  }

  int returned;
  if (read(resp_pipe_fd, &returned, sizeof(int)) == -1) {
    perror("read");
    return 1;
  }

  if (returned == 0) {
    size_t num_events;

    if (read(resp_pipe_fd, &num_events, sizeof(size_t)) == -1) {
      perror("read");
      return 1;
    }

    if ((int)num_events == 0) {
      if (print_str(out_fd, "No events\n")) {
        perror("Error writing to file descriptor");
        return 1;
      }
      return 0;
    }
    

    for (size_t i = 0; i < num_events; i++) {
      char buff[256] = "Event: ";
      if (print_str(out_fd, buff)) {
        perror("Error writing to file descriptor");
        return 1;
      }

      unsigned int id;
      if (read(resp_pipe_fd, &id, sizeof(unsigned int)) == -1) {
        perror("read");
        return 1;
      }

      snprintf(buff, sizeof(buff), "%u", id);
      if ((print_str(out_fd, buff) == -1) |
          (print_str(out_fd, "\n") == -1)) {
        perror("Error writing to file descriptor");
        return 1;
      }
    }
  }

  return 0;
}