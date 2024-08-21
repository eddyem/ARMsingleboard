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
    int gain;
    int slaveaddr;
    int verbose;
    int wakeup;
    int dumpregs;
    int monitnew;
    int reset;
    int help;
    int tunelco;
    int irqdisp;
    int lcofdiv;
    char *logfile;
} glob_pars;

static glob_pars G = {
    .device = "/dev/i2c-0",
    .gain = -1,
    .irqdisp = -1,
    .lcofdiv = -1,
    .slaveaddr = 0,
    .tunelco=-1,
};

static myoption cmdlnopts[] = {
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    "I2C device path"},
    {"dumpregs",NO_ARGS,    NULL,   'D',    arg_int,    APTR(&G.dumpregs),  "dump all registers of device"},
    {"fdiv",    NEED_ARG,   NULL,   'f',    arg_int,    APTR(&G.lcofdiv),   "change LCO_FDIV value"},
    {"gain",    NEED_ARG,   NULL,   'g',    arg_int,    APTR(&G.gain),      "change AFE_GB (gain) value"},
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"irqdisp",  NEED_ARG,  NULL,     0,    arg_int,    APTR(&G.irqdisp),   "show LCO on IRQ: nothing (0), TRCO (1), SRCO (2) or LCO (3)"},
    {"monitnew",NO_ARGS,    NULL,   'n',    arg_int,    APTR(&G.monitnew),  "monitor changed values"},
    {"slave",   NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.slaveaddr), "I2C slave address"},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   "Verbose (each -v increase)"},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   "file for logging"},
    {"reset",   NO_ARGS,    NULL,   'R',    arg_int,    APTR(&G.reset),     "reset to factory settings"},
    {"tunelco", NEED_ARG,   NULL,   't',    arg_int,    APTR(&G.tunelco),   "tune LCO with given value"},
    {"wakeup",  NO_ARGS,    NULL,   'w',    arg_int,    APTR(&G.wakeup),    "wakeup device"},
   end_option
};

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif
static uint8_t oldvals[256] = {0};
#define TRY(reg) if(as3935_getter(reg, &u8)){if(!onlynew || u8 != oldvals[reg]){ green(STRINGIFY(reg) ": "); oldvals[reg] = u8;
#define EL(reg) }}else WARNX("Can't read " STRINGIFY(reg) );
void dumpregs(int onlynew){
    uint8_t u8;
    TRY(AFE_GAIN)
    t_afe_gain g = (t_afe_gain)u8;
    printf("PWD=%d, AFE_GB=%d\n", g.PWD, g.AFE_GB);
    EL(AFE_GAIN)
    TRY(THRESHOLD)
    t_threshold t = (t_threshold) u8;
    printf("WDTH=%d, NF_LEV=%d\n", t.WDTH, t.NF_LEV);
    EL(THRESHOLD)
    TRY(LIGHTNING_REG)
    t_lightning_reg l = (t_lightning_reg) u8;
    printf("SREJ=%d, MIN_NUM_LIG=%d, CL_STAT=%d\n", l.SREJ, l.MIN_NUM_LIG, l.CL_STAT);
    EL(LIGHTNING_REG)
    TRY(INT_MASK_ANT)
    t_int_mask_ant i = (t_int_mask_ant) u8;
    printf("INT=%d ", i.INT);
    const char *intw;
    switch(i.INT){
        case 0:
            intw = "no interrupts";
        break;
        case INT_NH:
            intw = "noice too high";
        break;
        case INT_D:
            intw = "disturber";
        break;
        case INT_L:
            intw = "lightning";
        break;
        default: intw = "unknown";
    }
    printf("(%s), MASK_DIST=%d, LCO_FDIV=%d\n", intw, i.MASK_DIST, i.LCO_FDIV);
    EL(INT_MASK_ANT)
    TRY(S_LIG_L)
    printf("%d\n", u8);
    EL(S_LIG_L)
    TRY(S_LIG_M)
    printf("%d\n", u8);
    EL(S_LIG_M)
    TRY(S_LIG_MM)
    printf("%d\n", u8);
    EL(S_LIG_MM)
    TRY(DISTANCE)
    printf("%d\n", u8);
    EL(DISTANCE)
    TRY(TUN_DISP)
    t_tun_disp t = (t_tun_disp) u8;
    printf("TUN_CAP=%d, DISP_TRCO=%d, DISP_SRCO=%d, DISP_LCO=%d\n", t.TUN_CAP, t.DISP_TRCO, t.DISP_SRCO, t.DISP_LCO);
    EL(TUN_DISP)
    TRY(CALIB_TRCO)
    t_calib c = (t_calib) u8;
    printf("CALIB_NOK=%d, CALIB_DONE=%d\n", c.CALIB_NOK, c.CALIB_DONE);
    EL(CALIB_TRCO)
    TRY(CALIB_SRCO)
    t_calib c = (t_calib) u8;
    printf("CALIB_NOK=%d, CALIB_DONE=%d\n", c.CALIB_NOK, c.CALIB_DONE);
    EL(CALIB_SRCO)
}
#undef TRY
#undef EL

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
    if(G.dumpregs || G.monitnew) dumpregs(0);
    if(G.reset) if(!as3935_resetdef()) ERRX("Can't reset to default settings");
    if(G.wakeup){
        if(!as3935_wakeup()) ERRX("Can't wakeup sensor");
    }
    if(G.gain > -1){
        if(!as3935_gain(G.gain)) ERRX("Can't set gain");
        else green("AFE_GB=%d\n", G.gain);
    }
    if(G.irqdisp > -1){
        if(!as3935_displco(G.irqdisp)) ERRX("Can't change DISP_xx");
        else green("DISP changed\n");
    }
    if(G.tunelco > -1){
        if(!as3935_tuncap(G.tunelco)) ERRX("Can't set TUN_CAP to %d", G.tunelco);
        else green("TUN_CAP = %d\n", G.tunelco);
    }
    if(G.lcofdiv > -1){
        if(!as3935_lco_fdiv(G.lcofdiv)) ERRX("Can't change FDIV");
        else green("LCO_FDIV=%d\n", G.lcofdiv);
    }
    if(G.monitnew) while(1){
        dumpregs(1);
        sleep(1);
    }
    return 0;
}

