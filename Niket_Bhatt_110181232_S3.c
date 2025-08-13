// Distributed File System - S3 Server Implementation by Niket
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

void process_s1_request(int s1_conn);
int handle_txt_upload(int s1_conn, char *file_path, char *dest_path);
int handle_txt_download(int s1_conn, char *file_path);
int handle_txt_removal(int s1_conn, char *file_path);
int create_txt_tar(int s1_conn);
int display_txt_files(int s1_conn, char *pathname);
int create_directory_structure(char *path);
void handle_error(const char *msg);

int main(int argc, char *argv[]) 
{
    int port = 4309;
    
    if (argc == 2) {
        port = atoi(argv[1]);
    }

    int server_socket, s1_conn;
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
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        handle_error("Binding failed");
    }

    listen(server_socket, MAX_CLIENTS);
    client_len = sizeof(client_addr);

    printf("S3 TXT Server started on port %d\n", port);

    while (1) {
        s1_conn = accept(server_socket, (struct sockaddr *) &client_addr, &client_len);
        if (s1_conn < 0) {
            handle_error("Accept failed");
        }

        child_pid = fork();
        if (child_pid < 0) {
            handle_error("Fork failed");
        }

        if (child_pid == 0) {
            close(server_socket);
            process_s1_request(s1_conn);
            close(s1_conn);
            exit(0);
        } else {
            close(s1_conn);
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(server_socket);
    return 0;
}

void process_s1_request(int s1_conn) 
{
    char buffer[BUFFER_SIZE];
    int n;
    
    bzero(buffer, BUFFER_SIZE);
    n = read(s1_conn, buffer, BUFFER_SIZE - 1);
    if (n < 0) {
        handle_error("Reading from S1 failed");
    }
    
    printf("S3 received: %s\n", buffer);
    
    char *cmd = strtok(buffer, " ");
    if (cmd == NULL) {
        write(s1_conn, "ERROR: Invalid command", 22);
        return;
    }
    
    if (strcmp(cmd, "uploadf") == 0) {
        char *file_path = strtok(NULL, " ");
        char *dest_path = strtok(NULL, " ");
        if (file_path == NULL || dest_path == NULL) {
            write(s1_conn, "ERROR: Missing parameters", 25);
            return;
        }
        handle_txt_upload(s1_conn, file_path, dest_path);
    } 
    else if (strcmp(cmd, "downlf") == 0) {
        char *file_path = strtok(NULL, " ");
        if (file_path == NULL) {
            write(s1_conn, "ERROR: Missing filename", 23);
            return;
        }
        handle_txt_download(s1_conn, file_path);
    } 
    else if (strcmp(cmd, "removef") == 0) {
        char *file_path = strtok(NULL, " ");
        if (file_path == NULL) {
            write(s1_conn, "ERROR: Missing filename", 23);
            return;
        }
        handle_txt_removal(s1_conn, file_path);
    } 
    else if (strcmp(cmd, "downltar") == 0) {
        create_txt_tar(s1_conn);
    } 
    else if (strcmp(cmd, "dispfnames") == 0) {
        char *pathname = strtok(NULL, " ");
        if (pathname == NULL) {
            write(s1_conn, "ERROR: Missing pathname", 23);
            return;
        }
        display_txt_files(s1_conn, pathname);
    } 
    else {
        write(s1_conn, "ERROR: Unknown command", 22);
    }
}

int handle_txt_upload(int s1_conn, char *file_path, char *dest_path) 
{
    write(s1_conn, "READY", 5);
    
    off_t file_size;
    if (read(s1_conn, &file_size, sizeof(off_t)) != sizeof(off_t)) {
        write(s1_conn, "ERROR: Failed to read file size", 31);
        return -1;
    }
    
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), dest_path + 3);
    create_directory_structure(s3_path);
    
    char full_dest_path[MAX_PATH_LEN];
    snprintf(full_dest_path, MAX_PATH_LEN, "%s/%s", s3_path, file_path);
    
    int fd = open(full_dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        write(s1_conn, "ERROR: Failed to create file", 28);
        return -1;
    }
    
    char buffer[BUFFER_SIZE];
    off_t remaining = file_size;
    while (remaining > 0) {
        int bytes = read(s1_conn, buffer, (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
        if (bytes <= 0) {
            close(fd);
            write(s1_conn, "ERROR: Transfer failed", 22);
            return -1;
        }
        write(fd, buffer, bytes);
        remaining -= bytes;
    }
    close(fd);
    
    write(s1_conn, "SUCCESS: TXT stored in S3", 25);
    return 0;
}

int handle_txt_download(int s1_conn, char *file_path) 
{
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), file_path + 3);
    
    struct stat st;
    if (stat(s3_path, &st) != 0) {
        off_t error_size = -1;
        write(s1_conn, &error_size, sizeof(off_t));
        return -1;
    }
    
    int fd = open(s3_path, O_RDONLY);
    if (fd < 0) {
        off_t error_size = -1;
        write(s1_conn, &error_size, sizeof(off_t));
        return -1;
    }
    
    write(s1_conn, &st.st_size, sizeof(off_t));
    
    char buffer[BUFFER_SIZE];
    off_t remaining = st.st_size;
    while (remaining > 0) {
        ssize_t n = read(fd, buffer, (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE);
        if (n <= 0) {
            close(fd);
            return -1;
        }
        write(s1_conn, buffer, n);
        remaining -= n;
    }
    close(fd);
    return 0;
}

