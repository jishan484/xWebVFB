#ifndef WEBSOCKET_C_H
#define WEBSOCKET_C_H

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include "vnc_libs/sha1.h"
#include "vnc_libs/htmlPage.h"
#include "vnc_libs/logo.h"

#define MAX_CLIENTS 25
#define PORT 8080

typedef struct Websocket {
    int server_fd;
    int client_socket[MAX_CLIENTS];
    int ws_client_socket[MAX_CLIENTS];
    int clients;
    int socketPort;
    bool stop;
    int ready;

    // Callbacks
    void (*callBack)(int sid);
    void (*callBackMsg)(char *data, int sid);
} Websocket;

// Function declarations
void ws_init(Websocket *ws);
void ws_onMessage(Websocket *ws, void (*ptr)(char *data, int sid));
void ws_begin(Websocket *ws, int port);
void ws_connections(Websocket *ws);
void ws_sendText(Websocket *ws, char *text, int sid);
void ws_sendFrame(Websocket *ws, char *img, long size, int sid);
void ws_sendRaw(Websocket *ws, int startByte, char *data, long size, int sid);
void ws_p_sendRaw(Websocket *ws, int startByte, char *data1, char *data2, long data1Size, long data2Size, int sid);
void ws_decode(unsigned char *src, char *dest);
void handshake(Websocket *ws, unsigned char *data, int sd, int sid);


void ws_init(Websocket *ws) {
    ws->server_fd = 0;
    ws->clients = 0;
    ws->socketPort = PORT;
    ws->stop = false;
    ws->ready = 0;
    ws->callBack = NULL;
    ws->callBackMsg = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ws->client_socket[i] = 0;
        ws->ws_client_socket[i] = 0;
    }
}

void ws_onMessage(Websocket *ws, void (*ptr)(char *data, int sid)) {
    ws->callBackMsg = ptr;
}

void ws_begin(Websocket *ws, int port) {
    ws->socketPort = port;
}

void ws_decode(unsigned char *data, char *result) {
    if (data[0] == 0) return;
    int size = data[1] & 127;
    int index = 2;
    for (int i = 6; i < size + 6; i++) {
        result[i - 6] = data[i] ^ data[index++];
        if (index == 6) index = 2;
    }
}

void ws_sendRaw(Websocket *ws, int startByte, char *data, long size, int sid) {
    char header[11];
    int moded = 0;
    header[0] = startByte;

    if (size <= 125) {
        header[1] = size;
        moded = 2;
    } else if (size <= 65535) {
        header[1] = 126;
        header[2] = ((size >> 8) & 255);
        header[3] = (size & 255);
        moded = 4;
    } else {
        header[1] = 127;
        header[2] = ((size >> 56) & 255);
        header[3] = ((size >> 48) & 255);
        header[4] = ((size >> 40) & 255);
        header[5] = ((size >> 32) & 255);
        header[6] = ((size >> 24) & 255);
        header[7] = ((size >> 16) & 255);
        header[8] = ((size >> 8) & 255);
        header[9] = (size & 255);
        moded = 10;
    }

    if (sid != -1) {
        send(ws->client_socket[sid], header, moded, 0);
        send(ws->client_socket[sid], data, size, 0);
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ws->client_socket[i] == 0 || ws->ws_client_socket[i] == 0) continue;
        send(ws->client_socket[i], header, moded, 0);
        send(ws->client_socket[i], data, size, 0);
    }
}

