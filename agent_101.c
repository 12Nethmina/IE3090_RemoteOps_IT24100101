#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdint.h>

#define PORT 9410
#define BACKLOG 10
#define BUFFER_SIZE 65536

#define AUTH_TOKEN "OPS-0101"
#define SID "1010"

#define STORAGE_DIR "./agentfiles/IT24100101"
#define MAX_FILE_SIZE (10ULL * 1024ULL * 1024ULL)

#define MONITOR_INTERVAL 2


/*
 * =========================================================
 * MONITOR CONTEXT
 * =========================================================
 */
typedef struct
{
    int active;
    int udp_port;
    struct in_addr controller_ip;
    pthread_mutex_t mutex;

} MonitorContext;


/*
 * =========================================================
 * SEND ALL
 * =========================================================
 */
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


/*
 * =========================================================
 * RECEIVE ONE COMPLETE LINE
 * =========================================================
 */
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


/*
 * =========================================================
 * RECEIVE EXACT NUMBER OF BYTES
 * =========================================================
 */
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

        if (received == 0)
        {
            return -1;
        }

        if (received < 0)
        {
            return -1;
        }

        total_received += (size_t)received;
    }

    return 0;
}


/*
 * =========================================================
 * DISCARD BYTES
 * =========================================================
 */
int discard_bytes(int socket_fd,
                  size_t length)
{
    unsigned char buffer[8192];

    size_t remaining = length;

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
            return -1;
        }

        remaining -= chunk;
    }

    return 0;
}


/*
 * =========================================================
 * SYSINFO FUNCTIONS
 * =========================================================
 */

double get_cpu_load(void)
{
    FILE *file =
        fopen("/proc/loadavg", "r");

    if (file == NULL)
    {
        return 0.0;
    }

    double load = 0.0;

    if (fscanf(file,
               "%lf",
               &load) != 1)
    {
        load = 0.0;
    }

    fclose(file);

    return load;
}


long get_memory_used_mb(void)
{
    FILE *file =
        fopen("/proc/meminfo", "r");

    if (file == NULL)
    {
        return 0;
    }

    long mem_total_kb = 0;
    long mem_available_kb = 0;

    char line[256];

    while (fgets(line,
                 sizeof(line),
                 file) != NULL)
    {
        if (sscanf(line,
                   "MemTotal: %ld kB",
                   &mem_total_kb) == 1)
        {
            continue;
        }

        if (sscanf(line,
                   "MemAvailable: %ld kB",
                   &mem_available_kb) == 1)
        {
            continue;
        }

        if (mem_total_kb > 0 &&
            mem_available_kb > 0)
        {
            break;
        }
    }

    fclose(file);

    long used_kb =
        mem_total_kb -
        mem_available_kb;

    if (used_kb < 0)
    {
        used_kb = 0;
    }

    return used_kb / 1024;
}


long get_uptime_sec(void)
{
    FILE *file =
        fopen("/proc/uptime", "r");

    if (file == NULL)
    {
        return 0;
    }

    double uptime = 0.0;

    if (fscanf(file,
               "%lf",
               &uptime) != 1)
    {
        uptime = 0.0;
    }

    fclose(file);

    return (long)uptime;
}


/*
 * =========================================================
 * CREATE SYSINFO STRING
 * =========================================================
 */
void build_sysinfo_message(char *message,
                           size_t message_size)
{
    double cpu_load =
        get_cpu_load();

    long memory_used_mb =
        get_memory_used_mb();

    long uptime_sec =
        get_uptime_sec();

    snprintf(message,
             message_size,
             "SYSINFO %.2f %ld %ld SID:%s",
             cpu_load,
             memory_used_mb,
             uptime_sec,
             SID);
}


/*
 * =========================================================
 * SEND SYSINFO RESPONSE
 * =========================================================
 */
void handle_sysinfo(int client_socket)
{
    double cpu_load =
        get_cpu_load();

    long memory_used_mb =
        get_memory_used_mb();

    long uptime_sec =
        get_uptime_sec();

    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "OK SYSINFO %.2f %ld %ld SID:%s\n",
             cpu_load,
             memory_used_mb,
             uptime_sec,
             SID);

    if (send_all(client_socket,
                 response,
                 strlen(response)) < 0)
    {
        perror("send SYSINFO");
        return;
    }

    printf("Sent: %s",
           response);
}


/*
 * =========================================================
 * LISTPROC
 * =========================================================
 */
