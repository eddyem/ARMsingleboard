/*
 * This file is part of the lightning project.
 * Copyright 2024 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include "as3935.h"
#include "i2c.h"
#include <stdio.h>
#include <usefull_macros.h>

typedef struct{
    char *device;
    int slaveaddr;
    int verbose;
    int help;
    char *logfile;
} glob_pars;

static glob_pars G = {.device = "/dev/i2c-3", .slaveaddr = 0};

static myoption cmdlnopts[] = {
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    "I2C device path"},
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"slave",   NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.slaveaddr), "I2C slave address"},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   "Verbose (each -v increase)"},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   "file for logging"},
   end_option
};

int main(int argc, char **argv){
    initial_setup();
    parseargs(&argc, &argv, cmdlnopts);
    if(G.help) showhelp(-1, cmdlnopts);
    if(G.slaveaddr < 0 || G.slaveaddr > 0x7f) ERRX("I2C address should be 7-bit");
    if(!as3935_open(G.device, G.slaveaddr)) ERR("Can't open %s", G.device);
    if(G.logfile){
        sl_loglevel l = LOGLEVEL_ERR;
        for(int i = 0; i < G.verbose; ++i){
            if(++l == LOGLEVEL_ANY) break;
        }
        OPENLOG(G.logfile, l, 1);
        if(!globlog) ERRX("Can't open logfile %s", G.logfile);
    }
    LOGMSG("Connected to slave 0x%02x\n", G.slaveaddr);
    ;
    return 0;
}

