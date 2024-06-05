/*
 * This file is part of the schlagbaum project.
 * Copyright 2023 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

// GPIO polling interval - 50ms
#define GPIO_POLL_INTERVAL (0.05)

// amount of in/out GPIO pins
#define GPIO_IN_NUMBER      (6)
#define GPIO_OUT_NUMBER     (6)

// maximal GPIO number
#define GPIO_MAX_NUMBER     (32)

// 6 outputs
#define GPIO_OUT_MASK   0x3f
// 6 inputs
#define GPIO_IN_MASK    0x3f

// timeout - clear GPIO after receiving command - 1 minute
#define GPIO_TIMEOUT    (60.)

int gpio_open_device(const char *path);
int gpio_setup_outputs();
int gpio_setup_inputs();
int gpio_poll(uint32_t *up, uint32_t *down);
int gpio_set_output(int input);
int gpio_clear_output(int input);
void gpio_close();