int handle_txt_removal(int s1_conn, char *file_path) 
{
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), file_path + 3);
    
    if (unlink(s3_path) == 0) {
        write(s1_conn, "SUCCESS: TXT deleted from S3", 28);
    } else {
        write(s1_conn, "ERROR: TXT not found in S3", 26);
    }
    
    return 0;
}

int create_txt_tar(int s1_conn) 
{
    char s3_dir[MAX_PATH_LEN];
    snprintf(s3_dir, MAX_PATH_LEN, "%s/S3", getenv("HOME"));
    
    char tar_cmd[MAX_PATH_LEN * 2];
    snprintf(tar_cmd, sizeof(tar_cmd),
             "find %s -type f -name \"*.txt\" | tar -cf /tmp/text.tar -T -", s3_dir);

    if (system(tar_cmd) != 0) {
        off_t error_size = -1;
        write(s1_conn, &error_size, sizeof(off_t));
        return -1;
    }
    
    struct stat st;
    if (stat("/tmp/text.tar", &st) != 0) {
        off_t error_size = -1;
        write(s1_conn, &error_size, sizeof(off_t));
        return -1;
    }
    
    int fd = open("/tmp/text.tar", O_RDONLY);
    if (fd < 0) {
        off_t error_size = -1;
        write(s1_conn, &error_size, sizeof(off_t));
        return -1;
    }
    
    write(s1_conn, &st.st_size, sizeof(off_t));
    
    off_t remaining = st.st_size;
    while (remaining > 0) {
        ssize_t sent = sendfile(s1_conn, fd, NULL, remaining);
        if (sent <= 0) {
            close(fd);
            break;
        }
        remaining -= sent;
    }
    
    close(fd);
    unlink("/tmp/text.tar");
    return 0;
}

int display_txt_files(int s1_conn, char *pathname) 
{
    char s3_path[MAX_PATH_LEN];
    snprintf(s3_path, MAX_PATH_LEN, "%s/S3%s", getenv("HOME"), 
             (strcmp(pathname, "~S1") == 0) ? "" : (pathname + 3));
    
    struct stat st;
    if (stat(s3_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        write(s1_conn, "", 0);
        return 0;
    }
    
    char file_list[BUFFER_SIZE * 2] = {0};
    DIR *dir = opendir(s3_path);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            
            char *ext = strrchr(ent->d_name, '.');
            if (ext && strcmp(ext, ".txt") == 0) {
                char output_path[MAX_PATH_LEN];
                snprintf(output_path, sizeof(output_path), "~S1/%s\n", ent->d_name);
                strncat(file_list, output_path, BUFFER_SIZE * 2 - strlen(file_list) - 1);
            }
        }
        closedir(dir);
    }
    
    write(s1_conn, file_list, strlen(file_list));
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