void handle_listproc(int client_socket)
{
    FILE *process_pipe;

    char line[512];

    char response[BUFFER_SIZE];

    size_t offset = 0;

    process_pipe =
        popen("ps -e -o pid=,comm=", "r");

    if (process_pipe == NULL)
    {
        const char *error_response =
            "ERR 006 PROCESS_LIST_FAILED SID:" SID "\n";

        send_all(client_socket,
                 error_response,
                 strlen(error_response));

        return;
    }

    int written =
        snprintf(response,
                 sizeof(response),
                 "OK PROCS ");

    if (written < 0)
    {
        pclose(process_pipe);
        return;
    }

    offset = (size_t)written;

    int first_process = 1;

    while (fgets(line,
                 sizeof(line),
                 process_pipe) != NULL)
    {
        int pid;

        char process_name[256];

        if (sscanf(line,
                   "%d %255s",
                   &pid,
                   process_name) != 2)
        {
            continue;
        }

        if (!first_process)
        {
            written =
                snprintf(response + offset,
                         sizeof(response) - offset,
                         ",");

            if (written < 0 ||
                (size_t)written >=
                    sizeof(response) - offset)
            {
                break;
            }

            offset += (size_t)written;
        }

        written =
            snprintf(response + offset,
                     sizeof(response) - offset,
                     "%d/%s",
                     pid,
                     process_name);

        if (written < 0 ||
            (size_t)written >=
                sizeof(response) - offset)
        {
            break;
        }

        offset += (size_t)written;

        first_process = 0;
    }

    pclose(process_pipe);

    snprintf(response + offset,
             sizeof(response) - offset,
             " SID:%s\n",
             SID);

    send_all(client_socket,
             response,
             strlen(response));

    printf("Sent LISTPROC response.\n");
}


/*
 * =========================================================
 * EXEC WHITELIST
 * =========================================================
 */
void handle_exec(int client_socket,
                 const char *command_name)
{
    const char *system_command = NULL;

    if (strcmp(command_name,
               "DATE") == 0)
    {
        system_command = "date";
    }
    else if (strcmp(command_name,
                    "UPTIME") == 0)
    {
        system_command = "uptime";
    }
    else if (strcmp(command_name,
                    "DISKFREE") == 0)
    {
        system_command =
            "df -h / | tail -n 1";
    }
    else if (strcmp(command_name,
                    "HOSTNAME") == 0)
    {
        system_command = "hostname";
    }
    else if (strcmp(command_name,
                    "WHOAMI") == 0)
    {
        system_command = "whoami";
    }
    else
    {
        char response[BUFFER_SIZE];

        snprintf(response,
                 sizeof(response),
                 "ERR 002 COMMAND_NOT_ALLOWED SID:%s\n",
                 SID);

        send_all(client_socket,
                 response,
                 strlen(response));

        printf("Rejected EXEC command: %s\n",
               command_name);

        return;
    }

    FILE *command_pipe =
        popen(system_command,
              "r");

    if (command_pipe == NULL)
    {
        char response[BUFFER_SIZE];

        snprintf(response,
                 sizeof(response),
                 "ERR 007 EXEC_FAILED SID:%s\n",
                 SID);

        send_all(client_socket,
                 response,
                 strlen(response));

        return;
    }

    char output[8192];

    size_t total = 0;

    while (total < sizeof(output) - 1)
    {
        size_t bytes_read =
            fread(output + total,
                  1,
                  sizeof(output) - 1 - total,
                  command_pipe);

        if (bytes_read == 0)
        {
            break;
        }

        total += bytes_read;
    }

    output[total] = '\0';

    pclose(command_pipe);

    for (size_t i = 0;
         i < total;
         i++)
    {
        if (output[i] == '\n' ||
            output[i] == '\r' ||
            output[i] == '\t')
        {
            output[i] = ' ';
        }
    }

    while (total > 0 &&
           output[total - 1] == ' ')
    {
        output[total - 1] = '\0';
        total--;
    }

    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "OK EXEC_RESULT %s SID:%s\n",
             output,
             SID);

    send_all(client_socket,
             response,
             strlen(response));

    printf("Sent: %s",
           response);
}


/*
 * =========================================================
 * PUT
 * =========================================================
 */
