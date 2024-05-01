#define __USE_XOPEN
#include <sys/sendfile.h>
#include <time.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <libgen.h>
#define MAX_CLIENTS 20
#define _XOPEN_SOURCE 500
#define BUFFER_SIZE 1000
#define SERVER_PORT 4511
#define MIRROR1_PORT 4351
#define MIRROR2_PORT 4356
struct FTW;
int connection_count = 0;
int count = 0;
char * output;
char fileToBeSearched[256];
int filecount = 0;
int extcount = 0;
int ucount = 0;
char * date;
long size1, size2;
char * directory_name;
char * file_extension;
char * extension;
long size1, size2;
//Splitting array based on single space, takes command symbol based on which we want to seperate and adds the count of symbol in size and return array 
char ** split_command(char * command, char * symbol, int * size) {
  * size = 1;
  for (char * a = command;* a != '\0'; a++) {
    if (strchr(symbol, * a) != NULL) {
      ( * size) ++;
    }
  }
  char ** command_array = malloc(( * size) * sizeof(char));
  if (command_array == NULL) {
    printf("Memory not allocated. Error.Try again.\n");
    exit(EXIT_FAILURE);
  }
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
  return command_array;
}
//Adding file in tar takes the path of files and adds it in tar file
void add_in_tar(const char * filepath) {
  char dir_name[1024];
  char file_name[1024];
  char * tar_name = "temp.tar";
  strcpy(dir_name, filepath);
  strcpy(file_name, strrchr(dir_name, '/'));
  char * last_slash = strrchr(dir_name, '/');
  if (last_slash != NULL) {
    * last_slash = '\0';
  }
  char command[524];
  sprintf(command, "tar -rf %s  --transform='s|.*/||' -C %s %s", tar_name, 	dir_name, file_name + 1);
  if (system(command) != 0) {
    printf("error in adding file!!\ns");
  }
}
//nftw call back function checks if file is regular file and is within range
//if in range calls add_in_tar function for adding that file in tar and returns int
int find_files(const char * fpath,const struct stat * sb, int tflag, struct FTW * ftwbuf) {
  // Check if it's a regular file and size is within range
  if (tflag == FTW_F && sb -> st_size >= size1 && sb -> st_size <= size2) {
    printf("%s\n", fpath);
    add_in_tar(fpath);
  }
  return 0; // Continue traversal
}
//nftw call back function for checking if file has same extension as asked by client if yes add in tar file
int search_and_archive(const char * fpath,const struct stat * sb, int flagType, struct FTW * ftw) {
  if (flagType == FTW_F) {
    const char * ext = strrchr(fpath, '.');
    if (ext != NULL && strcmp(ext + 1, file_extension) == 0) {
      add_in_tar(fpath); //calling function to add searched files(fpath) in temp.tar
    }
  }
  return 0;
}
//nftw call back function for searching a file and if file found add all details of that dile in output array
int search_files(const char * fpath,const struct stat * sb, int flagType, struct FTW * ftw) {
  char * base = basename(fpath);
  filecount = 0;
  output = malloc(BUFFER_SIZE * sizeof(char));
  for (int i = 0; i < strlen(output); i++) {
    output[i] = '\0';
  }
  if (output == NULL) {
    printf("Memory allocation failed for output\n");
    return -1;
  }
  if (flagType == FTW_F && strcmp(base, fileToBeSearched) == 0) {
    filecount++;
    strcat(output, "File Name: ");
    strcat(output, fileToBeSearched);
    strcat(output, "\n");
    strcat(output, "File Size: ");
    long size = sb -> st_size;
    char size_str[20];
    snprintf(size_str, sizeof(size_str), "%ld", size);
    strcat(output, size_str);
    strcat(output, " bytes\n");
    strcat(output, "Date Created: ");
    strcat(output, ctime( & sb -> st_mtime));
    strcat(output, "File Permissions: ");
    strcat(output, (S_ISDIR(sb -> st_mode)) ? "d" : "-");
    strcat(output, (sb -> st_mode & S_IRUSR) ? "r" : "-");
    strcat(output, (sb -> st_mode & S_IWUSR) ? "w" : "-");
    strcat(output, (sb -> st_mode & S_IXUSR) ? "x" : "-");
    strcat(output, (sb -> st_mode & S_IXUSR) ? "x" : "-");
    strcat(output, (sb -> st_mode & S_IRGRP) ? "r" : "-");
    strcat(output, (sb -> st_mode & S_IWGRP) ? "w" : "-");
    strcat(output, (sb -> st_mode & S_IXGRP) ? "x" : "-");
    strcat(output, (sb -> st_mode & S_IROTH) ? "r" : "-");
    strcat(output, (sb -> st_mode & S_IWOTH) ? "w" : "-");
    strcat(output, (sb -> st_mode & S_IXOTH) ? "x" : "-");
    return 1;
  }
  return 0;
}
//nftw call back function for add all those files in tar which are greater or equal to the given date
int greater_equal_time(const char * file_name, struct stat * sb, int flag, struct FTW * s) {
  struct tm t_date;
  char * tar_name = "temp.tar";
  if (flag == FTW_F) {
    struct stat file_stat;
    if (stat(file_name, & file_stat) == -1) {
      perror("Failed to get file information");
      return -1;
    }

    // Get file modification date
    struct tm * file_tm = localtime( & file_stat.st_mtime);
    int file_year = file_tm -> tm_year;
    int file_month = file_tm -> tm_mon;
    int file_day = file_tm -> tm_mday;

    // Parse user-given date
    struct tm user_tm;
    if (strptime(date, "%Y-%m-%d", & user_tm) == NULL) {
      perror("Invalid user-given date format\n");
      return -1;
    }
    int user_year = user_tm.tm_year;
    int user_month = user_tm.tm_mon;
    int user_day = user_tm.tm_mday;

    // Compare dates
    if (file_year >= user_year && file_month >= user_month && file_day >= user_day) {
      char command[1000];
      sprintf(command, "tar -rf %s --transform='s|.*/||' -C %s %s", tar_name, directory_name, file_name);
      system(command);
    }
  }
  return 0;
}
//nftw call back function for add all those files in tar which are less or equal to the given date
int less_equal_time(const char * file_name, struct stat * sb, int flag, struct FTW * s) {
  struct tm t_date;
  char * tar_name = "temp.tar";
  if (flag == FTW_F) {
    struct stat file_stat;
    if (stat(file_name, & file_stat) == -1) {
      perror("Failed to get file information");
      return -1;
    }

    // Get file modification date
    struct tm * file_tm = localtime( & file_stat.st_mtime);
    int file_year = file_tm -> tm_year;
    int file_month = file_tm -> tm_mon;
    int file_day = file_tm -> tm_mday;

    // Parse user-given date
    struct tm user_tm;
    if (strptime(date, "%Y-%m-%d", & user_tm) == NULL) {
      printf("Invalid user-given date format\n");
      return -1;
    }
    int user_year = user_tm.tm_year;
    int user_month = user_tm.tm_mon;
    int user_day = user_tm.tm_mday;

    // Compare dates
    if (file_year <= user_year && file_month <= user_month && file_day <= user_day) {
      // Add the file to the tar archive
      char command[1000];
      sprintf(command, "tar -rf %s  --transform='s|.*/||' -C %s %s", tar_name, directory_name, file_name);
      system(command);
    }
  }
  return 0;
}

