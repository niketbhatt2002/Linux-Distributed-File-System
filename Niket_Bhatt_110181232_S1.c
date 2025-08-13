// Distributed File System - S1 Server Implementation by Niket
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
#include <dirent.h>
#include <libgen.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_PATH_LEN 1024
#define MAX_FILES 3

int main_port = 4307;
int s2_port = 4308;
int s3_port = 4309;
int s4_port = 4310;

void process_client_request(int client_conn);
int handle_upload(int client_conn, char *filename, char *dest_path);
int handle_download(int client_conn, char *filename);
int handle_remove(int client_conn, char *filename);
int handle_tar_download(int client_conn, char *filetype);
int display_files(int client_conn, char *pathname);
int forward_to_server(int target_port, char *file_path, char *dest_path);
int send_command_to_server(int port, char *command, char *response);
int create_directory_structure(char *path);
void handle_error(const char *msg);

int main(int argc, char *argv[]) 
{
    if (argc == 5) {
        main_port = atoi(argv[1]);
        s2_port = atoi(argv[2]);
        s3_port = atoi(argv[3]);
        s4_port = atoi(argv[4]);
    }

    int server_socket, client_conn;
    socklen_t client_len;
    struct sockaddr_in server_addr, client_addr;
    pid_t child_pid;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        handle_error("Socket creation failed");
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(main_port);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        handle_error("Binding failed");
    }

    listen(server_socket, MAX_CLIENTS);
    client_len = sizeof(client_addr);

    printf("S1 Server started on port %d\n", main_port);

    while (1) {
        client_conn = accept(server_socket, (struct sockaddr *) &client_addr, &client_len);
        if (client_conn < 0) {
            handle_error("Client accept failed");
        }

        child_pid = fork();
        if (child_pid < 0) {
            handle_error("Fork failed");
        }

        if (child_pid == 0) {
            close(server_socket);
            process_client_request(client_conn);
            close(client_conn);
            exit(0);
        } else {
            close(client_conn);
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(server_socket);
    return 0;
}

void process_client_request(int client_conn) 
{
    char buffer[BUFFER_SIZE];
    int n;
    
    bzero(buffer, BUFFER_SIZE);
    n = read(client_conn, buffer, BUFFER_SIZE - 1);
    if (n < 0) {
        handle_error("Reading from client failed");
    }
    
    printf("Received command: %s\n", buffer);
    
    char *cmd = strtok(buffer, " ");
    if (cmd == NULL) {
        write(client_conn, "ERROR: Invalid command", 22);
        return;
    }
    
    if (strcmp(cmd, "uploadf") == 0) {
        char *filename = strtok(NULL, " ");
        char *dest_path = strtok(NULL, " ");
        if (filename == NULL || dest_path == NULL) {
            write(client_conn, "ERROR: Invalid uploadf format", 29);
            return;
        }
        handle_upload(client_conn, filename, dest_path);
    } 
    else if (strcmp(cmd, "downlf") == 0) {
        char *filename = strtok(NULL, " ");
        if (filename == NULL) {
            write(client_conn, "ERROR: Invalid downlf format", 28);
            return;
        }
        handle_download(client_conn, filename);
    } 
    else if (strcmp(cmd, "removef") == 0) {
        char *filename = strtok(NULL, " ");
        if (filename == NULL) {
            write(client_conn, "ERROR: Invalid removef format", 29);
            return;
        }
        handle_remove(client_conn, filename);
    } 
    else if (strcmp(cmd, "downltar") == 0) {
        char *filetype = strtok(NULL, " ");
        if (filetype == NULL) {
            write(client_conn, "ERROR: Invalid downltar format", 30);
            return;
        }
        handle_tar_download(client_conn, filetype);
    } 
    else if (strcmp(cmd, "dispfnames") == 0) {
        char *pathname = strtok(NULL, " ");
        if (pathname == NULL) {
            write(client_conn, "ERROR: Invalid dispfnames format", 32);
            return;
        }
        display_files(client_conn, pathname);
    } 
    else {
        write(client_conn, "ERROR: Unknown command", 22);
    }
}

int handle_upload(int client_conn, char *filename, char *dest_path) 
{
    write(client_conn, "READY", 5);
    
    off_t file_size;
    read(client_conn, &file_size, sizeof(off_t));
    
    char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        write(client_conn, "ERROR: File has no extension", 28);
        return -1;
    }
    
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), dest_path + 3);
    create_directory_structure(s1_path);
    
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, MAX_PATH_LEN, "%s/%s", s1_path, basename(filename));
    
    int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        write(client_conn, "ERROR: Failed to create file", 28);
        return -1;
    }
    
    char buffer[BUFFER_SIZE];
    off_t remaining = file_size;
    while (remaining > 0) {
        int n = read(client_conn, buffer, BUFFER_SIZE);
        if (n <= 0) {
            close(fd);
            write(client_conn, "ERROR: File transfer failed", 27);
            return -1;
        }
        write(fd, buffer, n);
        remaining -= n;
    }
    close(fd);
    
    if (strcmp(ext, ".c") == 0) {
        write(client_conn, "SUCCESS: File uploaded to S1", 29);
        return 0;
    } else {
        int target_port = 0;
        if (strcmp(ext, ".pdf") == 0) target_port = s2_port;
        else if (strcmp(ext, ".txt") == 0) target_port = s3_port;
        else if (strcmp(ext, ".zip") == 0) target_port = s4_port;
        
        if (target_port > 0) {
            if (forward_to_server(target_port, full_path, dest_path) == 0) {
                unlink(full_path);
                write(client_conn, "SUCCESS: File forwarded to server", 33);
            } else {
                write(client_conn, "ERROR: Failed to forward file", 29);
            }
        } else {
            write(client_conn, "ERROR: Unsupported file type", 28);
        }
    }
    
    return 0;
}

