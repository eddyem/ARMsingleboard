/*
 * This file is part of the bmp280 project.
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

#include "BMP280.h"
#include "i2c.h"

typedef struct{
    char *device;
    int slaveaddr;
    int help;
} glob_pars;

static glob_pars G = {.device = "/dev/i2c-3", .slaveaddr = BMP280_I2C_ADDRESS};

static myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    _("I2C device path")},
    {"slave",   NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.slaveaddr), _("I2C slave address (0x76 or 0x77)")},
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
    while(!BMP280_init()) sleep(1);
    uint8_t devid;
    BMP280_read_ID(&devid);
    DBG("ID: 0x%02x", devid);
    while(!BMP280_start()){
        DBG("Trying to start");
        sleep(1);
    }
    while (1){
        BMP280_process();
        BMP280_status s = BMP280_get_status();
        if(s == BMP280_RDY){ // data ready - get it
            float T, P, H;
            int ntries = 0;
            for(; ntries < 3; ++ntries) if(BMP280_getdata(&T, &P, &H)) break;
            if(ntries == 3){
                WARNX("Can't read data");
                continue;
            }
            float mm = P * 0.00750062f;
            printf("T=%.1f, P=%.1fPa (%.1fmmHg)", T, P, mm);
            if(devid == BME280_CHIP_ID){ // got humidity too
                printf(", H=%.1f%%", H);
            }
            printf("\n");
            sleep(5);
            while(!BMP280_start()) usleep(1000);
        }else if(s == BMP280_ERR){
            printf("Error in measurement\n");
            BMP280_reset();
            BMP280_init();
        }
    }

clo:
    i2c_close();
    return 0;
}
