#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9410

#define UDP_PORT 9411

#define BUFFER_SIZE 65536


int send_all(int socket_fd,
             const char *buffer,
             size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length)
    {
        ssize_t sent =
            send(socket_fd,
                 buffer + total_sent,
                 length - total_sent,
                 0);

        if (sent <= 0)
        {
            return -1;
        }

        total_sent += (size_t)sent;
    }

    return 0;
}


int recv_line(int socket_fd,
              char *buffer,
              size_t size)
{
    size_t index = 0;

    while (index < size - 1)
    {
        char c;

        ssize_t received =
            recv(socket_fd,
                 &c,
                 1,
                 0);

        if (received == 0)
        {
            return 0;
        }

        if (received < 0)
        {
            return -1;
        }

        buffer[index++] = c;

        if (c == '\n')
        {
            break;
        }
    }

    buffer[index] = '\0';

    return 1;
}


int send_file(int socket_fd,
              const char *filename,
              size_t filesize)
{
    FILE *file =
        fopen(filename,
              "rb");

    if (file == NULL)
    {
        return -1;
    }

    unsigned char buffer[8192];

    size_t remaining =
        filesize;

    while (remaining > 0)
    {
        size_t chunk =
            remaining > sizeof(buffer)
                ? sizeof(buffer)
                : remaining;

        size_t bytes_read =
            fread(buffer,
                  1,
                  chunk,
                  file);

        if (bytes_read != chunk)
        {
            fclose(file);
            return -1;
        }

        if (send_all(socket_fd,
                     (const char *)buffer,
                     bytes_read) < 0)
        {
            fclose(file);
            return -1;
        }

        remaining -= bytes_read;
    }

    fclose(file);

    return 0;
}


int recv_all(int socket_fd,
             unsigned char *buffer,
             size_t length)
{
    size_t total_received = 0;

    while (total_received < length)
    {
        ssize_t received =
            recv(socket_fd,
                 buffer + total_received,
                 length - total_received,
                 0);

        if (received <= 0)
        {
            return -1;
        }

        total_received +=
            (size_t)received;
    }

    return 0;
}


int execute_command(int socket_fd,
                    const char *command,
                    char *response,
                    size_t response_size)
{
    char request[BUFFER_SIZE];

    snprintf(request,
             sizeof(request),
             "EXEC %s\n",
             command);

    if (send_all(socket_fd,
                 request,
                 strlen(request)) < 0)
    {
        return -1;
    }

    printf("Sent: EXEC %s\n",
           command);

    int result =
        recv_line(socket_fd,
                  response,
                  response_size);

    if (result <= 0)
    {
        return -1;
    }

    printf("Agent response: %s",
           response);

    return 0;
}


int get_file(int socket_fd,
             const char *filename,
             const char *output_filename)
{
    char request[BUFFER_SIZE];

    snprintf(request,
             sizeof(request),
             "GET %s\n",
             filename);

    if (send_all(socket_fd,
                 request,
                 strlen(request)) < 0)
    {
        return -1;
    }

    printf("Sent: GET %s\n",
           filename);

    char response[BUFFER_SIZE];

    int result =
        recv_line(socket_fd,
                  response,
                  sizeof(response));

    if (result <= 0)
    {
        return -1;
    }

    printf("Agent response: %s",
           response);

    char response_type[64];

    char received_filename[512];

    size_t filesize;

    char sid[64];

    int fields =
        sscanf(response,
               "OK %63s %511s %zu SID:%63s",
               response_type,
               received_filename,
               &filesize,
               sid);

    if (fields != 4 ||
        strcmp(response_type,
               "FILE_SEND") != 0)
    {
        return -1;
    }

    FILE *file =
        fopen(output_filename,
              "wb");

    if (file == NULL)
    {
        return -1;
    }

    unsigned char buffer[8192];

    size_t remaining =
        filesize;

    while (remaining > 0)
    {
        size_t chunk =
            remaining > sizeof(buffer)
                ? sizeof(buffer)
                : remaining;

        if (recv_all(socket_fd,
                     buffer,
                     chunk) < 0)
        {
            fclose(file);
            return -1;
        }

        if (fwrite(buffer,
                   1,
                   chunk,
                   file) != chunk)
        {
            fclose(file);
            return -1;
        }

        remaining -= chunk;
    }

    fclose(file);

    printf("Downloaded file: %s\n",
           output_filename);

    printf("Downloaded bytes: %zu\n",
           filesize);

    return 0;
}