int handle_download(int client_conn, char *filename) 
{
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), filename + 3);
    
    struct stat st;
    if (stat(s1_path, &st) == 0) {
        int fd = open(s1_path, O_RDONLY);
        if (fd < 0) {
            write(client_conn, "ERROR: Failed to open file", 26);
            return -1;
        }
        
        write(client_conn, &st.st_size, sizeof(off_t));
        
        char buffer[BUFFER_SIZE];
        off_t remaining = st.st_size;
        while (remaining > 0) {
            ssize_t n = read(fd, buffer, (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
            if (n <= 0) {
                close(fd);
                return -1;
            }
            write(client_conn, buffer, n);
            remaining -= n;
        }
        close(fd);
        return 0;
    }
    
    char *ext = strrchr(filename, '.');
    int target_port = 0;
    if (strcmp(ext, ".pdf") == 0) target_port = s2_port;
    else if (strcmp(ext, ".txt") == 0) target_port = s3_port;
    else if (strcmp(ext, ".zip") == 0) target_port = s4_port;
    
    if (target_port > 0) {
        int sockfd;
        struct sockaddr_in serv_addr;
        struct hostent *server;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            off_t error_size = -1;
            write(client_conn, &error_size, sizeof(off_t));
            return -1;
        }

        server = gethostbyname("localhost");
        if (server == NULL) {
            close(sockfd);
            off_t error_size = -1;
            write(client_conn, &error_size, sizeof(off_t));
            return -1;
        }

        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(target_port);

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sockfd);
            off_t error_size = -1;
            write(client_conn, &error_size, sizeof(off_t));
            return -1;
        }

        char command[BUFFER_SIZE];
        snprintf(command, BUFFER_SIZE, "downlf %s", filename);
        write(sockfd, command, strlen(command));

        off_t filesize;
        if (read(sockfd, &filesize, sizeof(off_t)) == sizeof(off_t)) {
            write(client_conn, &filesize, sizeof(off_t));
            
            if (filesize > 0) {
                char buffer[BUFFER_SIZE];
                off_t remaining = filesize;
                while (remaining > 0) {
                    int bytes = read(sockfd, buffer, 
                                   (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
                    if (bytes <= 0) break;
                    write(client_conn, buffer, bytes);
                    remaining -= bytes;
                }
            }
        } else {
            off_t error_size = -1;
            write(client_conn, &error_size, sizeof(off_t));
        }

        close(sockfd);
    } else {
        off_t error_size = -1;
        write(client_conn, &error_size, sizeof(off_t));
    }
    
    return 0;
}

