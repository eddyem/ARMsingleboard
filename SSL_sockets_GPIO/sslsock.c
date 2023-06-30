/*
 * This file is part of the sslsosk project.
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

#include <pthread.h>
#include <resolv.h>
#include <signal.h>     // pthread_kill
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "sslsock.h"
#ifdef SERVER
#include "server.h"
#else
#include "client.h"
#endif
#ifdef __arm__
#include "gpio.h"
#endif

#ifdef SERVER
static int OpenConn(int port){
    int sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd < 0){
        LOGERR("Can't open socket");
        ERRX("socket()");
    }
    int enable = 1;
    // allow reuse of descriptor
    if(setsockopt(sd, SOL_SOCKET,  SO_REUSEADDR, (void *)&enable, sizeof(int)) < 0){
        LOGERR("Can't apply SO_REUSEADDR to socket");
        ERRX("setsockopt()");
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(sd, (struct sockaddr*)&addr, sizeof(addr))){
        LOGWARN("Can't bind port %d", port);
        ERRX("bind()");
    }
    if(listen(sd, BACKLOG)){
        LOGWARN("Can't listen()");
        ERRX("listen()");
    }
    return sd;
}
#else
static int OpenConn(int port){
    FNAME();
    int sd;
    struct hostent *host;
    struct sockaddr_in addr;
    if((host = gethostbyname(G.serverhost)) == NULL ){
        LOGWARN("gethostbyname(%s) error", G.serverhost);
        ERRX("gethostbyname()");
    }
    sd = socket(PF_INET, SOCK_STREAM, 0);
    DBG("sd=%d", sd);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(long*)(host->h_addr);
    if(connect(sd, (struct sockaddr*)&addr, sizeof(addr))){
        close(sd);
        LOGWARN("Can't connect to %s", G.serverhost);
        ERRX("Can't connect to %s", G.serverhost);
    }
    return sd;
}
#endif

static SSL_CTX* InitCTX(void){
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method =
#ifdef CLIENT
      TLS_client_method();
#else
      TLS_server_method();
#endif
    ctx = SSL_CTX_new(method);
    if(!ctx){
        LOGWARN("Can't create SSL context");
        ERRX("SSL_CTX_new()");
    }
    if(SSL_CTX_load_verify_locations(ctx, G.ca, NULL) != 1){
        LOGWARN("Could not set the CA file location\n");
        ERRX("Could not set the CA file location\n");
    }
#ifdef SERVER

    SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(G.ca));
#endif
    if(SSL_CTX_use_certificate_file(ctx, G.cert, SSL_FILETYPE_PEM) <= 0){
        LOGWARN("Can't use SSL certificate %s", G.cert);
        ERRX("Can't use SSL certificate %s", G.cert);
    }
    if(SSL_CTX_use_PrivateKey_file(ctx, G.key, SSL_FILETYPE_PEM) <= 0){
        LOGWARN("Can't use SSL key %s", G.key);
        ERRX("Can't use SSL key %s", G.key);
    }
    if(!SSL_CTX_check_private_key(ctx)){
        LOGWARN("Private key does not match the public certificate\n");
        ERRX("Private key does not match the public certificate\n");
    }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
#ifdef SERVER
    SSL_CTX_set_verify(ctx, // Specify that we need to verify the client as well
       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
       NULL);
#else
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
#endif
    SSL_CTX_set_verify_depth(ctx, 1); // We accept only certificates signed only by the CA himself
    return ctx;
}

int open_socket(){
    int fd;
#ifdef __arm__
#ifndef SERVER
    if(!G.commands){ // open devices if not client
#endif
        if(-1 == gpio_open_device(G.gpiodevpath)) ERRX("Can't open GPIO device");
        if(-1 == gpio_setup_outputs() || -1 == gpio_setup_inputs()) ERRX("Can't setup GPIO");
#ifndef SERVER
    }
#endif
#endif
    SSL_library_init();
    SSL_CTX *ctx = InitCTX();
    fd = OpenConn(atoi(G.port));
#ifdef SERVER
    serverproc(ctx, fd);
#else
    clientproc(ctx, fd);
#endif
    // newer reached
#ifdef __arm__
    gpio_close();
#endif
    close(fd);
    SSL_CTX_free(ctx);
    return 0;
}

static int geterrcode(SSL *ssl, int errcode){
    int sslerr = SSL_get_error(ssl, errcode);
    if(SSL_ERROR_WANT_READ == sslerr ||
        SSL_ERROR_WANT_WRITE == sslerr) return 0; // empty call
    int sd = SSL_get_fd(ssl);
    if(sslerr != SSL_ERROR_ZERO_RETURN){
        LOGERR("SSL error %d @client %d", sslerr, sd);
        WARNX("SSL error %d @client %d", sslerr, sd);
    }
    return -1;
}

/**
 * @brief read_string - read '\n'-terminated string from SSL
 * @param ssl - SSL
 * @param buf - buffer for text
 * @param l - max buf length (including zero)
 * @return amount of bytes read or -1 if client disconnected
 */
