/*
 * This file is part of the i2c project.
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
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <usefull_macros.h>

typedef struct{
    char *device;
    int slaveaddr;
    int help;
    int reg16;
    int reg8;
    int data2write;
    int datalen;
} glob_pars;
static glob_pars G = {.device = "/dev/i2c-3", .slaveaddr = 0x33};
static myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    _("I2C device path")},
    {"slave",   NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.slaveaddr), _("I2C slave address")},
    {"reg16",   NEED_ARG,   NULL,   'r',    arg_int,    APTR(&G.reg16),     _("16-bit register address to read/write")},
    {"reg8",    NEED_ARG,   NULL,   'R',    arg_int,    APTR(&G.reg8),      _("8-bit register address to read/write")},
    {"data",    NEED_ARG,   NULL,   'D',    arg_int,    APTR(&G.data2write),_("data to write")},
    {"len",     NEED_ARG,   NULL,   'l',    arg_int,    APTR(&G.datalen),   _("length of data to read")},
   end_option
};


#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

static uint16_t lastaddr = 0;

static int i2c_read_reg8(int fd, uint8_t regaddr, uint8_t *data){
    struct i2c_smbus_ioctl_data args;
    union i2c_smbus_data sd;
    args.read_write = I2C_SMBUS_READ;
    args.command    = regaddr;
    args.size       = I2C_SMBUS_BYTE_DATA;
    args.data       = &sd;
    if(ioctl(fd, I2C_SMBUS, &args) < 0) return FALSE;
    *data = sd.byte;
    return TRUE;
}
static int i2c_write_reg8(int fd, uint8_t regaddr, uint8_t data){
    struct i2c_smbus_ioctl_data args;
    union i2c_smbus_data sd;
    sd.byte = data;
    args.read_write = I2C_SMBUS_WRITE;
    args.command    = regaddr;
    args.size       = I2C_SMBUS_BYTE_DATA;
    args.data       = &sd;
    if(ioctl(fd, I2C_SMBUS, &args) < 0) return FALSE;
    return TRUE;
}
static int i2c_read_reg(int fd, uint16_t regaddr, uint16_t *data){
    struct i2c_msg m[2];
    struct i2c_rdwr_ioctl_data x = {.msgs = m, .nmsgs = 2};
    m[0].addr = lastaddr; m[1].addr = lastaddr;
    m[0].flags = 0;
    m[1].flags = I2C_M_RD;
    m[0].len = 2; m[1].len = 2;
    uint8_t a[2], d[2] = {0};
    a[0] = regaddr >> 8;
    a[1] = regaddr & 0xff;
    m[0].buf = a; m[1].buf = d;
    if(ioctl(fd, I2C_RDWR, &x) < 0) return FALSE;
    *data = (uint16_t)((d[0] << 8) | (d[1]));
    return TRUE;
}
static int i2c_write_reg(int fd, uint16_t regaddr, uint16_t data){
    union i2c_smbus_data d;
    d.block[0] = 3;
    d.block[1] = regaddr & 0xff;
    d.block[2] = data >> 8;
    d.block[3] = data & 0xff;
    struct i2c_smbus_ioctl_data args;
    args.read_write = I2C_SMBUS_WRITE;
    args.command    = regaddr >> 8;
    args.size       = I2C_SMBUS_I2C_BLOCK_DATA;
    args.data       = &d;
    if(ioctl(fd, I2C_SMBUS, &args) < 0) return FALSE;
    printf("Block: ");
    for(int i = 0; i < 4; ++i) printf("0x%02x ", d.block[i]);
    printf("\n");
    return TRUE;
}


static inline int i2c_set_slave_address(int fd, uint8_t addr){
    if(ioctl (fd, I2C_SLAVE, addr) < 0) return FALSE;
    lastaddr = addr;
    return TRUE;
}

static inline int i2c_open(const char *path){
    return open(path, O_RDWR);
}

int main(int argc, char **argv){
    uint16_t d;
    uint8_t d8;
    initial_setup();
    parseargs(&argc, &argv, cmdlnopts);
    if(G.help) showhelp(-1, cmdlnopts);
    if(G.slaveaddr < 0 || G.slaveaddr > 0x7f) ERRX("I2C address should be 7-bit");
    if(G.reg16 && G.reg8) ERRX("Enter either 8-bit address or 16-bit");
    int fd = i2c_open(G.device);
    if(fd < 0) ERR("Can't open %s", G.device);
    if(G.datalen){
        if(G.datalen < 0) ERRX("data length is uint16_t");
        if(G.datalen + G.reg16 > 0xffff) ERRX("Data len + start reg should be uint16_t");
    }
    if(!i2c_set_slave_address(fd, (uint8_t)G.slaveaddr)){
        WARN("Can't set slave address 0x%02x", G.slaveaddr);
        goto clo;
    }
    if(!i2c_read_reg8(fd, (uint16_t)0, &d8)){
        WARN("Can't find slave 0x%02x", G.slaveaddr);
        goto clo;
    }
    green("Connected to slave 0x%02x\n", G.slaveaddr);
    if(!G.reg8 && !G.reg16) goto clo; // nothing to do
    if(G.data2write){ // write data to register
        if(G.reg8){
            if(G.data2write < 0 || G.data2write > 0xff){
                WARNX("Data to write should be uint8_t");
                goto clo;
            }
            printf("Try to write 0x%02x to 0x%02x ... ", G.data2write, G.reg8);
            if(!i2c_write_reg8(fd, (uint8_t)G.reg16, (uint8_t)G.data2write)){
                WARN("Can't write"); goto clo;
            }
            else printf("OK\n");
        }else{
            if(G.data2write < 0 || G.data2write > 0xffff){
                WARNX("Data to write should be uint16_t");
                goto clo;
            }
            printf("Try to write 0x%04x to 0x%04x ... ", G.data2write, G.reg16);
            if(!i2c_write_reg(fd, (uint16_t)G.reg16, (uint16_t)G.data2write)){
                WARN("Can't write"); goto clo;
            }
            else printf("OK\n");
        }
    }
    if(!G.datalen){
        if(G.reg8){
            if(!i2c_read_reg8(fd, (uint8_t)G.reg8, &d8)){
                WARN("Can't read"); goto clo;
            }
            printf("Read: 0x%02x\n", d8);
        }else{
            if(!i2c_read_reg(fd, (uint16_t)G.reg16, &d)){
                WARN("Can't read"); goto clo;
            }
            printf("Read: 0x%04x\n", d);
        }
    }else{
        int reg = (G.reg8) ? G.reg8 : G.reg16;
        int lastreg = G.datalen + reg;
        for(int i = reg; i < lastreg; ++i){
            if(G.reg8){
                if(!i2c_read_reg8(fd, (uint8_t)i, &d8)){
                    WARN("Can't read"); continue;
                }
                printf("%2d: 0x%02x -> 0x%02x\n", i-reg, i, d8);
            }else{
                if(!i2c_read_reg(fd, (uint16_t)i, &d)){
                    WARN("Can't read"); continue;
                }
                printf("%4d: 0x%04x -> 0x%04x\n", i-G.reg16, i, d);
            }
        }
    }
clo:
    close(fd);
    return 0;
}