/*
 * =========================================================
 * MAIN
 * =========================================================
 */
int main(void)
{
    int client_socket;

    struct sockaddr_in server_addr;

    char response[BUFFER_SIZE];

    /*
     * =====================================================
     * TCP CONNECTION
     * =====================================================
     */
    client_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (client_socket < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(SERVER_PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(client_socket);

        return 1;
    }

    if (connect(client_socket,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");

        close(client_socket);

        return 1;
    }

    printf("Connected to RemoteOps Agent\n");
    printf("Server: %s:%d\n",
           SERVER_IP,
           SERVER_PORT);


    /*
     * =====================================================
     * AUTH
     * =====================================================
     */
    const char *auth_command =
        "AUTH OPS-0101\n";

    send_all(client_socket,
             auth_command,
             strlen(auth_command));

    printf("Sent: AUTH OPS-0101\n");

    int result =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (result <= 0)
    {
        close(client_socket);
        return 1;
    }

    printf("Agent response: %s",
           response);

    if (strncmp(response,
                "OK AUTHENTICATED",
                strlen("OK AUTHENTICATED")) != 0)
    {
        close(client_socket);
        return 1;
    }

    printf("Authentication successful.\n");


    /*
     * =====================================================
     * SYSINFO
     * =====================================================
     */
    const char *sysinfo_command =
        "SYSINFO\n";

    send_all(client_socket,
             sysinfo_command,
             strlen(sysinfo_command));

    printf("Sent: SYSINFO\n");

    result =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (result <= 0)
    {
        close(client_socket);
        return 1;
    }

    printf("Agent response: %s",
           response);


    /*
     * =====================================================
     * LISTPROC
     * =====================================================
     */
    const char *listproc_command =
        "LISTPROC\n";

    send_all(client_socket,
             listproc_command,
             strlen(listproc_command));

    printf("Sent: LISTPROC\n");

    result =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (result <= 0)
    {
        close(client_socket);
        return 1;
    }

    printf("Agent response:\n%s",
           response);


    /*
     * =====================================================
     * EXEC
     * =====================================================
     */
    const char *exec_commands[] =
    {
        "DATE",
        "UPTIME",
        "DISKFREE",
        "HOSTNAME",
        "WHOAMI"
    };

    int command_count =
        sizeof(exec_commands) /
        sizeof(exec_commands[0]);

    for (int i = 0;
         i < command_count;
         i++)
    {
        if (execute_command(client_socket,
                            exec_commands[i],
                            response,
                            sizeof(response)) < 0)
        {
            close(client_socket);
            return 1;
        }
    }


    /*
     * =====================================================
     * INVALID EXEC
     * =====================================================
     */
    execute_command(client_socket,
                    "LS",
                    response,
                    sizeof(response));


    /*
     * =====================================================
     * PUT
     * =====================================================
     */
    const char *upload_file =
        "test_upload.txt";

    struct stat file_info;

    if (stat(upload_file,
             &file_info) < 0)
    {
        perror("stat");

        close(client_socket);

        return 1;
    }

    size_t upload_size =
        (size_t)file_info.st_size;

    char put_command[BUFFER_SIZE];

    snprintf(put_command,
             sizeof(put_command),
             "PUT %s %zu\n",
             upload_file,
             upload_size);

    send_all(client_socket,
             put_command,
             strlen(put_command));

    printf("Sent: PUT %s %zu\n",
           upload_file,
           upload_size);

    if (send_file(client_socket,
                  upload_file,
                  upload_size) < 0)
    {
        printf("PUT failed.\n");

        close(client_socket);

        return 1;
    }

    printf("File bytes sent: %zu\n",
           upload_size);

    result =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (result <= 0)
    {
        close(client_socket);
        return 1;
    }

    printf("Agent response: %s",
           response);


    /*
     * =====================================================
     * GET
     * =====================================================
     */
    if (get_file(client_socket,
                 "test_upload.txt",
                 "downloaded_test_upload.txt") < 0)
    {
        printf("GET failed.\n");

        close(client_socket);

        return 1;
    }


    /*
     * =====================================================
     * UDP SOCKET
     * =====================================================
     */
    int udp_socket =
        socket(AF_INET,
               SOCK_DGRAM,
               0);

    if (udp_socket < 0)
    {
        perror("UDP socket");

        close(client_socket);

        return 1;
    }

    struct sockaddr_in udp_addr;

    memset(&udp_addr,
           0,
           sizeof(udp_addr));

    udp_addr.sin_family =
        AF_INET;

    udp_addr.sin_addr.s_addr =
        INADDR_ANY;

    udp_addr.sin_port =
        htons(UDP_PORT);

    if (bind(udp_socket,
             (struct sockaddr *)&udp_addr,
             sizeof(udp_addr)) < 0)
    {
        perror("bind UDP");

        close(udp_socket);
        close(client_socket);

        return 1;
    }

    printf("UDP monitor listening on port %d\n",
           UDP_PORT);


    /*
     * =====================================================
     * MONITOR START
     * =====================================================
     */
    char monitor_start[BUFFER_SIZE];

    snprintf(monitor_start,
             sizeof(monitor_start),
             "MONITOR START %d\n",
             UDP_PORT);

    send_all(client_socket,
             monitor_start,
             strlen(monitor_start));

    printf("Sent: MONITOR START %d\n",
           UDP_PORT);

    result =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (result <= 0)
    {
        close(udp_socket);
        close(client_socket);
        return 1;
    }

    printf("Agent response: %s",
           response);


    /*
     * =====================================================
     * RECEIVE UDP MONITORING DATAGRAMS
     * =====================================================
     */
    struct sockaddr_in monitor_sender;

    socklen_t monitor_sender_len =
        sizeof(monitor_sender);

    char udp_buffer[BUFFER_SIZE];

    int packets_received = 0;

    printf("Waiting for UDP monitoring packets...\n");

    while (packets_received < 5)
    {
        fd_set read_fds;

        FD_ZERO(&read_fds);

        FD_SET(udp_socket,
               &read_fds);

        struct timeval timeout;

        timeout.tv_sec = 5;

        timeout.tv_usec = 0;

        int ready =
            select(udp_socket + 1,
                   &read_fds,
                   NULL,
                   NULL,
                   &timeout);

        if (ready < 0)
        {
            perror("select");
            break;
        }

        if (ready == 0)
        {
            printf("UDP monitoring timeout.\n");
            break;
        }

        ssize_t received =
            recvfrom(udp_socket,
                     udp_buffer,
                     sizeof(udp_buffer) - 1,
                     0,
                     (struct sockaddr *)&monitor_sender,
                     &monitor_sender_len);

        if (received < 0)
        {
            perror("recvfrom");
            break;
        }

        udp_buffer[received] =
            '\0';

        printf("UDP received: %s\n",
               udp_buffer);

        packets_received++;
    }


    /*
     * =====================================================
     * MONITOR STOP
     * =====================================================
     */
    const char *monitor_stop =
        "MONITOR STOP\n";

    send_all(client_socket,
             monitor_stop,
             strlen(monitor_stop));

    printf("Sent: MONITOR STOP\n");

    result =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (result > 0)
    {
        printf("Agent response: %s",
               response);
    }


    /*
     * =====================================================
     * QUIT
     * =====================================================
     */
    const char *quit_command =
        "QUIT\n";

    send_all(client_socket,
             quit_command,
             strlen(quit_command));

    printf("Sent: QUIT\n");

    result =
        recv_line(client_socket,
                  response,
                  sizeof(response));

    if (result > 0)
    {
        printf("Agent response: %s",
               response);
    }


    /*
     * Close sockets.
     */
    close(udp_socket);

    close(client_socket);

    printf("Connections closed.\n");

    return 0;
}