void handle_put(int client_socket,
                const char *filename,
                size_t filesize)
{
    if (strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL ||
        strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0)
    {
        discard_bytes(client_socket,
                      filesize);

        const char *response =
            "ERR 008 INVALID_FILENAME SID:" SID "\n";

        send_all(client_socket,
                 response,
                 strlen(response));

        return;
    }

    if ((unsigned long long)filesize >
        MAX_FILE_SIZE)
    {
        discard_bytes(client_socket,
                      filesize);

        const char *response =
            "ERR 004 FILE_TOO_LARGE SID:" SID "\n";

        send_all(client_socket,
                 response,
                 strlen(response));

        printf("Rejected oversized file: %s\n",
               filename);

        return;
    }

    mkdir("agentfiles", 0755);
    mkdir(STORAGE_DIR, 0755);

    char path[BUFFER_SIZE];

    snprintf(path,
             sizeof(path),
             "%s/%s",
             STORAGE_DIR,
             filename);

    FILE *file =
        fopen(path, "wb");

    if (file == NULL)
    {
        discard_bytes(client_socket,
                      filesize);

        const char *response =
            "ERR 009 FILE_OPEN_FAILED SID:" SID "\n";

        send_all(client_socket,
                 response,
                 strlen(response));

        perror("fopen");

        return;
    }

    unsigned char buffer[8192];

    size_t remaining =
        filesize;

    int transfer_ok = 1;

    while (remaining > 0)
    {
        size_t chunk =
            remaining > sizeof(buffer)
                ? sizeof(buffer)
                : remaining;

        if (recv_all(client_socket,
                     buffer,
                     chunk) < 0)
        {
            transfer_ok = 0;
            break;
        }

        if (fwrite(buffer,
                   1,
                   chunk,
                   file) != chunk)
        {
            transfer_ok = 0;
            break;
        }

        remaining -= chunk;
    }

    fclose(file);

    if (!transfer_ok)
    {
        remove(path);

        const char *response =
            "ERR 010 FILE_TRANSFER_FAILED SID:" SID "\n";

        send_all(client_socket,
                 response,
                 strlen(response));

        printf("PUT failed: %s\n",
               filename);

        return;
    }

    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "OK FILE_RECEIVED %s SID:%s\n",
             filename,
             SID);

    send_all(client_socket,
             response,
             strlen(response));

    printf("PUT received: %s (%zu bytes)\n",
           filename,
           filesize);
}


/*
 * =========================================================
 * GET
 * =========================================================
 */
void handle_get(int client_socket,
                const char *filename)
{
    if (strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL ||
        strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0)
    {
        const char *response =
            "ERR 005 FILE_NOT_FOUND SID:" SID "\n";

        send_all(client_socket,
                 response,
                 strlen(response));

        return;
    }

    char path[BUFFER_SIZE];

    snprintf(path,
             sizeof(path),
             "%s/%s",
             STORAGE_DIR,
             filename);

    FILE *file =
        fopen(path, "rb");

    if (file == NULL)
    {
        const char *response =
            "ERR 005 FILE_NOT_FOUND SID:" SID "\n";

        send_all(client_socket,
                 response,
                 strlen(response));

        printf("GET file not found: %s\n",
               filename);

        return;
    }

    if (fseek(file,
              0,
              SEEK_END) != 0)
    {
        fclose(file);

        return;
    }

    long file_size_long =
        ftell(file);

    if (file_size_long < 0)
    {
        fclose(file);

        return;
    }

    size_t filesize =
        (size_t)file_size_long;

    rewind(file);

    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "OK FILE_SEND %s %zu SID:%s\n",
             filename,
             filesize,
             SID);

    if (send_all(client_socket,
                 response,
                 strlen(response)) < 0)
    {
        fclose(file);

        return;
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
            break;
        }

        if (send_all(client_socket,
                     (const char *)buffer,
                     bytes_read) < 0)
        {
            break;
        }

        remaining -= bytes_read;
    }

    fclose(file);

    printf("GET sent: %s (%zu bytes)\n",
           filename,
           filesize);
}


/*
 * =========================================================
 * UDP MONITOR THREAD
 * =========================================================
 */