int handle_remove(int client_conn, char *filename) 
{
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), filename + 3);
    
    if (unlink(s1_path) == 0) {
        write(client_conn, "SUCCESS: File deleted from S1", 30);
        return 0;
    }
    
    char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        write(client_conn, "ERROR: File has no extension", 28);
        return -1;
    }
    
    int target_port = 0;
    if (strcmp(ext, ".pdf") == 0) target_port = s2_port;
    else if (strcmp(ext, ".txt") == 0) target_port = s3_port;
    else if (strcmp(ext, ".zip") == 0) target_port = s4_port;
    
    if (target_port > 0) {
        char command[MAX_PATH_LEN];
        snprintf(command, MAX_PATH_LEN, "removef %s", filename);
        
        char response[BUFFER_SIZE];
        if (send_command_to_server(target_port, command, response) == 0) {
            write(client_conn, response, strlen(response));
        } else {
            write(client_conn, "ERROR: Failed to contact server", 31);
        }
    } else {
        write(client_conn, "ERROR: File not found", 21);
    }
    
    return 0;
}

int handle_tar_download(int client_conn, char *filetype) 
{
    if (strcmp(filetype, ".c") == 0) {
        char s1_dir[MAX_PATH_LEN];
        snprintf(s1_dir, MAX_PATH_LEN, "%s/S1", getenv("HOME"));

        // Simple approach - create tar directly
        char tar_cmd[MAX_PATH_LEN * 2];
        snprintf(tar_cmd, sizeof(tar_cmd), "cd %s && tar -cf /tmp/cfiles.tar *.c 2>/dev/null", s1_dir);

        system(tar_cmd);

        struct stat st;
        if (stat("/tmp/cfiles.tar", &st) == 0 && st.st_size > 0) {
            write(client_conn, &st.st_size, sizeof(off_t));
            
            int fd = open("/tmp/cfiles.tar", O_RDONLY);
            if (fd >= 0) {
                sendfile(client_conn, fd, NULL, st.st_size);
                close(fd);
            }
            unlink("/tmp/cfiles.tar");
        } else {
            // Create empty tar if no files
            system("tar -cf /tmp/cfiles.tar --files-from /dev/null");
            if (stat("/tmp/cfiles.tar", &st) == 0) {
                write(client_conn, &st.st_size, sizeof(off_t));
                int fd = open("/tmp/cfiles.tar", O_RDONLY);
                if (fd >= 0) {
                    sendfile(client_conn, fd, NULL, st.st_size);
                    close(fd);
                }
                unlink("/tmp/cfiles.tar");
            }
        }
    } else if (strcmp(filetype, ".pdf") == 0 || strcmp(filetype, ".txt") == 0) {
        // Keep existing code for PDF and TXT - DON'T CHANGE THIS PART
        int target_port = (strcmp(filetype, ".pdf") == 0) ? s2_port : s3_port;
        char command[100];
        snprintf(command, 100, "downltar %s", filetype);

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd >= 0) {
            struct sockaddr_in serv_addr;
            struct hostent *server = gethostbyname("localhost");
            
            if (server != NULL) {
                bzero((char *)&serv_addr, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
                serv_addr.sin_port = htons(target_port);
                
                if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0) {
                    write(sockfd, command, strlen(command));
                    
                    off_t file_size;
                    if (read(sockfd, &file_size, sizeof(off_t)) == sizeof(off_t)) {
                        write(client_conn, &file_size, sizeof(off_t));
                        
                        char buffer[BUFFER_SIZE];
                        off_t remaining = file_size;
                        while (remaining > 0) {
                            int bytes = read(sockfd, buffer, 
                                           (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
                            if (bytes <= 0) break;
                            write(client_conn, buffer, bytes);
                            remaining -= bytes;
                        }
                    }
                }
            }
            close(sockfd);
        }
    } else {
        write(client_conn, "ERROR: Unsupported filetype for tar", 35);
    }
    
    return 0;
}
int display_files(int client_conn, char *pathname) 
{
    char file_list[BUFFER_SIZE * 4] = {0};
    
    char s1_path[MAX_PATH_LEN];
    snprintf(s1_path, MAX_PATH_LEN, "%s/S1%s", getenv("HOME"), 
             (strcmp(pathname, "~S1") == 0) ? "" : (pathname + 3));

    DIR *dir = opendir(s1_path);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            
            char *ext = strrchr(ent->d_name, '.');
            if (ext && strcmp(ext, ".c") == 0) {
                char output_path[MAX_PATH_LEN];
                snprintf(output_path, sizeof(output_path), "~S1/%s\n", ent->d_name);
                strncat(file_list, output_path, BUFFER_SIZE * 4 - strlen(file_list) - 1);
            }
        }
        closedir(dir);
    }

    char command[MAX_PATH_LEN];
    snprintf(command, MAX_PATH_LEN, "dispfnames %s", pathname);
    char response[BUFFER_SIZE];

    if (send_command_to_server(s2_port, command, response) == 0) {
        strncat(file_list, response, BUFFER_SIZE * 4 - strlen(file_list) - 1);
    }

    if (send_command_to_server(s3_port, command, response) == 0) {
        strncat(file_list, response, BUFFER_SIZE * 4 - strlen(file_list) - 1);
    }

    if (send_command_to_server(s4_port, command, response) == 0) {
        strncat(file_list, response, BUFFER_SIZE * 4 - strlen(file_list) - 1);
    }

    write(client_conn, file_list, strlen(file_list));
    return 0;
}