void ws_p_sendRaw(Websocket *ws, int startByte, char *data1, char *data2, long data1Size, long data2Size, int sid) 
{
    int moded = 0;
    long imgSize = data1Size + data2Size;
    char header[11];
    header[0] = startByte;
    if (imgSize <= 125)
    {
        header[1] = imgSize;
        moded = 2;
    }
    else if (imgSize >= 126 && imgSize <= 65535)
    {
        header[1] = 126;
        header[2] = ((imgSize >> 8) & 255);
        header[3] = (imgSize & 255);
        moded = 4;
    }
    else
    {
        header[1] = 127;
        header[2] = ((imgSize >> 56) & 255);
        header[3] = ((imgSize >> 48) & 255);
        header[4] = ((imgSize >> 40) & 255);
        header[5] = ((imgSize >> 32) & 255);
        header[6] = ((imgSize >> 24) & 255);
        header[7] = ((imgSize >> 16) & 255);
        header[8] = ((imgSize >> 8) & 255);
        header[9] = (imgSize & 255);
        moded = 10;
    }
    if (sid != -1)
    {
        send(ws->client_socket[sid], header, moded, 0);    // for websocket header
        send(ws->client_socket[sid], data1, data1Size, 0); // for websocket data 1
        send(ws->client_socket[sid], data2, data2Size, 0); // for websocket data 2
        return;
    }
    for (int i = 0; i < 30; i++)
    {
        if (ws->client_socket[i] == 0 || ws->ws_client_socket[i] == 0) continue;
        send(ws->client_socket[i], header, moded, 0); // for websocket header
        send(ws->client_socket[i], data1, data1Size, 0); // for websocket data 1
        send(ws->client_socket[i], data2, data2Size, 0); // for websocket data 2
    }
}


void ws_sendText(Websocket *ws, char *text, int sid) {
    ws_sendRaw(ws, 129, text, strlen(text), sid);
}

void ws_sendFrame(Websocket *ws, char *img, long size, int sid) {
    ws_sendRaw(ws, 130, img, size, sid);
}

int strcomncase_(char *text, const char *pattern, int start, int end);
int strcomncase_(char *text, const char *pattern, int start, int end) {
    int j = 0, k = 32;
    for (int i = start; i < end; i++, j++) {
        if (text[i] != pattern[j] && text[i] + k != pattern[j])
            return 0;
    }
    return 1;
}


void handshake(Websocket *ws, unsigned char *data, int sd, int sid)
{
    bool flag = false;

    // Make a writable copy of the request line(s) — strtok will modify it.
    char reqcopy[1024];
    strncpy(reqcopy, (const char *)data, sizeof(reqcopy)-1);
    reqcopy[sizeof(reqcopy)-1] = '\0';

    char *ptr = strtok(reqcopy, "\n");
    char key[1024];
    key[0] = '\0';

    while (ptr != NULL)
    {
        size_t plen = strlen(ptr);
        if (plen > 0 && ptr[plen-1] == '\r') ptr[plen-1] = '\0';
        if (strcomncase_(ptr, "sec-websocket-key:", 0, 18) == 1)
        {
            const char *p = ptr + 18;
            while (*p == ' ' || *p == '\t') p++;
            strncpy(key, p, sizeof(key)-1);
            key[sizeof(key)-1] = '\0';
            char *hash = generate_websocket_accept(key); // returns malloc'd string
            if (!hash) {
                send(sd, "HTTP/1.1 500 Internal Server Error\r\n\r\n", 37, 0);
                close(sd);
                ws->client_socket[sid] = 0;
                return;
            }
            char response[1024];
            int n = snprintf(response, sizeof(response),
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Server: PIwebVNC (by Jishan)\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: %s\r\n"
                "\r\n",
                hash);
            if (n > 0 && n < (int)sizeof(response)) {
                send(sd, response, (size_t)n, 0);
            } else {
                send(sd, response, strlen(response), 0);
            }

            free(hash);

            ws->ws_client_socket[sid] = 1;
            ws->ready = 1;
            flag = true;
            break;
        }

        ptr = strtok(NULL, "\n");
    }

    if (!flag)
    {
        char header[256];  // plenty of space for HTTP headers

        if (strcomncase_((char*) data, "GET /favicon.ico", 0, 16) ||
            strcomncase_((char*) data, "GET /logo.png", 0, 13)) {
            int hlen = sprintf(header,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/png\r\n"
                "Content-Length: 644715\r\n"
                "Cache-Control: max-age=14400\r\n"   // same as CF uses
                "Connection: close\r\n"
                "Server: PIwebVNC\r\n"
                "\r\n");
            send(ws->client_socket[sid], header, hlen, 0);
            send(ws->client_socket[sid], vnclogo_PNG, vnclogo_PNG_len, 0);
        } else {
            int hlen = sprintf(header,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Server: PIwebVNC (by Jishan)\r\n"
                "\r\n");
            send(ws->client_socket[sid], header, hlen, 0);
            send(ws->client_socket[sid], index_html, index_html_len, 0);
        }
        close(sd);
        ws->client_socket[sid] = 0;
        ws->ws_client_socket[sid] = 0;
        if (ws->clients > 0) ws->clients--;
    }
    else {
        if (ws->callBack != NULL)
        {
            (*ws->callBack)(sid);
        }
    }
}


