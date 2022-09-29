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

#include "htu21d.h"
#include "i2c.h"

// read with no hold master!
// REGISTERS/COMANDS:
#define HTU21_READ_TEMP     (0xF3)
#define HTU21_READ_HUMID    (0xF5)
#define HTU21_READ_USERREG  (0xE7)
#define HTU21_WRITE_USERREG (0xE6)
#define HTU21_SOFT_RESET    (0xFE)
// status mask & val
#define HTU21_STATUS_MASK   (0x03)
#define HTU21_HUMID_FLAG    (0x02)
// user reg fields
#define HTU21_REG_VBAT      (0x40)
#define HTU21_REG_D1        (0x80)
#define HTU21_REG_D0        (0x01)
#define HTU21_REG_HTR       (0x04)
#define HTU21_REG_ODIS      (0x02)
#define HTU21_REG_VBAT      (0x40)
#define HTU21_REG_DEFVAL    (0x02)

static HTU21D_status htustatus = HTU21D_RELAX;
static float Tmeasured, Hmeasured;
static double lastw = 0.; // last time of measurements start

HTU21D_status HTU21D_get_status(){
    return htustatus;
}

static int writecmd(uint8_t cmd){
    if(1 != write(i2c_getfd(), &cmd, 1)) return FALSE;
    return TRUE;
}

/**
 * @brief HTU21D_read_ID - read user register and compare with default
 * @return TRUE if all OK
 */
int HTU21D_read_ID(){
    if(htustatus != HTU21D_RELAX) return FALSE;
	uint8_t ID;
    if(!i2c_read_reg8(HTU21_READ_USERREG, &ID)){
        DBG("Can't read HTU_REG_ID");
        return FALSE;
    }
    DBG("HTU, reg: 0x%02x", ID);
    if(ID != HTU21_REG_DEFVAL){
        DBG("Not HTU21D or need reloading\n");
        writecmd(HTU21_SOFT_RESET);
        return FALSE;
    }
    return TRUE;
}

int HTU21D_startmeasure(){
    htustatus = HTU21D_BUSY;
    if(!writecmd(HTU21_READ_TEMP)){
        htustatus = HTU21D_ERR;
        return FALSE;
    }
    DBG("Wait for T\n");
    lastw = dtime();
    return TRUE;
}

static void HTU21D_cmdH(){
    htustatus = HTU21D_BUSY;
    if(!writecmd(HTU21_READ_HUMID)){
        htustatus = HTU21D_ERR;
        return;
    }
    DBG("Wait for H, dt=%g", dtime() - lastw);
    lastw = dtime();
}

int HTU21D_getTH(float *T, float *H){
    if(htustatus != HTU21D_RDY) return FALSE;
    if(T) *T = Tmeasured;
    if(H) *H = Hmeasured;
    htustatus = HTU21D_RELAX;
    return TRUE;
}

#define SHIFTED_DIVISOR 0x988000    //This is the 0x0131 polynomial shifted to farthest left of three bytes
// check CRC, return 0 if all OK
static uint32_t htu_check_crc(uint8_t *crc){
    DBG("HTU check CRC\n");
    uint32_t remainder = (crc[0] << 16) | (crc[1] << 8) | crc[2];
    uint32_t divsor = (uint32_t)SHIFTED_DIVISOR;
    int i;
    for(i = 0; i < 16; i++) {
        if (remainder & (uint32_t)1 << (23 - i))
            remainder ^= divsor;
        divsor >>= 1;
    }
    return remainder;
}

void HTU21D_process(){
	uint8_t d[3];
    if(htustatus != HTU21D_BUSY) return;
    if(3 != read(i2c_getfd(), d, 3)){ // NACK'ed - not ready
        if(dtime() - lastw > HTU21D_CONVTIMEOUT){
            DBG("Wait too long -> err");
            htustatus = HTU21D_ERR;
            return;
        }
        return;
    }
    DBG("Got: 0x%02x, 0x%02x, 0x%02x", d[0], d[1], d[2]);
    if(htu_check_crc(d)){
        htustatus = HTU21D_ERR;
        DBG("CRC failed\n");
        return;
    }
    uint16_t TH = (uint16_t)((d[0]<<8) | d[1]);;
    if(!(TH & HTU21_HUMID_FLAG)){ // temperature measured
        Tmeasured = -46.85 + 175.72*(TH & 0xFFFC)/65536.;
        DBG("T=%.1f", Tmeasured);
        HTU21D_cmdH();
    }else{ // humidity measured
        Hmeasured = -6.f + 125.f*(TH & 0xFFFC)/65536.;
        DBG("H=%.1f", Hmeasured);
        htustatus = HTU21D_RDY;
    }
}

int HTU21D_heater(int ON){
    if(htustatus != HTU21D_RELAX) return FALSE;
    uint8_t val;
    if(!i2c_read_reg8(HTU21_READ_USERREG, &val)){
        DBG("Can't read userreg");
        return FALSE;
    }
    DBG("REG: 0x%02x", val);
    if(ON) val |= HTU21_REG_HTR;
    else val &= ~HTU21_REG_HTR;
    DBG("REG -> 0x%02x", val);
    if(!i2c_write_reg8(HTU21_WRITE_USERREG, val)){
        DBG("Can't write write userreg");
        return FALSE;
    }
    return TRUE;
}