void *monitor_thread_function(void *arg)
{
    MonitorContext *context =
        (MonitorContext *)arg;

    int udp_socket =
        socket(AF_INET,
               SOCK_DGRAM,
               0);

    if (udp_socket < 0)
    {
        perror("UDP socket");
        return NULL;
    }

    struct sockaddr_in destination;

    memset(&destination,
           0,
           sizeof(destination));

    destination.sin_family =
        AF_INET;

    destination.sin_port =
        htons(context->udp_port);

    destination.sin_addr =
        context->controller_ip;

    while (1)
    {
        /*
         * Check whether monitoring is still active.
         */
        pthread_mutex_lock(&context->mutex);

        int active =
            context->active;

        pthread_mutex_unlock(&context->mutex);

        if (!active)
        {
            break;
        }

        /*
         * Build SYSINFO-style UDP message.
         */
        char message[BUFFER_SIZE];

        build_sysinfo_message(message,
                              sizeof(message));

        /*
         * Send UDP datagram.
         */
        ssize_t sent =
            sendto(udp_socket,
                   message,
                   strlen(message),
                   0,
                   (struct sockaddr *)&destination,
                   sizeof(destination));

        if (sent < 0)
        {
            perror("sendto");
            break;
        }

        printf("UDP monitoring sent: %s\n",
               message);

        /*
         * Wait two seconds.
         */
        sleep(MONITOR_INTERVAL);
    }

    close(udp_socket);

    return NULL;
}


/*
 * =========================================================
 * START MONITOR
 * =========================================================
 */
int start_monitor(MonitorContext **monitor,
                   struct sockaddr_in *client_addr,
                   int udp_port)
{
    /*
     * Do not create a second monitoring thread
     * if one is already active.
     */
    if (*monitor != NULL)
    {
        return 0;
    }

    MonitorContext *context =
        malloc(sizeof(MonitorContext));

    if (context == NULL)
    {
        return -1;
    }

    context->active = 1;

    context->udp_port =
        udp_port;

    context->controller_ip =
        client_addr->sin_addr;

    pthread_mutex_init(&context->mutex,
                       NULL);

    pthread_t thread_id;

    if (pthread_create(&thread_id,
                       NULL,
                       monitor_thread_function,
                       context) != 0)
    {
        pthread_mutex_destroy(&context->mutex);

        free(context);

        return -1;
    }

    /*
     * We store the thread ID indirectly by making
     * the context available. The client thread will
     * temporarily use a separate field below.
     */
    *((pthread_t *)((char *)context -
                    sizeof(pthread_t))) = thread_id;

    *monitor = context;

    return 1;
}


/*
 * =========================================================
 * STOP MONITOR
 * =========================================================
 */
void stop_monitor(MonitorContext **monitor,
                  pthread_t *thread_id)
{
    if (*monitor == NULL)
    {
        return;
    }

    MonitorContext *context =
        *monitor;

    pthread_mutex_lock(&context->mutex);

    context->active = 0;

    pthread_mutex_unlock(&context->mutex);

    pthread_join(*thread_id,
                 NULL);

    pthread_mutex_destroy(&context->mutex);

    free(context);

    *monitor = NULL;
}


/*
 * =========================================================
 * CLIENT THREAD
 * =========================================================
 */