void ws_connections(Websocket *ws) {
     int new_socket, valread = 0;
    struct sockaddr_in address;
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    int opt = 1, max_sd = 0;
    int addrlen = sizeof(address);
    unsigned char buffer[1024] = {0};
    fd_set readfds;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(ws->socketPort);

    if ((ws->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(ws->server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // timeout
    if (setsockopt(ws->server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout) < 0)
        perror("setsockopt failed\n");
    if (setsockopt(ws->server_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,sizeof timeout) < 0)
        perror("setsockopt failed\n");
    //
    if (bind(ws->server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(ws->server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("[WsocVNC] > LOG: Waiting For Client Connections : Port %d Opened (HTTP/WS)\n",ws->socketPort);
    while (!ws->stop)
    {
        FD_ZERO(&readfds);
        FD_SET(ws->server_fd, &readfds);
        max_sd = ws->server_fd;
        for (int i = 0; i < 10; i++)
        {
            int sd = ws->client_socket[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR))
        {
            perror("bind failed");
        }
        if (FD_ISSET(ws->server_fd, &readfds))
        {
            if ((new_socket = accept(ws->server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            ws->ready = 1;
            bool isAccespted = false;
            printf("[WsocVNC] > LOG: New Connection : Client_No [%d] -> socket_fd=%d, ipAddr=%s, Port=%d\n", ws->clients, new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            for (int i = 0; i < 10; i++)
            {
                if (ws->client_socket[i] == 0 && ws->clients < 10)
                {
                    ws->client_socket[i] = new_socket;
                    ws->clients++;
                    isAccespted = true;
                    break;
                }
            }
            if (!isAccespted)
            {
                //reject connection
                send(new_socket, "HTTP/1.1 400 Bad Request\r\n\r\n", 25, 0);
                close(new_socket);
                printf("[WsocVNC] > ERR: Max connections reached\n");
            }
        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                if (ws->client_socket[i] == 0)
                    continue;
                int sd = ws->client_socket[i];
                if (FD_ISSET(sd, &readfds))
                {
                    if ((valread = read(sd, buffer, 1024)) == 0)
                    {
                        getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                        printf("[WsocVNC] > LOG: Client [%d] Disconnected : ip=%s, port=%d\n", ws->clients, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                        close(sd);
                        ws->client_socket[i] = 0;
                        ws->ws_client_socket[i] = 0;
                        ws->clients--;
                    }
                    else
                    {
                        buffer[valread] = '\0';
                        if (buffer[0] == 'G' && buffer[1] == 'E' && buffer[2] == 'T')
                        {
                            handshake(ws, buffer, sd, i);
                        }
                        else if ((int)buffer[0] == 136)
                        {
                            close(sd);
                            ws->client_socket[i] = 0;
                            ws->ws_client_socket[i] = 0;
                            ws->clients--;
                            printf("[WsocVNC] > LOG: Host disconnected : %d | current active clients %d\n", sd,ws->clients);
                        }
                        else
                        {
                            if (ws->callBackMsg != NULL)
                            {
                                char inputData[200]={0};
                                ws_decode(buffer , inputData);
                                ws->callBackMsg(inputData, i);
                            }
                        }
                    }
                }
            }
        }
    }
    printf("[WsocVNC] > LOG: WebSocket closed successfully\n");
}

#endif
