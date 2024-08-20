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

uint8_t slaveaddr = 0;

int i2c_read_reg(int fd, uint8_t regaddr, uint8_t *data){
    struct i2c_smbus_ioctl_data args;
    union i2c_smbus_data sd;
    args.read_write = I2C_SMBUS_READ;
    args.command    = regaddr;
    args.size       = I2C_SMBUS_BYTE_DATA;
    args.data       = &sd;
    if(ioctl(fd, I2C_SMBUS, &args) < 0){
        WARNX("Can't read reg %d", regaddr);
        LOGWARN("Can't read reg %d", regaddr);
        return FALSE;
    }
    *data = sd.byte;
    return TRUE;
}

int i2c_write_reg(int fd, uint8_t regaddr, uint8_t data){
    struct i2c_smbus_ioctl_data args;
    union i2c_smbus_data sd;
    sd.byte = data;
    args.read_write = I2C_SMBUS_WRITE;
    args.command    = regaddr;
    args.size       = I2C_SMBUS_BYTE_DATA;
    args.data       = &sd;
    if(ioctl(fd, I2C_SMBUS, &args) < 0){
        WARNX("Can't write reg %d", regaddr);
        LOGWARN("Can't write reg %d", regaddr);
        return FALSE;
    }
    return TRUE;
}

int i2c_set_slave_address(int fd, uint8_t addr){
    if(addr == slaveaddr) return TRUE;
    if(ioctl (fd, I2C_SLAVE, addr) < 0){
        WARNX("Can't set slave address %d", addr);
        LOGWARN("Can't set slave address %d", addr);
        return FALSE;
    }
    slaveaddr = addr;
    return TRUE;
}

int i2c_open(const char *path){
    return open(path, O_RDWR);
}
