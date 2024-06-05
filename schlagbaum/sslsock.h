/*
 * This file is part of the schlagbaum project.
 * Copyright 2023 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <arpa/inet.h>  // inet_ntop
#include <fcntl.h>
#include <netdb.h>      // addrinfo
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#if ! defined CLIENT && ! defined SERVER
#error "Define CLIENT or SERVER before including this file"
#endif
#if defined CLIENT && defined SERVER
#error "Both CLIENT and SERVER defined"
#endif

#define BACKLOG     10

// command protocol
// open gate
#define CMD_OPEN    "open"
// close
#define CMD_CLOSE   "close"
// siren
#define CMD_SIREN   "siren"
// LEDs
#define CMD_LED0    "led0"
#define CMD_LED1    "led1"
// connection OK - ping message
#define CMD_PING    "ping"

// server sends ping each 5s
#define PING_TIMEOUT    (5.)

typedef struct{
    uint32_t gpio;      // gpio in number
    const char *cmd;    // text command
} cmd_t;

int open_socket();
int read_string(SSL *ssl, char *buf, int l);

int handle_message(const char *msg, cmd_t *gpios);
#ifdef __arm__
void poll_gpio(SSL **ssls, int nfd, cmd_t *commands);
#endif
