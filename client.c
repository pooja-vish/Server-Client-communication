#define SERVERW24_PORT 4511
#define MIRROR1_PORT 4351
#define MIRROR2_PORT 4356
#define STORAGE_PATH "/home/vishwak/w24project/temp.tar.gz"
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <regex.h>
//declaring global variables 
int verificationOkay = 0, isMessage = 0;

//function to connect to servers based on the portnumber
int connect_server(int portNumber) {
  //create socket
  int sid;
  struct sockaddr_in servAdd;
  if ((sid = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Cannot create socket\n");
    exit(1);
  }
  //add the socket details
  servAdd.sin_family = AF_INET;
  servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
  servAdd.sin_port = htons((uint16_t) portNumber);

  //make the connection
  if (connect(sid, (struct sockaddr * ) & servAdd, sizeof(servAdd)) < 0) {
    fprintf(stderr, "connect() has failed, exiting\n");
    exit(3);
  }
  return sid;
}
//function to validate the date for date format and valid range
//returns 0 if invalid, 1 if valid
int validateDate(char * dateString) {
  int isValid = 1;
  if (strlen(dateString) != 10)
    isValid = 0;

  //check indexes of '-'
  if (dateString[4] != '-' || dateString[7] != '-')
    isValid = 0;

  // verify that values for year is a valid number
  char * endptr;
  int year = strtol(dateString, & endptr, 10);
  if ( * endptr != '-' || year < 0)
    isValid = 0;
   // verify that values for month is valid number
  int month = strtol(dateString + 5, & endptr, 10);
  if ( * endptr != '-' || month < 1 || month > 12)
    isValid = 0;
   // verify that values for day is valid number
  int day = strtol(dateString + 8, & endptr, 10);
  if ( * endptr != '\0' || day < 1 || day > 31)
    isValid = 0;

  //to check if days are valid for given month
  int daysInMonth[] = {
    31,
    28 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)),
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31
  };

  //check if days are valid 
  if (day > daysInMonth[month - 1])
    isValid = 0;
  //return 0 if invalid, 1 if valid 
  return isValid;
}

