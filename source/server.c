#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <network.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <wiiuse/wpad.h>

#define PORT 9001
#define MAXPATHLEN 1024
#define BLOCKSIZE 4096

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void initialize() {
    VIDEO_Init();
    WPAD_Init();

    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}

bool initialize_network() {
    char ip[16];
    if (if_config(ip, NULL, NULL, true, 20) < 0) {
        printf("Network configuration failed!\n");
        return false;
    }
    printf("Network initialized. IP: %s\n", ip);
    return true;
}

bool initialize_storage() {
    // Initialize the SD card
    __io_wiisd.startup();
    
    if (!fatMountSimple("sd", &__io_wiisd)) {
        printf("Failed to initialize FAT filesystem on SD card\n");
        return false;
    }
    printf("Storage initialized on SD card\n");
    return true;
}

void receive_file(s32 client_sock) {
    char filename[MAXPATHLEN];
    char buffer[BLOCKSIZE];
    s32 filesize, received;
    FILE *file;

    // Receive filename
    net_recv(client_sock, filename, MAXPATHLEN, 0);
    printf("Receiving file: %s\n", filename);

    // Receive filesize
    net_recv(client_sock, &filesize, sizeof(filesize), 0);
    filesize = ntohl(filesize);  // Convert from network byte order
    printf("File size: %d bytes\n", filesize);

    // Open file for writing
    file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open file for reading\n");
        return;
    }

    // Receive file data
    s32 total_received = 0;
    while (total_received < filesize) {
        received = net_recv(client_sock, buffer, BLOCKSIZE, 0);
        if (received <= 0) break;
        fwrite(buffer, 1, received, file);
        total_received += received;
        printf("Received %d of %d bytes\r", total_received, filesize);
    }
    printf("\n");

    fclose(file);
    printf("File transfer complete\n");
}

void safe_shutdown() {
    fatUnmount("sd:");
    __io_wiisd.shutdown();
}

void run_server() {
    s32 sock_fd, client_sock;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(client);

    sock_fd = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock_fd < 0) {
        printf("Failed to create socket\n");
        return;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (net_bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Bind failed\n");
        net_close(sock_fd);
        return;
    }

    if (net_listen(sock_fd, 1) < 0) {
        printf("Listen failed\n");
        net_close(sock_fd);
        return;
    }

    printf("Server listening on port %d\n", PORT);

    while(1) {
        
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);

        if (pressed & WPAD_BUTTON_HOME) 
            exit(0);

        printf("Waiting for connection...\n");
        client_sock = net_accept(sock_fd, (struct sockaddr *)&client, &client_len);
        if (client_sock < 0) {
            printf("Accept failed\n");
            continue;
        }

        printf("Client connected\n");
        receive_file(client_sock);
        net_close(client_sock);
    }

    net_close(sock_fd);
}

int main(int argc, char **argv) {
    initialize();

    if (!initialize_network()) {
        printf("Failed to initialize network. Exiting...\n");
        exit(1);
    }

    if (!initialize_storage()) {
        printf("Failed to initialize storage. Exiting...\n");
        exit(1);
    }

    run_server();

    safe_shutdown();

    return 0;
}