int read_string(SSL *ssl, char *buf, int l){
    if(!ssl || l < 1) return 0;
    bzero(buf, l);
    int bytes = SSL_peek(ssl, buf, l);
    DBG("Peek: %d (bufsz %d); buf=%s", bytes, l, buf);
    if(bytes < 1){ // nothing to read or error
        return geterrcode(ssl, bytes);
    }
    if(bytes < l && buf[bytes-1] != '\n'){ // string not ready, no buffer overfull
        return -1; // wait a rest of string
    }
    bytes = SSL_read(ssl, buf, l);
    DBG("Read: %d", bytes);
    if(bytes < 1){ // error
        return geterrcode(ssl, bytes);
    }
    buf[bytes-1] = 0; // replace '\n' with 0
    return bytes;
}

#ifdef __arm__
/**
 * @brief getpin - get pin number from string
 * @param str - received command
 * @param idx - index of number in `str`
 * @return number or -1 if not found
 */
static int getpin(const char *str, int idx){
    char *eptr = NULL;
    const char *start = str + idx;
    long x = strtol(start, &eptr, 10);
    if(eptr == start) return -1;
    if(x < 0 || x > GPIO_MAX_NUMBER) return -1;
    return (int)x;
}

/**
 * @brief handle_message - parser or client/server messages
 * @param msg - string command
 */
int handle_message(const char *msg){
    int act = -1, pin = -1, ret = FALSE;
    if(strncmp(msg, "UP", 2) == 0){
        act = 1; pin = getpin(msg, 2);
    }else if(strncmp(msg, "DOWN", 4) == 0){
        act = 0; pin = getpin(msg, 4);
    }
    DBG("message: '%s', act=%d, pin=%d", msg, act, pin);
    if(act != -1 && pin != -1){
        int res = FALSE;
        if(act == 1) res = gpio_set_output(pin);
        else res = gpio_clear_output(pin);
        if(!res) LOGERR("Can't change state according to pin %d", pin);
        else{
            LOGMSG("%s gpio %d", act == 1 ? "Set" : "Reset", pin);
            verbose(1, "%s gpio %d", act == 1 ? "Set" : "Reset", pin);
            ret = TRUE;
        }
    }
    return ret;
}
/**
 * @brief poll_gpio - GPIO polling
 * @param ssl - ssl array to write
 * @param nfd - amount of descriptors (+1 - starting frol ssls[1])
 */
void poll_gpio(SSL **ssls, int nfd){
    static double t0 = 0.;
    if(dtime() - t0 < GPIO_POLL_INTERVAL) return;
    char buf[64];
    uint32_t up, down;
    if(gpio_poll(&up, &down) > 0){
        if(up) snprintf(buf, 63, "UP%" PRIu32 "\n", up);
        else snprintf(buf, 63, "DOWN%" PRIu32 "\n", down);
        int l = strlen(buf);
        if(nfd == 1){
            if(SSL_write(ssls[0], buf, l) <= 0) WARNX("SSL write error");
        }else{
            for(int i = nfd-1; i > 0; --i){
                if(SSL_write(ssls[i], buf, l) <= 0){
                    WARNX("SSL write error");
                }
            }
        }
    }
    t0 = dtime();
}
#endif
