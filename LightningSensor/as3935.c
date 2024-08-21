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

#include <stdio.h>
#include <usefull_macros.h>

#include "as3935.h"
#include "i2c.h"

static int dev_fd = -1;

#define I2Cread(reg, val)   i2c_read_reg(dev_fd, reg, (uint8_t*)val)
#define I2Cwrite(reg, val)  i2c_write_reg(dev_fd, reg, (uint8_t)val)

// open device and set slave address
int as3935_open(const char *path, uint8_t id){
    dev_fd = i2c_open(path);
    if(dev_fd < 0) return FALSE;
    if(!i2c_set_slave_address(dev_fd, id)){
        WARNX("Can't set slave address 0x%02x", id);
        return FALSE;
    }
    return TRUE;
}

// common getter
int as3935_getter(uint8_t reg, uint8_t *data){
    return I2Cread(reg, data);
}

// and common setter
int as3935_setter(uint8_t reg, uint8_t data){
    return I2Cwrite(reg, data);
}

// display ont IRQ: nothing (0), TRCO (1), SRCO (2) or LCO (3)
int as3935_displco(uint8_t n){
    if(n > 3) return FALSE;
    t_tun_disp t;
    if(!I2Cread(TUN_DISP, &t)) return FALSE;
    t.DISP_LCO = t.DISP_SRCO = t.DISP_TRCO = 0;
    switch(n){
        case 1:
            t.DISP_TRCO = 1;
        break;
        case 2:
            t.DISP_SRCO = 1;
        break;
        case 3:
            t.DISP_LCO = 1;
        break;
        default:
        break;
    }
    return I2Cwrite(TUN_DISP, t.u8);
}

// tune LCO: change capasitor value
int as3935_tuncap(uint8_t n){
    if(n > 0xf) return FALSE;
    t_tun_disp t;
    if(!I2Cread(TUN_DISP, &t)) return FALSE;
    t.TUN_CAP = n;
    return I2Cwrite(TUN_DISP, t.u8);
}

// set gain
int as3935_gain(uint8_t n){
    if(n > 0x1f) return FALSE;
    t_afe_gain g;
    if(!I2Cread(AFE_GAIN, &g)) return FALSE;
    g.AFE_GB = n;
    return I2Cwrite(AFE_GAIN, g.u8);
}

// starting calibration
int as3935_calib_rco(){
    t_tun_disp t;
    if(!I2Cread(TUN_DISP, &t)) return FALSE;
    if(!I2Cwrite(CALIB_RCO, DIRECT_COMMAND)) return FALSE;
    t.DISP_LCO = t.DISP_TRCO = 0;
    t.DISP_SRCO = 1;
    if(!I2Cwrite(TUN_DISP, t.u8)) return FALSE;
    double t0 = dtime();
    while(dtime() - t0 < 0.005);
    t.DISP_SRCO = 0;
    if(!I2Cwrite(TUN_DISP, t.u8)) return FALSE;
    t_calib srco, trco;
    if(!I2Cread(CALIB_TRCO, &trco)) return FALSE;
    if(!I2Cread(CALIB_SRCO, &srco)) return FALSE;
    DBG("srco=%d, trco=%d", srco.u8, trco.u8);
    if(!srco.CALIB_DONE || !trco.CALIB_DONE) return FALSE;
    return TRUE;
}

// wakeup - call this function after first run
int as3935_wakeup(){
    t_afe_gain g;
    if(!I2Cread(AFE_GAIN, &g)) return FALSE;
    g.PWD = 0;
    if(!I2Cwrite(AFE_GAIN, g.u8)) return FALSE;
    return as3935_calib_rco();
}

// set amplifier gain
int as3935_set_gain(uint8_t g){
    if(g > 0x1f) return FALSE;
    t_afe_gain a = {0};
    a.AFE_GB = g;
    return I2Cwrite(AFE_GAIN, a.u8);
}

// watchdog threshold
int as3935_wdthres(uint8_t t){
    if(t > 0x0f) return FALSE;
    t_threshold thres;
    if(!I2Cread(THRESHOLD, &thres)) return FALSE;
    thres.WDTH = t;
    return I2Cwrite(THRESHOLD, thres.u8);
}

// noice floor level
int as3935_nflev(uint8_t l){
    if(l > 7) return FALSE;
    t_threshold thres;
    if(!I2Cread(THRESHOLD, &thres)) return FALSE;
    thres.NF_LEV = l;
    return I2Cwrite(THRESHOLD, thres.u8);
}

// spike rejection
int as3935_srej(uint8_t s){
    if(s > 0xf) return FALSE;
    t_lightning_reg lr;
    if(!I2Cread(LIGHTNING_REG, &lr)) return FALSE;
    lr.SREJ = s;
    return I2Cwrite(LIGHTNING_REG, lr.u8);
}

// minimal lighting number
int as3935_minnumlig(uint8_t n){
    if(n > 3) return FALSE;
    t_lightning_reg lr;
    if(!I2Cread(LIGHTNING_REG, &lr)) return FALSE;
    lr.MIN_NUM_LIG = n;
    return I2Cwrite(LIGHTNING_REG, lr.u8);
}

// clear amount of lightnings for last 15 min
int as3935_clearstat(){
    t_lightning_reg lr;
    if(!I2Cread(LIGHTNING_REG, &lr)) return FALSE;
    lr.CL_STAT = 1;
    return I2Cwrite(LIGHTNING_REG, lr.u8);
}

// get interrupt code
int as3935_intcode(uint8_t *code){
    if(!code) return FALSE;
    t_int_mask_ant i;
    if(!I2Cread(INT_MASK_ANT, &i)) return FALSE;
    *code = i.INT;
    return TRUE;
}

// should interrupt react on disturbers?
int as3935_mask_disturber(uint8_t m){
    if(m > 1) return FALSE;
    t_int_mask_ant i;
    if(!I2Cread(INT_MASK_ANT, &i)) return FALSE;
    i.MASK_DIST = m;
    return I2Cwrite(INT_MASK_ANT, i.u8);
}

// set Fdiv of antenna LCO
int as3935_lco_fdiv(uint8_t d){
    if(d > 3) return FALSE;
    t_int_mask_ant i;
    if(!I2Cread(INT_MASK_ANT, &i)) return FALSE;
    i.LCO_FDIV = d;
    return I2Cwrite(INT_MASK_ANT, i.u8);
}

// calculate last lightning energy
int as3935_energy(uint32_t *E){
    if(!E) return FALSE;
    uint8_t u8; uint32_t energy;
    t_s_lig_mm mm;
    if(!I2Cread(S_LIG_MM, &mm)) return FALSE;
    energy = mm.S_LIG_MM << 8;
    if(!I2Cread(S_LIG_M, &u8)) return FALSE;
    energy |= u8;
    energy <<= 8;
    if(!I2Cread(S_LIG_L, &u8)) return FALSE;
    energy |= u8;
    *E = energy;
    return TRUE;
}

// get distance
int as3935_distance(uint8_t *d){
    if(!d) return FALSE;
    t_distance dist;
    if(!I2Cread(DISTANCE, &dist)) return FALSE;
    *d = dist.DISTANCE;
    return TRUE;
}

// reset to factory settings
int as3935_resetdef(){
    return I2Cwrite(PRESET_DEFAULT, DIRECT_COMMAND);
}