//for swapping two variables
void swap(char ** a, char ** b) {
  char * temp = * a;
  * a = * b;
  * b = temp;
}
//for sorting directory names alphabetically
char ** sort_alphabetically(char ** subdir_names, int count) {
  for (int i = 0; i < count - 1; i++) {
    for (int k = i + 1; k < count; k++) {
      //strcasecmp to compare files irrespective whether they are in upper or lower case
      if (strcasecmp(subdir_names[i], subdir_names[k]) > 0) {
        swap( & subdir_names[i], & subdir_names[k]);
      }
    }
  }
  return subdir_names;
}
//for sorting directory names based on time older being on top 
char ** sort_timely(char ** subdir_names, int count) {
  struct stat isub, ksub;
  for (int i = 0; i < count - 1; i++) {
    stat(subdir_names[i], & isub);
    for (int k = i + 1; k < count; k++) {
      stat(subdir_names[k], & ksub);
      if (isub.st_mtime < ksub.st_mtime) {
        swap( & subdir_names[i], & subdir_names[k]);
      }
    }
  }
  return subdir_names;
}
//getting all the subdirectories present inside home and based on client request based on parameter sorting either alphabetically or timely.
char ** list_subdirectories(const char * directory_name,
  const char * message) {
  DIR * dir = opendir(directory_name);
  if (dir == NULL) {
    perror("opendir");
    exit(EXIT_FAILURE);
  }
  count = 0;
  struct dirent * directory;
  while ((directory = readdir(dir)) != NULL) {
    if (strcmp(directory -> d_name, "..") != 0 && strcmp(directory -> d_name, ".") != 0 && directory -> d_type == DT_DIR)
      count++;
  }

  char ** subdir_names = malloc(count * sizeof(char * ));
  for (int i = 0; i < count; i++) {
    subdir_names[i] = "\0";
  }
  if (subdir_names == NULL) {
    printf("Issue with malloc\n");
    exit(EXIT_FAILURE);
  }

  int j = 0;
  rewinddir(dir);
  while ((directory = readdir(dir)) != NULL) {
    if (strcmp(directory -> d_name, "..") != 0 && strcmp(directory -> d_name, ".") != 0 && directory -> d_type == DT_DIR) {
      subdir_names[j] = strdup(directory -> d_name);
      j++;
    }
  }

  if (strstr(message, "dirlist -a") != NULL) {
    sort_alphabetically(subdir_names, count);
  } else if (strstr(message, "dirlist -t") != NULL) {
    sort_timely(subdir_names, count);
  }

  closedir(dir);
  return subdir_names;
}
//for getting details who will handle incoming request Server, Mirror1 or Mirror2
int server_number(int ctcount) {
  int cflag = -1;
  if (ctcount >= 1 && ctcount <= 3) {
    cflag = 0;
  } else if (ctcount >= 4 && ctcount <= 6) {
    cflag = 1;
  } else if (ctcount >= 7 && ctcount <= 9) {
    cflag = 2;
  } else if (ctcount >= 10) {
    int request = ctcount % 3;
    if (request == 1) {
      cflag = 0;
    } else if (request == 2) {
      cflag = 1;
    } else {
      cflag = 2;
    }
  }
  return cflag;
}
//for handling the client request
void crequest(int client_socket) {
  char message[BUFFER_SIZE];
  ssize_t bytes_received;
  write(client_socket, "Server connected", strlen("Server connected"));
  while ((bytes_received = read(client_socket, message, BUFFER_SIZE - 1)) > 0) {
    //directory_name=getenv("HOME");
    directory_name = "/home/vishwak/Desktop/CPrograms/ASPAssigment1-practice";
    message[bytes_received] = '\0';
    fprintf(stderr, "serverw24$%s\n", message);
    // Check if client wants to quit
    if (strcasecmp(message, "quitc") == 0) {
      close(client_socket);
      return;
    } 
    //Check if client request starts with dirlist
    else if (strstr(message, "dirlist") != NULL) {
      char ** result = list_subdirectories(directory_name, message);
      char directory_list[BUFFER_SIZE * count];
      for (int i = 0; i < BUFFER_SIZE * count; i++) {
        directory_list[i] = '\0';
      }
      for (int i = 0; i < count; i++) {
        strcat(directory_list, result[i]);
        if (i < count - 1)
          strcat(directory_list, "\n");
        free(result[i]);
      }
      free(result);
      write(client_socket, directory_list, strlen(directory_list));
    } 
    //Check if client request starts with w24fdb
    else if (strstr(message, "w24fdb") != NULL) {
      char * array[2];
      array[0] = strtok(message, " ");
      array[1] = strtok(NULL, " ");
      date = array[1];
      fprintf(stderr, "date=%s2\n", date);
      //if tar file is already present remove
      if (access("temp.tar.gz", F_OK) != -1)
        remove("temp.tar.gz");
      if (nftw(directory_name, less_equal_time, 100, FTW_NS) == -1) {
        printf("Error with directory\n");
        exit(EXIT_FAILURE);
      }
      if (access("temp.tar", F_OK) != -1) {
        system("gzip temp.tar");
        int file_fd = open("temp.tar.gz", O_RDONLY);
        if (file_fd != -1) {
          struct stat file_stat;
          if (fstat(file_fd, & file_stat) == -1) {
            perror("Failed to get file information");
            exit(EXIT_FAILURE);
          }
          off_t file_size = file_stat.st_size;
          // Send the file
          off_t offset = 0;
          while (offset < file_size) {
            ssize_t bytes_sent = sendfile(client_socket, file_fd, &offset, file_size - offset);
            if (bytes_sent == -1) {
              perror("sendfile");
              exit(EXIT_FAILURE);
            }
            offset += bytes_sent;
          }
          //send(client_socket, "END_OF_FILE", strlen("END_OF_FILE"), 0);
        }
        //Check if client request starts with w24fdb
        close(file_fd);
      } else {
      	sendfile(client_socket, -1, NULL, sizeof("File not found"));
      }
    }//If client request starts with w24fn
    else if (strstr(message, "w24fn") != NULL) {
      char * array[2];
      array[0] = strtok(message, " ");
      array[1] = strtok(NULL, " ");
      int len = strlen(array[1]);
      strncpy(fileToBeSearched, array[1], sizeof(fileToBeSearched));
      int c;
      if (nftw(directory_name, search_files, 20, FTW_NS) == 1 && filecount > 0) {
        write(client_socket, output, strlen(output));
        free(output);
      }
      if (filecount == 0) {
        write(client_socket, "File not found", strlen("File not found"));
      }
    }
    //If client request starts with w24ft 
    else if (strstr(message, "w24ft") != NULL) {
      if (access("temp.tar.gz", F_OK) != -1)
        remove("temp.tar.gz");
      int ext = 0;
      char ** ext_list = split_command(message, " ", & ext);
      for (int i = 1; i < ext; i++) {
        file_extension = ext_list[i];
        if (nftw(directory_name, search_and_archive, 20, FTW_NS) == -1) {
          perror("nftw");
        }
      }
      if (access("temp.tar", F_OK) != -1) {
        system("gzip temp.tar");
        int file_fd = open("temp.tar.gz", O_RDONLY);
        if (file_fd != -1) {
          struct stat file_stat;
          if (fstat(file_fd, & file_stat) == -1) {
            perror("Failed to get file information");
            exit(EXIT_FAILURE);
          }
          off_t file_size = file_stat.st_size;
	   off_t offset = 0;
	   //sending until all zip is sent byte by byte
          while (offset < file_size) {
            ssize_t bytes_sent = sendfile(client_socket, file_fd, & offset, file_size - offset);
            if (bytes_sent == -1) {
              perror("sendfile");
              exit(EXIT_FAILURE);
            }
            offset += bytes_sent;
          }
          // Close the file descriptor
          close(file_fd);
        }
      } else {
        sendfile(client_socket, -1, NULL, sizeof("File not found"));
      }
    }//If client request starts with w24fz 
    else if (strstr(message, "w24fz") != NULL) {
      char * array[3];
      if (access("temp.tar.gz", F_OK) != -1)
        remove("temp.tar.gz");
      array[0] = strtok(message, " ");
      array[1] = strtok(NULL, " ");
      array[2] = strtok(NULL, " ");
      size1 = atol(array[1]);
      size2 = atol(array[2]);
      if (nftw(directory_name, find_files, 20, FTW_NS) == -1) {
        perror("nftw");
      }
      if (access("temp.tar", F_OK) != -1) {
        system("gzip temp.tar");
        int file_fd = open("temp.tar.gz", O_RDONLY);
        if (file_fd != -1) {
          struct stat file_stat;
          if (fstat(file_fd, & file_stat) == -1) {
            perror("Failed to get file information");
            exit(EXIT_FAILURE);
          }
          off_t file_size = file_stat.st_size;
          // Send the file
          off_t offset = 0;
          while (offset < file_size) {
            ssize_t bytes_sent = sendfile(client_socket, file_fd, & offset, file_size - offset);
            if (bytes_sent == -1) {
              perror("sendfile");
              exit(EXIT_FAILURE);
            }
            offset += bytes_sent;
          }
          // Close the file descriptor
          close(file_fd);
        }
      } else {
        sendfile(client_socket, -1, NULL, sizeof("File not found"));
      }
    } else if (strstr(message, "w24fda") != NULL) {
      char * array[2];
      array[0] = strtok(message, " ");
      array[1] = strtok(NULL, " ");
      date = array[1];
      if (access("temp.tar.gz", F_OK) != -1)
        remove("temp.tar.gz");
      if (nftw(directory_name, greater_equal_time, 100, FTW_NS) == -1) {
        printf("Error with directory\n");
        exit(EXIT_FAILURE);
      }
      if (access("temp.tar", F_OK) != -1) {
        system("gzip temp.tar");
        int file_fd = open("temp.tar.gz", O_RDONLY);
        if (file_fd != -1) {
          struct stat file_stat;
          if (fstat(file_fd, & file_stat) == -1) {
            perror("Failed to get file information");
            exit(EXIT_FAILURE);
          }
          off_t file_size = file_stat.st_size;
          // Send the file
          off_t offset = 0;
          while (offset < file_size) {
            ssize_t bytes_sent = sendfile(client_socket, file_fd, & offset, file_size - offset);
            if (bytes_sent == -1) {
              perror("sendfile");
              exit(EXIT_FAILURE);
            }
            offset += bytes_sent;
          }

        }
        close(file_fd);

      } else {
        sendfile(client_socket, -1, NULL, sizeof("File not found"));

      }
    } else {
      write(client_socket, message, strlen(message));
    }
  }
  close(client_socket);
}

