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

#include <stdio.h>
#include <usefull_macros.h>
#include <unistd.h>

#include "i2c.h"
#include "si7005.h"

#define SI7005_REGSTATUS    0
#define SI7005_STATUSNRDY   1
#define SI7005_REGDATA      1
#define SI7005_REGCONFIG    3
#define SI7005_CONFFAST     (1<<5)
#define SI7005_CONFTEMP     (1<<4)
#define SI7005_CONFHEAT     (1<<1)
#define SI7005_CONFSTART    (1<<0)
#define SI7005_REGID        0x11

#define SI7005_ID       0x50

static si7005_status sistatus = SI7005_RELAX;

static float Tmeasured, Hmeasured;
static double lastw = 0.; // last time of measurements start

si7005_status si7005_get_status(){
    return sistatus;
}

/**
 * @brief si7005_read_ID - read device ID
 * @return TRUE if all OK
 */
int si7005_read_ID(){
    if(sistatus != SI7005_RELAX) return FALSE;
    //i2c_write_reg8(SI7005_REGCONFIG, 0);
	uint8_t ID;
    if(!i2c_read_reg8(SI7005_REGID, &ID)){
        DBG("Can't read SI_REG_ID");
        return FALSE;
    }
    DBG("SI, device ID: 0x%02x", ID);
    if(ID != SI7005_ID){
        DBG("Not SI7005\n");
        return FALSE;
    }
    return TRUE;
}

/*
 * start themperature reading @return TRUE if all OK
 */
int si7005_startmeasure(){
    sistatus = SI7005_BUSY;
    if(!i2c_write_reg8(SI7005_REGCONFIG, SI7005_CONFTEMP | SI7005_CONFSTART)){
        DBG("Can't write start Tmeas");
        sistatus = SI7005_ERR;
        return FALSE;
    }
    DBG("Wait for T\n");
    lastw = dtime();
    return TRUE;
}

/*
 * start humidity reading @return TRUE if all OK
 */
static void si7005_cmdH(){
    sistatus = SI7005_BUSY;
    i2c_write_reg8(SI7005_REGCONFIG, SI7005_CONFSTART);
    if(!i2c_write_reg8(SI7005_REGCONFIG, SI7005_CONFSTART)){
        DBG("Can't write start Hmeas");
        sistatus = SI7005_ERR;
        return;
    }
    DBG("Wait for H, dt=%g", dtime() - lastw);
    lastw = dtime();
}

int si7005_getTH(float *T, float *H){
    if(sistatus != SI7005_RDY) return FALSE;
    DBG("dt=%g", dtime() - lastw);
    DBG("Measured T=%.1f, H=%.1f", Tmeasured, Hmeasured);
    // correct T/H
#define A0 (-4.7844f)
#define A1 (0.4008f)
#define A2 (-0.00393f)
    if(H){
        *H = Hmeasured - (A2*Hmeasured*Hmeasured + A1*Hmeasured + A0);
    }
    if(T) *T = Tmeasured;
    sistatus = SI7005_RELAX;
    return TRUE;
}

/*
 * process state machine
 */
void si7005_process(){
	uint8_t c, d[3];
    if(sistatus != SI7005_BUSY) return;
    if(3 != read(i2c_getfd(), d, 3)){
        DBG("Can't read status");
        sistatus = SI7005_ERR;
        return;
    }
    DBG("Status: 0x%02x, H: 0x%02x, L: 0x%02x", d[0], d[1], d[2]);
    if(!i2c_read_reg8(SI7005_REGCONFIG, &c)){
        DBG("Can't read config");
        sistatus = SI7005_ERR;
        return;
    }
    DBG("Config: 0x%02x", c);
    if(d[0] & SI7005_STATUSNRDY){ // not ready yet
    //if(c & SI7005_CONFSTART){
        if(dtime() - lastw > SI7005_CONVTIMEOUT){
            DBG("Wait too long -> err");
            sistatus = SI7005_ERR;
            return;
        }
        usleep(20000);
        return;
    }
    uint16_t TH = (uint16_t)((d[1]<<8) | d[2]);
    if(c & SI7005_CONFTEMP){ // temperature measured
        TH >>= 2;
        Tmeasured = TH/32.f - 50.f;
        DBG("T=%.1f", Tmeasured);
        si7005_cmdH();
    }else{ // humidity measured
        TH >>= 4;
        Hmeasured = TH/16.f - 24.f;
        DBG("H=%.1f", Hmeasured);
        sistatus = SI7005_RDY;
    }
}

/**
 * @brief si7005_heater - turn on/off heater
 * @param ON == 1 to turn on
 * @return FALSE if failed
 */
int si7005_heater(int ON){
    if(sistatus != SI7005_RELAX) return FALSE;
    uint8_t reg = (ON) ? SI7005_CONFHEAT : 0;
    if(!i2c_write_reg8(SI7005_REGCONFIG, reg)){
        DBG("Can't write write regconfig");
        return FALSE;
    }
    return TRUE;
}
