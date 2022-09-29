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

#pragma once

#include <stdint.h>

// conversion timeout (s)
#define SI7005_CONVTIMEOUT  2.0

typedef enum{
    SI7005_BUSY,        // measurement in progress
    SI7005_ERR,         // error in I2C
    SI7005_RELAX,       // relaxed state
    SI7005_RDY,         // data ready - can get it
} si7005_status;

si7005_status si7005_get_status();

int si7005_read_ID();
void si7005_process();
int si7005_startmeasure();
int si7005_getTH(float *T, float *H);
int si7005_heater(int ON);


