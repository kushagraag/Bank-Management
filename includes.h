#include <stdio.h> // Import for `printf` & `perror` functions
#include <errno.h> // Import for `errno` variable
#include <fcntl.h>      // Import for `fcntl` functions
#include <unistd.h>     // Import for `fork`, `fcntl`, `read`, `write`, `lseek, `_exit` functions
#include <string.h>  // Import for string functions
#include <stdbool.h> // Import for `bool` data type
#include <stdlib.h>  // Import for `atoi` function

#include <netinet/ip.h> // Import for `sockaddr_in` stucture

#include <sys/types.h>  // Import for `socket`, `bind`, `listen`, `accept`, `fork`, `lseek` functions
#include <sys/socket.h> // Import for `socket`, `bind`, `listen`, `accept` functions
#include <sys/stat.h>  // Import for `open`
#include <sys/ipc.h>
#include <sys/sem.h>


#include "./varsNConstants.h"
#include "./structs.h"