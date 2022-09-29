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

#define HTU21D_CONVTIMEOUT (2.0)

typedef enum{
    HTU21D_BUSY,        // measurement in progress
    HTU21D_ERR,         // error in I2C
    HTU21D_RELAX,       // relaxed state
    HTU21D_RDY,         // data ready - can get it
} HTU21D_status;

HTU21D_status HTU21D_get_status();

int HTU21D_read_ID();
void HTU21D_process();
int HTU21D_startmeasure();
int HTU21D_getTH(float *T, float *H);
int HTU21D_heater(int ON);

