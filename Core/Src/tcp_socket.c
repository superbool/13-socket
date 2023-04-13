/**
  ******************************************************************************
  * @file    LwIP/LwIP_HTTP_Server_Socket_RTOS/Src/httpserver-socket.c
  * @author  MCD Application Team
  * @brief   Basic http server implementation using LwIP socket API
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes  -----------------------------------------------------------------*/
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/apps/fs.h"
#include "string.h"
#include "tcp_socket.h"
#include "cmsis_os.h"

#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define WEBSERVER_THREAD_PRIO    ( osPriorityAboveNormal )

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
u32_t nPageHits = 0;
portCHAR PAGE_BODY[512];

extern int connected;


/**
  * @brief serve tcp connection
  * @param conn: connection socket
  * @retval None
  */
void http_server_serve(int conn) {
    int buflen = 128;
    int ret;

    unsigned char recv_buffer[128];

    /* Read in the request */
    ret = read(conn, recv_buffer, buflen);
    if (ret < 0) return;

    write(conn, recv_buffer, buflen);
    /* Close connection socket */
    close(conn);
}

/**
  * @brief  http server thread
  * @param arg: pointer on argument(not used here)
  * @retval None
  */
static void http_server_socket_thread(void *argument) {
    int sock, newconn, size;
    struct sockaddr_in address, remotehost;
    /* bind to port 80 at any interface */
    address.sin_family = AF_INET;
    address.sin_port = htons(80);
    address.sin_addr.s_addr = INADDR_ANY;

    /* create a TCP socket */
    do {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            printf("Socket error\n");
        } else {
            if (bind(sock, (struct sockaddr *) &address, sizeof(address)) < 0) {
                printf("Unable to bind\n");
            }
            /* listen for incoming connections (TCP listen backlog = 5) */
            if (listen(sock, 5) < 0) {
                printf("Listen error\n");
            }
        }
        osDelay(100);
    } while (sock < 0);

    printf("Socket bind to 80 success\n");
    size = sizeof(remotehost);

    for (;;) {
        newconn = accept(sock, (struct sockaddr *) &remotehost, (socklen_t *) &size);
        printf("my new client connected from (%s, %d)\n", inet_ntoa(remotehost.sin_addr), ntohs(remotehost.sin_port));
        http_server_serve(newconn);
        osDelay(1);
    }
}


#define PORT              80
#define RECV_DATA         (128)


static void tcpecho_thread(void *arg) {
    int sock = -1;
    char *recv_data;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_size;
    int recv_data_len;

    recv_data = (char *) pvPortMalloc(RECV_DATA);
    if (recv_data == NULL) {
        printf("No memory\n");
    }

    do {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            printf("Socket error\n");
        } else {
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(PORT);
            memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
            if (bind(sock, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1) {
                printf("Unable to bind\n");
            }
            if (listen(sock, 5) == -1) {
                printf("Listen error\n");
            }
        }
        HAL_Delay(100);
    } while (sock < 0);
    sin_size = sizeof(client_addr);
    while (1) {
        connected = accept(sock, (struct sockaddr *) &client_addr, &sin_size);
        printf("new client connected from (%s, %d) %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
               connected);
        {
            int flag = 1;
            setsockopt(connected,
                       IPPROTO_TCP,     /* set option at TCP level */
                       TCP_NODELAY,     /* name of option */
                       (void *) &flag, /* the cast is historical cruft */
                       sizeof(int));    /* length of option value */
        }

        while (1) {
            recv_data_len = recv(connected, recv_data, RECV_DATA, 0);
            printf("recv %d len data\n", recv_data_len);
            if (recv_data_len <= 0)
                break;

            write(connected, recv_data, recv_data_len);
        }

        if (connected >= 0)
            closesocket(connected);
        printf("closesocket %d\n", connected);
        connected = -1;
    }
    __exit:
    if (sock >= 0) closesocket(sock);
    if (recv_data) free(recv_data);
}

void tcpecho_init(void) {
    sys_thread_new("tcpecho_thread", tcpecho_thread, NULL, 2048, osPriorityNormal);
}