//function to split the given string to string array based on the symbol
//returns splitted string array
char ** split_command(char * command, char * symbol, int * size) {
  // get the number of elements
  * size = 1;
  for (char * a = command;* a != '\0'; a++) {
    if (strchr(symbol, * a) != NULL) {
      ( * size) ++;
    }
  }
  //create the dynamic array
  char ** command_array = (char ** ) malloc(( * size) * sizeof(char * ));
  if (command_array == NULL) {
    printf("Memory not allocated. Error.Try again.\n");
    exit(EXIT_FAILURE);
  }
  //tokenize the command
  char * element = strtok(command, symbol);
  int i = 0;
  while (element != NULL) {
    command_array[i] = strdup(element);
    if (command_array[i] == NULL) {
      printf("Memory not allocated. Error.Try again.\n");
      exit(EXIT_FAILURE);
    }
    i++;
    element = strtok(NULL, symbol);
  }
  command_array[i] = '\0';
  return command_array;
}
//main method
//program starts here
void main(int argc, char * argv[]) {
  char server_message[256];//to store message from server
  char message[256];
  char cmd[256], cmdCopy[256];//to store the user commands
  int sid, portNumber, pid, n;
  socklen_t len;
  //connnect to main server first using server port
  sid = connect_server(SERVERW24_PORT);
  
  //read the server msg from server if connection is made
  ssize_t bytes;
  if ((bytes = recv(sid, server_message, 256, 0)) == -1) {
    perror("recv");
    close(sid);
    exit(EXIT_FAILURE);
  }
  server_message[bytes] = '\0';
  int port_number;
  //check if server msg is a port number(numerical value), if yes then check the mirror number and connect to mirror
  if (sscanf(server_message, "%d", & port_number) == 1) {
    close(sid);//close the server request
    //if port numbre of mirror 1 is sent, connect to mirror1
    if (port_number == MIRROR1_PORT) {
      if (connect_server(MIRROR1_PORT) > 0);
      printf("Connected to Mirror1 server\n");
    }
    //if port numbre of mirror 2 is sent, connect to mirror2
    if (port_number == MIRROR2_PORT) {
      if (connect_server(MIRROR2_PORT) > 0);
      printf("Connected to Mirror2 server\n");
    }
  } else {//else condition where server sends the response and not port number 
    printf("Connected to Main server\n");
  }
  //fork a process 
  pid = fork();

  if (pid == 0) { //child to read data from stdin and send it to server
    while (1) {
      printf("\nclient$");
      fflush(stdout);
      //format the commands variables
      for (int i = 0; i < 256; i++) {
        cmd[i] = '\0';
        cmdCopy[i] = '\0';
      }
      //read the input from client prompt
      int n = read(0, cmd, 256);
      strcpy(cmdCopy, cmd);
      int cmd_len = strlen(cmd), size;
      //replace the last character with NULL
      if (cmd_len > 0 && cmd[cmd_len - 1] == '\n') {
        cmd[cmd_len - 1] = '\0';
        cmdCopy[cmd_len - 1] = '\0';
      }
      //split the command, calling split_command function
      char ** command_array = split_command(cmd, " ", & size);
      if (strcmp(command_array[0], "dirlist") == 0) {
        if (size == 2) {
          if ((strcmp(command_array[1], "-a") == 0) || (strcmp(command_array[1], "-t") == 0)) {
            strcpy(message, cmdCopy);
            strcat(cmdCopy, "\n");
            verificationOkay = 1;
            isMessage = 1;
          } else {
            printf("Only -a or -t argument allowed!\n");
            continue;
          }
        } else {
          printf("Incomplete dirlist command. Enter arguments!\n");
          continue;
        }
      } else if (strcmp(command_array[0], "w24fn") == 0) {
        if (size != 2) {
          printf("Expecting one filename!!\n");
          continue;
        } else {
          strcpy(message, cmdCopy);
          verificationOkay = 1;
          isMessage = 1;
        }
      } else if (strcmp(command_array[0], "w24fz") == 0) {
        if (size == 3) {
          if (atoi(command_array[1]) <= atoi(command_array[2])) {
            if (atoi(command_array[1]) <= 0 || atoi(command_array[2]) <= 0) {
              printf("Sizes should be greater than zero\n");
              continue;
            } else {
              strcpy(message, cmdCopy);
              verificationOkay = 1;
            }
          } else {
            printf("Size1 should be less than or equal to Size2\n");
            continue;
          }
        } else {
          printf("Enter two sizes please!!\n");
          continue;
        }
      } else if (strcmp(command_array[0], "w24ft") == 0) {
        if (size < 2 || size > 4) {
          printf("Enter extensions between 1 and 3!\n");
          continue;
        } else {
          strcpy(message, cmdCopy);
          verificationOkay = 1;
        }
      } else if (strcmp(command_array[0], "w24fdb") == 0) {
        if (size != 2) {
          printf("Enter the date please!\n");
          continue;
        } else {
          int valid = validateDate(command_array[1]);
          if (valid == 1) {
            strcpy(message, cmdCopy);
            verificationOkay = 1;
          } else {
            printf("Either date is not valid or not in YYYY-MM-DD format.Enter correct date please!\n");
            continue;
          }
        }
      } else if (strcmp(command_array[0], "w24fda") == 0) {
        if (size != 2) {
          printf("Enter full command and valid date!\n");
          continue;
        } else {
          int valid = validateDate(command_array[1]);
          if (valid == 1) {
            strcpy(message, cmdCopy);
            verificationOkay = 1;
          } else {
            printf("Either date is not valid or not in YYYY-MM-DD format.Enter correct date please!\n");
            continue;
          }
        }
      } else if (strcmp(command_array[0], "quitc") == 0) {
        strcpy(message, cmdCopy);
        verificationOkay = 1;
      } else {
        printf("Enter Valid commands!!\n");
        continue;
      }
      //write into socket
      if (verificationOkay == 1) {
        verificationOkay = 0;
        int bytes = write(sid, cmdCopy, strlen(cmdCopy));
        if (!strcasecmp(cmdCopy, "quitc")) {
          kill(getppid(), SIGTERM);
          exit(0);
        }
      }
      
      //format the server message
      for (int i = 0; i < 256; i++) {
        server_message[i] = '\0';
      }
      if (isMessage == 1) { //for message printing in console
        isMessage = 0;
        if (n = read(sid, server_message, 256)) {
          server_message[n] = '\0';
          fprintf(stderr, "%s\n", server_message);
          if (!strcasecmp(server_message, "quitc\n")) {
            kill(pid, SIGTERM);
            exit(0);
          }
        }
      } else { //for creating files in the storage folder
         if (access(STORAGE_PATH, F_OK) != -1)
            remove("temp.tar.gz");
         
        int file_fd;
        if ((file_fd = open(STORAGE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
          perror("File open failed");
          exit(EXIT_FAILURE);
        }

        // Receive file data from server
        ssize_t bytes_received;
        char buffer[1024];
        sleep(2);
        int fileFound = 1;
         while ((bytes_received = recv(sid, buffer, 1024, MSG_DONTWAIT)) > 0){       
           fileFound = 0;
            write(file_fd, buffer, bytes_received);
         }
         
        if(fileFound == 1){
            printf("No File Found\n");
         }
        close(file_fd);
      }
    }
  } else { //parent to wait to child to finish
    wait(NULL);
  }
}