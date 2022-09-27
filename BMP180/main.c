/*
 * This file is part of the bmp180 project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <stdio.h>
#include <unistd.h>
#include <usefull_macros.h>

#include "BMP180.h"
#include "i2c.h"

typedef struct{
    char *device;
    int slaveaddr;
    int help;
} glob_pars;

static glob_pars G = {.device = "/dev/i2c-3", .slaveaddr = BMP180_I2C_ADDRESS};

static myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    _("I2C device path")},
    {"slave",   NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.slaveaddr), _("I2C slave address")},
   end_option
};


int main(int argc, char **argv){
    initial_setup();
    parseargs(&argc, &argv, cmdlnopts);
    if(G.help) showhelp(-1, cmdlnopts);
    if(G.slaveaddr < 0 || G.slaveaddr > 0x7f) ERRX("I2C address should be 7-bit");
    if(!i2c_open(G.device)) ERR("Can't open %s", G.device);
    if(!i2c_set_slave_address((uint8_t)G.slaveaddr)){
        WARN("Can't set slave address 0x%02x", G.slaveaddr);
        goto clo;
    }
    if(!i2c_read_reg8(0, NULL)) ERR("Can't connect!");
    while(!BMP180_init()) sleep(1);
    while(!BMP180_start()) sleep(1);
    while (1){
        BMP180_process();
        BMP180_status s = BMP180_get_status();
        if(s == BMP180_RDY){ // data ready - get it
            float T;
            uint32_t P;
            BMP180_getdata(&T, &P);
            double mm = P * 0.00750062;
            printf("T=%.1f, P=%dPa (%.1fmmHg)\n", T, P, mm);
            sleep(5);
            while(!BMP180_start()) usleep(1000);
        }else if(s == BMP180_ERR){
            printf("Error in measurement\n");
            BMP180_reset();
            BMP180_init();
        }
    }

clo:
    i2c_close();
    return 0;
}
