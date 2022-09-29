/*
 * This file is part of the sihtu project.
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

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <usefull_macros.h>

#include "htu21d.h"
#include "si7005.h"
#include "i2c.h"

#define DEVADDR     (0x40)

typedef struct{
    char *device;
    int help;
    int heater;
} glob_pars;

static glob_pars G = {.device = "/dev/i2c-3", .heater = -1};

static myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    _("I2C device path")},
    {"heater",  NEED_ARG,   NULL,   'H',    arg_int,    APTR(&G.heater),    _("turn on (>0) or off (0) heater")},
   end_option
};

static void showd(float T, float H){
// Sonntag1990
#define dB  17.62f
#define dC  243.12f
    if(T < -273.15f || H < 0.f) return;
    float gamma = logf(H/100.f) + dB*T/(dC + T);
    float Tdp = dC * gamma / (dB - gamma);
    printf("T=%.1fC, H=%.1f%%, Tdp=%.1fC\n", T, H, Tdp);
    static int i = 0;
    if(++i > 3) signals(0);
}

static void processSI(){
    static double t0 = 0.;
    si7005_process();
    si7005_status s = si7005_get_status();
    if(s == SI7005_RELAX){
        if(dtime() - t0 > 1.){
            DBG("need to start measure");
            if(si7005_startmeasure()) t0 = dtime();
        }
    }else if(s == SI7005_RDY){ // humidity can be shown
        DBG("Got data");
        float T, H;
        if(!si7005_getTH(&T, &H)) return;
        showd(T, H);
        t0 = dtime();
    }else if(s == SI7005_ERR){
        DBG("got error");
        if(si7005_startmeasure()) t0 = dtime();
    }
}
static void processHTU(){
    static double t0 = 0.;
    HTU21D_process();
    HTU21D_status s = HTU21D_get_status();
    if(s == HTU21D_RELAX){
        if(dtime() - t0 > 1.){
            DBG("need to start measure");
            if(HTU21D_startmeasure()) t0 = dtime();
        }
    }else if(s == HTU21D_RDY){ // humidity can be shown
        DBG("Got data");
        float T, H;
        if(!HTU21D_getTH(&T, &H)) return;
        showd(T, H);
        t0 = dtime();
    }else if(s == HTU21D_ERR){
        DBG("got error");
        if(HTU21D_startmeasure()) t0 = dtime();
    }
}

int main(int argc, char **argv){
    initial_setup();
    parseargs(&argc, &argv, cmdlnopts);
    if(G.help) showhelp(-1, cmdlnopts);
    if(!i2c_open(G.device)) ERR("Can't open %s", G.device);
    if(!i2c_set_slave_address((uint8_t)DEVADDR)){
        WARN("Can't set slave address 0x%02x", DEVADDR);
        goto clo;
    }
    //if(!i2c_read_reg8(0, NULL)) ERR("Can't connect!");
    int si = 1; // == 0 if htu
    if(!si7005_read_ID()){
        DBG("Don't see SI7005");
        si = 0;
        if(!HTU21D_read_ID()){
            ERRX("Neither SI7005 nor HTU21D not found");
        }else while(!HTU21D_startmeasure()) usleep(1000);
    }else while(!si7005_startmeasure()) usleep(1000);
    if(G.heater > -1){
        int ans = FALSE;
        if(si) ans = si7005_heater(G.heater);
        else ans = HTU21D_heater(G.heater);
        if(!ans) WARNX("Can't turn on heater");
    }
    while(1){
        if(si) processSI();
        else processHTU();
    }
clo:
    i2c_close();
    return 0;
}
