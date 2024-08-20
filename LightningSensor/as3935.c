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

#pragma once

#include "as3935.h"
#include "i2c.h"

static int dev_fd = -1;

// open device and set slave address
int as3935_open(const char *path, uint8_t id){
    dev_fd = i2c_open(path);
    if(dev_fd < 0) return FALSE;
    if(!i2c_set_slave_address(dev_fd, id)){
        WARNX("Can't set slave address 0x%02x", G.slaveaddr);
        return FALSE;
    }
    return as3935_wakeup();
}

// common getter
int as3935_getter(uint8_t reg, uint8_t *data){
    return i2c_read_reg(dev_fd, reg, data);
}

// and common setter
int as3935_setter(uint8_t reg, uint8_t data){
    return i2c_write_reg(dev_fd, reg, data);
}


// starting calibration
int as3935_calib_rco(){
    t_tun_disp t;
    if(!i2c_read_reg(dev_fd, TUN_DISP, &t)) return FALSE;
    if(!i2c_write_reg(dev_fd, CALIB_RCO, DIRECT_COMMAND)) return FALSE;
    t.DISP_SRCO = 1;
    if(!i2c_write_reg(dev_fd, TUN_DISP, &t)) return FALSE;
    double t0 = dtime();
    while(dtime() - t0 < 0.002);
    t.DISP_SRCO = 0;
    if(!i2c_write_reg(dev_fd, TUN_DISP, &t)) return FALSE;
    t_calib srco, trco;
    if(!i2c_read_reg(dev_fd, CALIB_TRCO, &trco)) return FALSE;
    if(!i2c_read_reg(dev_fd, CALIB_SRCO, &srco)) return FALSE;
    if(!srco.CALIB_DONE || !trco.CALIB_DONE) return FALSE;
    return TRUE;
}

// wakeup - call this function after first run
int as3935_wakeup(){
    t_afe_gain g;
    if(!i2c_read_reg(dev_fd, AFE_GAIN, &g)) return FALSE;
    g.PWD = 0;
    if(!i2c_write_reg(dev_fd, AFE_GAIN, g)) return FALSE;
    return as3935_calib_rco;
}

// set amplifier gain
int as3935_set_gain(uint8_t g){
    if(g > 0x1f) return FALSE;
    t_afe_gain a = {0};
    a.AFE_GB = g;
    return i2c_write_reg(dev_fd, AFE_GAIN, a);
}

// watchdog threshold
int as3935_wdthres(uint8_t t){
    if(t > 0x0f) return FALSE;
    t_threshold thres;
    if(!i2c_read_reg(dev_fd, THRESHOLD, &thres)) return FALSE;
    thres.WDTH = t;
    return i2c_write_reg(dev_fd, THRESHOLD, thres);
}

// noice floor level
int as3935_nflev(uint8_t l){
    if(l > 7) return FALSE;
    t_threshold thres;
    if(!i2c_read_reg(dev_fd, THRESHOLD, &thres)) return FALSE;
    thres.NF_LEV = l;
    return i2c_write_reg(dev_fd, THRESHOLD, thres);
}

// spike rejection
int as3935_srej(uint8_t s){
    if(s > 0xf) return FALSE;
    t_lightning_reg lr;
    if(!i2c_read_reg(dev_fd, LIGHTNING_REG, &lr)) return FALSE;
    lr.SREJ = s;
    return i2c_write_reg(dev_fd, LIGHTNING_REG, lr);
}

// minimal lighting number
int as3935_minnumlig(uint8_t n){
    if(n > 3) return FALSE;
    t_lightning_reg lr;
    if(!i2c_read_reg(dev_fd, LIGHTNING_REG, &lr)) return FALSE;
    lr.MIN_NUM_LIG = n;
    return i2c_write_reg(dev_fd, LIGHTNING_REG, lr);
}

// clear amount of lightnings for last 15 min
int as3935_clearstat(){
    if(n > 3) return FALSE;
    t_lightning_reg lr;
    if(!i2c_read_reg(dev_fd, LIGHTNING_REG, &lr)) return FALSE;
    lr.CL_STAT = 1;
    return i2c_write_reg(dev_fd, LIGHTNING_REG, lr);
}

// get interrupt code
int as3935_intcode(uint8_t *code){
    if(!code) return FALSE;
    t_int_mask_ant i;
    if(!i2c_read_reg(dev_fd, INT_MASK_ANT, &i)) return FALSE;
    *code = i.INT;
    return TRUE;
}

// should interrupt react on disturbers?
int as3935_mask_disturber(uint8_t m){
    if(m > 1) return FALSE;
    t_int_mask_ant i;
    if(!i2c_read_reg(dev_fd, INT_MASK_ANT, &i)) return FALSE;
    i.MASK_DIST = m;
    return i2c_write_reg(dev_fd, INT_MASK_ANT, i);
}

// set Fdiv of antenna LCO
int as3935_lco_fdiv(uint8_t d){
    if(d > 3) return FALSE;
    t_int_mask_ant i;
    if(!i2c_read_reg(dev_fd, INT_MASK_ANT, &i)) return FALSE;
    i.LCO_FDIV = d;
    return i2c_write_reg(dev_fd, INT_MASK_ANT, i);
}

// calculate last lightning energy
int as3935_energy(uint32_t *E){
    if(!E) return FALSE;
    uint8_t u8; uint32_t energy;
    t_s_lig_mm mm;
    if(!i2c_read_reg(dev_fd, S_LIG_MM, &mm)) return FALSE;
    energy = mm.S_LIG_MM << 8;
    if(!i2c_read_reg(dev_fd, S_LIG_M, &u8)) return FALSE;
    energy |= u8;
    energy <<= 8;
    if(!i2c_read_reg(dev_fd, S_LIG_L, &u8)) return FALSE;
    energy |= u8;
    *E = energy;
    return TRUE;
}

int as3935_distance(uint8_t *d){
    if(!d) return FALSE;
    t_distance dist;
    if(!i2c_read_reg(dev_fd, DISTANCE, &dist)) return FALSE;
    *d = dist.DISTANCE;
    return TRUE;
}