int main(int argc, char * argv[]) {
  int server_socket, client_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  pid_t child_pid;

  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(SERVER_PORT);

  if (bind(server_socket, (struct sockaddr * ) & server_addr, sizeof(server_addr)) == -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if (listen(server_socket, MAX_CLIENTS) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  while (1) {
    if ((client_socket = accept(server_socket, (struct sockaddr * ) & client_addr, & client_addr_len)) == -1) {
      perror("accept");
      continue;
    }
    //incrementing count 
    connection_count++;
    int cflag = server_number(connection_count);
      if (cflag == 1 || cflag == 2) {
        char port_details[20];
        for (int i = 0; i < 20; i++) {
          port_details[i] = '\0';
        }
        if (cflag == 1)
          sprintf(port_details, "%d", MIRROR1_PORT);
        if (cflag == 2)
          sprintf(port_details, "%d", MIRROR2_PORT);
        write(client_socket, port_details, strlen(port_details));
        close(client_socket);
      } else if (cflag == 0) {
        
    if ((child_pid = fork()) == -1) {
      perror("fork");
      close(client_socket);
      continue;
    } else if (child_pid == 0) { // Child process
      close(server_socket); // Close server socket in child
      crequest(client_socket);
      close(client_socket);
      exit(EXIT_SUCCESS);
    } else {
      close(client_socket);
    }
    }
  }
  close(server_socket);
  return 0;
}