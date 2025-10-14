# Distributed File System (DFS) - Project

**Author:** Niket Bhatt (110181232)  
**Course:** COMP-8567 Advanced System Programming 
**Term:** Summer 2025  
**Due Date:** August 12, 2025, 11 PM EDT

## Project Overview

This project implements a **Distributed File System** using socket programming in C. The system consists of four servers (S1, S2, S3, S4) that work together to provide a transparent file storage service to multiple clients. Clients interact only with the main server (S1), which intelligently distributes files to appropriate storage servers based on file type.

## System Architecture

### Server Hierarchy
- **S1 (Main Server)** - Primary interface for all client connections
  - Stores `.c` files locally in `~/S1`
  - Coordinates file distribution to specialized servers
  - Handles all client communication
  
- **S2 (PDF Server)** - Dedicated storage for PDF files
  - Stores `.pdf` files in `~/S2`
  - Services S1 requests for PDF operations
  
- **S3 (TXT Server)** - Dedicated storage for text files
  - Stores `.txt` files in `~/S3`
  - Services S1 requests for text file operations
  
- **S4 (ZIP Server)** - Dedicated storage for ZIP files
  - Stores `.zip` files in `~/S4`
  - Services S1 requests for ZIP file operations

### File Distribution Strategy
```
Client Upload → S1 → File Type Detection → Appropriate Server
    ↓
.c files   → Stored locally on S1 (~/S1)
.pdf files → Forwarded to S2 (~/S2)
.txt files → Forwarded to S3 (~/S3)
.zip files → Forwarded to S4 (~/S4)
```

**Important:** Clients are unaware of servers S2, S3, and S4. All operations appear to happen on S1 from the client's perspective.

## Supported File Types

- **C Source Files** (`.c`) - Stored on S1
- **PDF Documents** (`.pdf`) - Stored on S2
- **Text Files** (`.txt`) - Stored on S3
- **ZIP Archives** (`.zip`) - Stored on S4

## Client Commands

### 1. Upload Files (`uploadf`)
**Syntax:** `uploadf filename1 [filename2] [filename3] destination_path`

- Upload 1-3 files simultaneously to the specified destination
- Files must exist in client's current working directory
- Destination path must start with `~S1/`
- Creates directory structure if it doesn't exist

**Examples:**
```bash
# Upload single C file
s25client$ uploadf sample.c ~S1/folder1/folder2

# Upload multiple files of different types
s25client$ uploadf sample.txt new.c test.pdf ~S1/folder1/folder2

# Upload PDF file (transparently stored on S2)
s25client$ uploadf sample.pdf ~S1/folder1/folder2
```

### 2. Download Files (`downlf`)
**Syntax:** `downlf filename1 [filename2]`

- Download 1-2 files from the distributed system
- Files are retrieved from appropriate servers transparently
- Downloaded to client's current working directory

**Examples:**
```bash
# Download single file
s25client$ downlf ~S1/folder1/folder2/sample.c

# Download multiple files
s25client$ downlf ~S1/folder1/folder2/sample.txt ~S1/folder1/folder2/xyz.pdf
```

### 3. Remove Files (`removef`)
**Syntax:** `removef filename1 [filename2]`

- Remove 1-2 files from the distributed system
- Files are deleted from appropriate servers

**Examples:**
```bash
# Remove single file
s25client$ removef ~S1/folder1/folder2/sample.pdf

# Remove multiple files
s25client$ removef ~S1/folder1/folder2/sample.pdf ~S1/folder1/folder2/test.zip
```

### 4. Download TAR Archive (`downltar`)
**Syntax:** `downltar filetype`

- Create and download TAR archive of specified file type
- Supported types: `.c`, `.pdf`, `.txt` (excludes `.zip`)
- Archives include all files of the specified type in the directory tree

**Examples:**
```bash
# Download all C files as tar
s25client$ downltar .c          # Creates cfiles.tar

# Download all PDF files as tar  
s25client$ downltar .pdf        # Creates pdf.tar

# Download all text files as tar
s25client$ downltar .txt        # Creates text.tar
```

### 5. Display File Names (`dispfnames`)
**Syntax:** `dispfnames pathname`

- Display names of all files in specified directory
- Shows files from all servers (S1, S2, S3, S4) in consolidated list
- Files grouped by type (.c, .pdf, .txt, .zip) and sorted alphabetically within groups

**Example:**
```bash
s25client$ dispfnames ~S1/folder1/folder2
```

## Installation and Setup

### Prerequisites
- GCC compiler
- Linux/Unix environment
- Network connectivity between machines/terminals