int forward_to_server(int target_port, char *file_path, char *dest_path) 
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    struct sockaddr_in serv_addr;
    struct hostent *server = gethostbyname("localhost");
    if (server == NULL) {
        close(sockfd);
        return -1;
    }
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(target_port);
    
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    char *filename = basename(file_path);
    char command[MAX_PATH_LEN];
    snprintf(command, MAX_PATH_LEN, "uploadf %s %s", filename, dest_path);
    write(sockfd, command, strlen(command));
    
    char response[10];
    read(sockfd, response, 5);
    
    struct stat st;
    if (stat(file_path, &st) == 0) {
        write(sockfd, &st.st_size, sizeof(off_t));
        
        int fd = open(file_path, O_RDONLY);
        if (fd >= 0) {
            char buffer[BUFFER_SIZE];
            off_t remaining = st.st_size;
            while (remaining > 0) {
                int bytes = read(fd, buffer, (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
                if (bytes <= 0) break;
                write(sockfd, buffer, bytes);
                remaining -= bytes;
            }
            close(fd);
        }
    }
    
    close(sockfd);
    return 0;
}

int send_command_to_server(int port, char *command, char *response) 
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    struct sockaddr_in serv_addr;
    struct hostent *server = gethostbyname("localhost");
    if (server == NULL) {
        close(sockfd);
        return -1;
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    write(sockfd, command, strlen(command));
    
    bzero(response, BUFFER_SIZE);
    read(sockfd, response, BUFFER_SIZE - 1);
    
    close(sockfd);
    return 0;
}

int create_directory_structure(char *path) 
{
    char *temp_path = strdup(path);
    char *token = strtok(temp_path, "/");
    char current_path[MAX_PATH_LEN] = "";
    
    if (path[0] == '/') {
        strcpy(current_path, "/");
    }
    
    while (token != NULL) {
        if (strlen(current_path) > 1) {
            strcat(current_path, "/");
        }
        strcat(current_path, token);
        
        if (mkdir(current_path, 0755) && errno != EEXIST) {
            free(temp_path);
            return -1;
        }
        
        token = strtok(NULL, "/");
    }
    
    free(temp_path);
    return 0;
}

void handle_error(const char *msg) 
{
    perror(msg);
    exit(1);
}