void *handle_client(void *arg)
{
    int client_socket =
        *(int *)arg;

    free(arg);

    struct sockaddr_in client_addr;

    socklen_t client_len =
        sizeof(client_addr);

    if (getpeername(client_socket,
                    (struct sockaddr *)&client_addr,
                    &client_len) != 0)
    {
        close(client_socket);
        pthread_exit(NULL);
    }

    printf("Client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    char buffer[BUFFER_SIZE];

    int authenticated = 0;

    MonitorContext *monitor = NULL;

    pthread_t monitor_thread_id;

    /*
     * =====================================================
     * AUTHENTICATION
     * =====================================================
     */
    while (!authenticated)
    {
        int result =
            recv_line(client_socket,
                      buffer,
                      sizeof(buffer));

        if (result <= 0)
        {
            close(client_socket);
            pthread_exit(NULL);
        }

        buffer[strcspn(buffer,
                       "\r\n")] = '\0';

        printf("Received: %s\n",
               buffer);

        if (strncmp(buffer,
                    "AUTH ",
                    5) != 0)
        {
            const char *response =
                "ERR 003 AUTH_REQUIRED SID:" SID "\n";

            send_all(client_socket,
                     response,
                     strlen(response));

            close(client_socket);

            pthread_exit(NULL);
        }

        char token[BUFFER_SIZE];

        if (sscanf(buffer,
                   "AUTH %1023s",
                   token) != 1)
        {
            const char *response =
                "ERR 001 AUTH_FAILED SID:" SID "\n";

            send_all(client_socket,
                     response,
                     strlen(response));

            continue;
        }

        if (strcmp(token,
                   AUTH_TOKEN) == 0)
        {
            const char *response =
                "OK AUTHENTICATED SID:" SID "\n";

            send_all(client_socket,
                     response,
                     strlen(response));

            authenticated = 1;

            printf("Authentication successful.\n");
        }
        else
        {
            const char *response =
                "ERR 001 AUTH_FAILED SID:" SID "\n";

            send_all(client_socket,
                     response,
                     strlen(response));

            printf("Authentication failed.\n");
        }
    }


    /*
     * =====================================================
     * AUTHENTICATED COMMANDS
     * =====================================================
     */
    while (authenticated)
    {
        int result =
            recv_line(client_socket,
                      buffer,
                      sizeof(buffer));

        if (result == 0)
        {
            printf("Client disconnected.\n");

            break;
        }

        if (result < 0)
        {
            perror("recv");

            break;
        }

        buffer[strcspn(buffer,
                       "\r\n")] = '\0';

        printf("Authenticated command received: %s\n",
               buffer);


        /*
         * SYSINFO
         */
        if (strcmp(buffer,
                   "SYSINFO") == 0)
        {
            handle_sysinfo(client_socket);
        }


        /*
         * LISTPROC
         */
        else if (strcmp(buffer,
                        "LISTPROC") == 0)
        {
            handle_listproc(client_socket);
        }


        /*
         * EXEC
         */
        else if (strncmp(buffer,
                         "EXEC ",
                         5) == 0)
        {
            char command_name[BUFFER_SIZE];

            if (sscanf(buffer,
                       "EXEC %1023s",
                       command_name) == 1)
            {
                handle_exec(client_socket,
                            command_name);
            }
            else
            {
                const char *response =
                    "ERR 002 COMMAND_NOT_ALLOWED SID:" SID "\n";

                send_all(client_socket,
                         response,
                         strlen(response));
            }
        }


        /*
         * PUT
         */
        else if (strncmp(buffer,
                         "PUT ",
                         4) == 0)
        {
            char filename[512];

            size_t filesize;

            if (sscanf(buffer,
                       "PUT %511s %zu",
                       filename,
                       &filesize) != 2)
            {
                const char *response =
                    "ERR 011 INVALID_PUT_FORMAT SID:" SID "\n";

                send_all(client_socket,
                         response,
                         strlen(response));
            }
            else
            {
                handle_put(client_socket,
                           filename,
                           filesize);
            }
        }


        /*
         * GET
         */
        else if (strncmp(buffer,
                         "GET ",
                         4) == 0)
        {
            char filename[512];

            if (sscanf(buffer,
                       "GET %511s",
                       filename) == 1)
            {
                handle_get(client_socket,
                           filename);
            }
            else
            {
                const char *response =
                    "ERR 005 FILE_NOT_FOUND SID:" SID "\n";

                send_all(client_socket,
                         response,
                         strlen(response));
            }
        }


        /*
         * =================================================
         * MONITOR START
         * =================================================
         */
        else if (strncmp(buffer,
                         "MONITOR START ",
                         14) == 0)
        {
            int udp_port;

            if (sscanf(buffer + 14,
                       "%d",
                       &udp_port) != 1 ||
                udp_port < 1 ||
                udp_port > 65535)
            {
                const char *response =
                    "ERR 013 INVALID_UDP_PORT SID:" SID "\n";

                send_all(client_socket,
                         response,
                         strlen(response));

                continue;
            }

            if (monitor == NULL)
            {
                monitor =
                    malloc(sizeof(MonitorContext));

                if (monitor == NULL)
                {
                    const char *response =
                        "ERR 014 MONITOR_FAILED SID:" SID "\n";

                    send_all(client_socket,
                             response,
                             strlen(response));

                    continue;
                }

                monitor->active = 1;

                monitor->udp_port =
                    udp_port;

                monitor->controller_ip =
                    client_addr.sin_addr;

                pthread_mutex_init(&monitor->mutex,
                                   NULL);

                if (pthread_create(&monitor_thread_id,
                                   NULL,
                                   monitor_thread_function,
                                   monitor) != 0)
                {
                    pthread_mutex_destroy(&monitor->mutex);

                    free(monitor);

                    monitor = NULL;

                    const char *response =
                        "ERR 014 MONITOR_FAILED SID:" SID "\n";

                    send_all(client_socket,
                             response,
                             strlen(response));

                    continue;
                }

                const char *response =
                    "OK MONITOR_STARTED SID:" SID "\n";

                send_all(client_socket,
                         response,
                         strlen(response));

                printf("UDP monitoring started on port %d\n",
                       udp_port);
            }
            else
            {
                const char *response =
                    "OK MONITOR_STARTED SID:" SID "\n";

                send_all(client_socket,
                         response,
                         strlen(response));
            }
        }


        /*
         * =================================================
         * MONITOR STOP
         * =================================================
         */
        else if (strcmp(buffer,
                        "MONITOR STOP") == 0)
        {
            if (monitor != NULL)
            {
                pthread_mutex_lock(&monitor->mutex);

                monitor->active = 0;

                pthread_mutex_unlock(&monitor->mutex);

                pthread_join(monitor_thread_id,
                             NULL);

                pthread_mutex_destroy(&monitor->mutex);

                free(monitor);

                monitor = NULL;

                printf("UDP monitoring stopped.\n");
            }

            const char *response =
                "OK MONITOR_STOPPED SID:" SID "\n";

            send_all(client_socket,
                     response,
                     strlen(response));
        }


        /*
         * =================================================
         * QUIT
         * =================================================
         */
        else if (strcmp(buffer,
                        "QUIT") == 0)
        {
            /*
             * Stop monitoring before closing.
             */
            if (monitor != NULL)
            {
                pthread_mutex_lock(&monitor->mutex);

                monitor->active = 0;

                pthread_mutex_unlock(&monitor->mutex);

                pthread_join(monitor_thread_id,
                             NULL);

                pthread_mutex_destroy(&monitor->mutex);

                free(monitor);

                monitor = NULL;
            }

            const char *response =
                "OK BYE SID:" SID "\n";

            send_all(client_socket,
                     response,
                     strlen(response));

            printf("Client requested QUIT.\n");

            close(client_socket);

            pthread_exit(NULL);
        }


        /*
         * Unknown/unimplemented command.
         */
        else
        {
            const char *response =
                "ERR 999 COMMAND_NOT_IMPLEMENTED SID:" SID "\n";

            send_all(client_socket,
                     response,
                     strlen(response));
        }
    }


    /*
     * Client disconnected unexpectedly.
     * Stop monitoring if active.
     */
    if (monitor != NULL)
    {
        pthread_mutex_lock(&monitor->mutex);

        monitor->active = 0;

        pthread_mutex_unlock(&monitor->mutex);

        pthread_join(monitor_thread_id,
                     NULL);

        pthread_mutex_destroy(&monitor->mutex);

        free(monitor);
    }

    close(client_socket);

    pthread_exit(NULL);
}


/*
 * =========================================================
 * MAIN
 * =========================================================
 */
int main(void)
{
    signal(SIGPIPE,
           SIG_IGN);

    int server_socket;

    struct sockaddr_in server_addr;

    server_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (server_socket < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;

    if (setsockopt(server_socket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");

        close(server_socket);

        return 1;
    }

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);

    if (bind(server_socket,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");

        close(server_socket);

        return 1;
    }

    if (listen(server_socket,
               BACKLOG) < 0)
    {
        perror("listen");

        close(server_socket);

        return 1;
    }

    printf("=====================================\n");
    printf(" RemoteOps Agent\n");
    printf(" TCP Port : %d\n", PORT);
    printf(" SID      : %s\n", SID);
    printf(" UDP Monitor Interval : %d seconds\n",
           MONITOR_INTERVAL);
    printf("=====================================\n");
    printf("Agent is listening...\n");

    while (1)
    {
        struct sockaddr_in client_addr;

        socklen_t client_len =
            sizeof(client_addr);

        int *client_socket =
            malloc(sizeof(int));

        if (client_socket == NULL)
        {
            perror("malloc");
            continue;
        }

        *client_socket =
            accept(server_socket,
                   (struct sockaddr *)&client_addr,
                   &client_len);

        if (*client_socket < 0)
        {
            perror("accept");

            free(client_socket);

            continue;
        }

        pthread_t thread_id;

        if (pthread_create(&thread_id,
                           NULL,
                           handle_client,
                           client_socket) != 0)
        {
            perror("pthread_create");

            close(*client_socket);

            free(client_socket);

            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_socket);

    return 0;
}