### Compilation
```bash
# Compile all server programs
gcc -o S1 Niket_Bhatt_110181232_S1.c
gcc -o S2 Niket_Bhatt_110181232_S2.c  
gcc -o S3 Niket_Bhatt_110181232_S3.c
gcc -o S4 Niket_Bhatt_110181232_S4.c

# Compile client program
gcc -o s25client Niket_Bhatt_110181232_s25client.c
```

### Directory Structure
The system automatically creates the following directories:
```
$HOME/
├── S1/    # Main server storage (.c files)
├── S2/    # PDF server storage (.pdf files)
├── S3/    # TXT server storage (.txt files)
└── S4/    # ZIP server storage (.zip files)
```

## Running the System

### Step 1: Start All Servers
**Important:** Servers must run on different machines/terminals and communicate via sockets only.

```bash
# Terminal 1 - Start S1 (Main Server)
./S1

# Terminal 2 - Start S2 (PDF Server)  
./S2

# Terminal 3 - Start S3 (TXT Server)
./S3

# Terminal 4 - Start S4 (ZIP Server)
./S4
```

### Step 2: Start Client
```bash
# Terminal 5 - Start Client
./s25client
```

### Default Port Configuration
- **S1 Server:** Port 4307
- **S2 Server:** Port 4308  
- **S3 Server:** Port 4309
- **S4 Server:** Port 4310

## Technical Implementation

### Process Management
- **S1 Server** uses `fork()` to create child processes for each client
- Child process runs `prcclient()` function in infinite loop
- Parent process continues listening for new client connections
- Proper zombie process cleanup implemented

### Socket Communication
- **TCP/IP** protocol for reliable communication
- **Binary-safe** file transfers
- Error handling for network failures
- Socket reuse with `SO_REUSEADDR`

### File Transfer Protocol
1. Client sends command to S1
2. S1 validates command syntax
3. For uploads: Client sends file size followed by file data
4. For downloads: S1 sends file size followed by file data
5. S1 coordinates with S2/S3/S4 as needed transparently

### Path Management
- All client paths use `~S1/` prefix
- Internal server paths use `~/SX/` structure
- Automatic directory creation for nested paths
- Path validation prevents directory traversal attacks

## Error Handling

The system handles various error conditions:
- **Network Errors:** Connection failures, timeouts
- **File System Errors:** Permission denied, disk full, file not found
- **Protocol Errors:** Invalid commands, malformed requests
- **Server Unavailability:** Graceful degradation when secondary servers are down

## Project Files

1. **Niket_Bhatt_110181232_S1.c** - Main server implementation
2. **Niket_Bhatt_110181232_S2.c** - PDF server implementation  
3. **Niket_Bhatt_110181232_S3.c** - TXT server implementation
4. **Niket_Bhatt_110181232_S4.c** - ZIP server implementation
5. **Niket_Bhatt_110181232_s25client.c** - Client application

## Learning Outcomes Demonstrated

This project demonstrates mastery of:
- **Socket Programming** - TCP/IP communication between distributed processes
- **Process Management** - Fork/exec, signal handling, zombie cleanup
- **File System Operations** - Directory traversal, file I/O, path management
- **System Design** - Distributed architecture, client-server model
- **Error Handling** - Robust error detection and recovery
- **Protocol Design** - Custom communication protocol implementation

## System Limitations

1. **File Types:** Limited to `.c`, `.pdf`, `.txt`, `.zip` extensions
2. **Upload Limit:** Maximum 3 files per upload operation
3. **Download/Remove Limit:** Maximum 2 files per operation
4. **TAR Support:** Only `.c`, `.pdf`, `.txt` files (excludes `.zip`)
5. **Path Restriction:** All paths must start with `~S1/`
6. **Network:** Designed for local network operation

## Security Considerations

- **File Permissions:** Files created with 644 permissions
- **Path Validation:** Basic protection against directory traversal
- **Local Operation:** Designed for trusted local network environment
- **No Authentication:** Educational project with no user authentication

## Troubleshooting

### Common Issues
1. **"Connection refused"** - Ensure all servers are running in correct order
2. **"File not found"** - Verify file exists and path is correct
3. **"Permission denied"** - Check directory permissions and file ownership
4. **"Invalid command"** - Verify command syntax matches specifications

### Debug Tips
- Check server startup order (S2, S3, S4 before S1)
- Verify port availability (no conflicts with other services)
- Ensure home directory has write permissions
- Test with simple operations first

---

**Academic Integrity Notice:** This project is submitted for COMP-8567 evaluation and is subject to MOSS plagiarism detection.
