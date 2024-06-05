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

#include <usefull_macros.h>
#include <string.h>

#include "client.h"
#include "cmdlnopts.h"
#include "sslsock.h"
#ifdef __arm__
#include "gpio.h"

// GPIO25 - LEDopen, 8 - LEDclose
static cmd_t client_in_gpios[] = {
    {18, CMD_OPEN},
    {23, CMD_CLOSE},
    {24, CMD_SIREN},
    {0, NULL}
};

#endif

// LEDs
static cmd_t client_out_gpios[] = {
    {10, CMD_LED0},
    {9, CMD_PING},
    {11, CMD_LED1},
    {0, NULL}
};

static int SSL_nbread(SSL *ssl, char *buf, int bufsz){
    struct pollfd fds = {0};
    int fd = SSL_get_fd(ssl);
    fds.fd = fd;
    fds.events = POLLIN | POLLPRI;
    if(poll(&fds, 1, 1) < 0){ // wait no more than 1ms
        LOGWARN("SSL_nbread(): poll() failed");
        WARNX("poll()");
        return 0;
    }
    if(fds.revents & (POLLIN | POLLPRI)){
        //DBG("Got info in fd #%d", fd);
        int l = read_string(ssl, buf, bufsz);
        //DBG("read %d bytes", l);
        return l;
    }
    return 0;
}

static void readssl(SSL *ssl){
    char buf[BUFSIZ];
    int bytes = SSL_nbread(ssl, buf, BUFSIZ-1);
    //int bytes = read_string(ssl, buf, BUFSIZ-1);
    if(bytes > 0){
        buf[bytes] = 0;
        verbose(1, "Received: \"%s\"", buf);
        handle_message(buf, client_out_gpios);
    }else if(bytes < 0){
        LOGWARN("Server disconnected or other error");
        ERRX("Disconnected");
    }
}

static void sendcommands(SSL *ssl){
    char buf[BUFSIZ];
    char **curdata = G.commands;
    if(!curdata) return;
    while(*curdata){
        verbose(1, "Send: \"%s\"", *curdata);
        int l = snprintf(buf, BUFSIZ-1, "%s\n", *curdata);
        if(SSL_write(ssl, buf, l) <= 0) WARNX("SSL write error");
        readssl(ssl);
        ++curdata;
    }
    double t0 = dtime();
    while(dtime() - t0 < 2.) readssl(ssl);
}

void clientproc(SSL_CTX *ctx, int fd){
    FNAME();
    SSL *ssl;
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    int c = SSL_connect(ssl);
    if(c < 0){
        LOGERR("SSL_connect()");
        ERRX("SSL_connect() error: %d", SSL_get_error(ssl, c));
    }
    int enable = 1;
    if(ioctl(fd, FIONBIO, (void *)&enable) < 0){
        LOGERR("Can't make socket nonblocking");
        ERRX("ioctl()");
    }
    if(G.commands){
        sendcommands(ssl);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        return;
    }
    while(1){
#ifdef __arm__
        poll_gpio(&ssl, 1, client_in_gpios);       
#endif
        readssl(ssl);
    }
    SSL_free(ssl);
}
