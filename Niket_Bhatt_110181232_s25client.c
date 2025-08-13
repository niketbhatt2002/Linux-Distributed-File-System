// DFS Client - COMPLETE WORKING VERSION 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>

#define PORT 4307
#define BUFFER_SIZE 1024

int connect_to_server();
int send_file(int sockfd, char *filename);
int receive_file(int sockfd, char *filename);

int main() {
    char input[BUFFER_SIZE];
    
    printf("DFS Client - Multi-File Support\n");
    printf("Commands:\n");
    printf("  uploadf <file1> [file2] [file3] <destination>\n");
    printf("  downlf <file1> [file2]\n");
    printf("  removef <file1> [file2]\n");
    printf("  downltar <filetype>\n");
    printf("  dispfnames <pathname>\n");
    printf("  exit\n\n");
    
    while (1) {
        printf("s25client$ ");
        fgets(input, BUFFER_SIZE, stdin);
        input[strcspn(input, "\n")] = 0; // Remove newline
        
        if (strlen(input) == 0) continue;
        if (strcmp(input, "exit") == 0) break;
        
        // Parse input into words
        char *words[10];
        int word_count = 0;
        char *input_copy = strdup(input);
        char *token = strtok(input_copy, " ");
        
        while (token != NULL && word_count < 10) {
            words[word_count] = strdup(token);
            word_count++;
            token = strtok(NULL, " ");
        }
        
        if (word_count == 0) {
            free(input_copy);
            continue;
        }
        
        char *command = words[0];
        
        // UPLOADF COMMAND
        if (strcmp(command, "uploadf") == 0) {
            if (word_count < 3) {
                printf("Usage: uploadf <file1> [file2] [file3] <destination>\n");
                goto cleanup;
            }
            
            // Last word is destination, others are files
            char *destination = words[word_count - 1];
            int file_count = word_count - 2;
            
            if (file_count > 3) {
                printf("ERROR: Maximum 3 files allowed\n");
                goto cleanup;
            }
            
            if (strncmp(destination, "~S1/", 4) != 0) {
                printf("ERROR: Destination must start with ~S1/\n");
                goto cleanup;
            }
            
            // Upload each file
            for (int i = 0; i < file_count; i++) {
                char *filename = words[i + 1];
                
                // Check file exists
                struct stat st;
                if (stat(filename, &st) != 0) {
                    printf("ERROR: File '%s' not found\n", filename);
                    continue;
                }
                
                // Check extension
                char *ext = strrchr(filename, '.');
                if (!ext || (strcmp(ext, ".c") != 0 && strcmp(ext, ".pdf") != 0 && 
                           strcmp(ext, ".txt") != 0 && strcmp(ext, ".zip") != 0)) {
                    printf("ERROR: File '%s' has unsupported extension\n", filename);
                    continue;
                }
                
                // Connect and upload
                int sockfd = connect_to_server();
                if (sockfd < 0) {
                    printf("ERROR: Cannot connect to server\n");
                    continue;
                }
                
                // Send command
                char cmd[BUFFER_SIZE];
                snprintf(cmd, BUFFER_SIZE, "uploadf %s %s", filename, destination);
                write(sockfd, cmd, strlen(cmd));
                
                // Read response
                char response[BUFFER_SIZE];
                bzero(response, BUFFER_SIZE);
                read(sockfd, response, BUFFER_SIZE - 1);
                
                if (strcmp(response, "READY") == 0) {
                    printf("Uploading file %d/%d: %s\n", i+1, file_count, filename);
                    send_file(sockfd, filename);
                    
                    bzero(response, BUFFER_SIZE);
                    read(sockfd, response, BUFFER_SIZE - 1);
                    printf("  %s\n", response);
                } else {
                    printf("  %s\n", response);
                }
                
                close(sockfd);
            }
        }
        
        // DOWNLF COMMAND
        else if (strcmp(command, "downlf") == 0) {
            if (word_count < 2 || word_count > 3) {
                printf("Usage: downlf <file1> [file2]\n");
                goto cleanup;
            }
            
            int file_count = word_count - 1;
            
            for (int i = 0; i < file_count; i++) {
                char *filename = words[i + 1];
                
                if (strncmp(filename, "~S1/", 4) != 0) {
                    printf("ERROR: File '%s' must start with ~S1/\n", filename);
                    continue;
                }
                
                int sockfd = connect_to_server();
                if (sockfd < 0) {
                    printf("ERROR: Cannot connect to server\n");
                    continue;
                }
                
                char cmd[BUFFER_SIZE];
                snprintf(cmd, BUFFER_SIZE, "downlf %s", filename);
                write(sockfd, cmd, strlen(cmd));
                
                char *base_name = basename(filename);
                printf("Downloading file %d/%d: %s\n", i+1, file_count, base_name);
                
                if (receive_file(sockfd, base_name) == 0) {
                    printf("  Successfully downloaded: %s\n", base_name);
                } else {
                    printf("  Failed to download: %s\n", base_name);
                }
                
                close(sockfd);
            }
        }
        
        // REMOVEF COMMAND
        else if (strcmp(command, "removef") == 0) {
            if (word_count < 2 || word_count > 3) {
                printf("Usage: removef <file1> [file2]\n");
                goto cleanup;
            }
            
            int file_count = word_count - 1;
            
            for (int i = 0; i < file_count; i++) {
                char *filename = words[i + 1];
                
                if (strncmp(filename, "~S1/", 4) != 0) {
                    printf("ERROR: File '%s' must start with ~S1/\n", filename);
                    continue;
                }
                
                int sockfd = connect_to_server();
                if (sockfd < 0) {
                    printf("ERROR: Cannot connect to server\n");
                    continue;
                }
                
                char cmd[BUFFER_SIZE];
                snprintf(cmd, BUFFER_SIZE, "removef %s", filename);
                write(sockfd, cmd, strlen(cmd));
                
                char response[BUFFER_SIZE];
                bzero(response, BUFFER_SIZE);
                read(sockfd, response, BUFFER_SIZE - 1);
                
                printf("Removing file %d/%d: %s\n", i+1, file_count, basename(filename));
                printf("  %s\n", response);
                
                close(sockfd);
            }
        }
        
        // DOWNLTAR COMMAND
        else if (strcmp(command, "downltar") == 0) {
            if (word_count != 2) {
                printf("Usage: downltar <filetype>\n");
                goto cleanup;
            }
            
            char *filetype = words[1];
            if (strcmp(filetype, ".c") != 0 && strcmp(filetype, ".pdf") != 0 && strcmp(filetype, ".txt") != 0) {
                printf("ERROR: Unsupported filetype for tar\n");
                goto cleanup;
            }
            
            int sockfd = connect_to_server();
            if (sockfd < 0) {
                printf("ERROR: Cannot connect to server\n");
                goto cleanup;
            }
            
            char cmd[BUFFER_SIZE];
            snprintf(cmd, BUFFER_SIZE, "downltar %s", filetype);
            write(sockfd, cmd, strlen(cmd));
            
            char output_file[50];
            if (strcmp(filetype, ".c") == 0) strcpy(output_file, "cfiles.tar");
            else if (strcmp(filetype, ".pdf") == 0) strcpy(output_file, "pdf.tar");
            else strcpy(output_file, "text.tar");
            
            if (receive_file(sockfd, output_file) == 0) {
                printf("Tar file '%s' downloaded successfully\n", output_file);
            }
            
            close(sockfd);
        }
        
        // DISPFNAMES COMMAND
        else if (strcmp(command, "dispfnames") == 0) {
            if (word_count != 2) {
                printf("Usage: dispfnames <pathname>\n");
                goto cleanup;
            }
            
            char *pathname = words[1];
            if (strncmp(pathname, "~S1/", 4) != 0) {
                printf("ERROR: Pathname must start with ~S1/\n");
                goto cleanup;
            }
            
            int sockfd = connect_to_server();
            if (sockfd < 0) {
                printf("ERROR: Cannot connect to server\n");
                goto cleanup;
            }
            
            char cmd[BUFFER_SIZE];
            snprintf(cmd, BUFFER_SIZE, "dispfnames %s", pathname);
            write(sockfd, cmd, strlen(cmd));
            
            char response[BUFFER_SIZE * 4];
            bzero(response, sizeof(response));
            read(sockfd, response, sizeof(response) - 1);
            
            printf("Files in %s:\n%s", pathname, response);
            
            close(sockfd);
        }
        
        else {
            printf("Unknown command: %s\n", command);
        }
        
        cleanup:
        // Free allocated memory
        for (int i = 0; i < word_count; i++) {
            free(words[i]);
        }
        free(input_copy);
    }
    
    return 0;
}

int connect_to_server() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    struct hostent *server = gethostbyname("localhost");
    if (server == NULL) {
        close(sockfd);
        return -1;
    }
    
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(PORT);
    
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int send_file(int sockfd, char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    fstat(fd, &st);
    write(sockfd, &st.st_size, sizeof(off_t));
    
    char buffer[BUFFER_SIZE];
    off_t remaining = st.st_size;
    while (remaining > 0) {
        ssize_t n = read(fd, buffer, BUFFER_SIZE);
        if (n <= 0) break;
        write(sockfd, buffer, n);
        remaining -= n;
    }
    
    close(fd);
    return 0;
}

int receive_file(int sockfd, char *filename) {
    off_t file_size;
    if (read(sockfd, &file_size, sizeof(off_t)) != sizeof(off_t)) return -1;
    if (file_size <= 0) return -1;
    
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    char buffer[BUFFER_SIZE];
    off_t remaining = file_size;
    while (remaining > 0) {
        int chunk = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        int bytes = read(sockfd, buffer, chunk);
        if (bytes <= 0) break;
        write(fd, buffer, bytes);
        remaining -= bytes;
    }
    
    close(fd);
    return 0;
}
