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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/gpio.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "gpio.h"

static int gpiofd = -1;
static struct gpio_v2_line_request rq_in, rq_out;

// inputs and outputs
static const int gpio_inputs[GPIO_IN_NUMBER] = {18, 23, 24, 25, 8, 7};
static const int gpio_outputs[GPIO_OUT_NUMBER] = {17, 27, 22, 10, 9, 11};

// last time GPIO was activated
static double gpio_clear_time[GPIO_OUT_NUMBER] = {1., 1., 1., 1., 1., 1.};
// last GPIO event times & event values
static double gpio_in_time[GPIO_IN_NUMBER] = {0.};
static enum gpio_v2_line_event_id gpio_in_event_id[GPIO_IN_NUMBER] = {0};

/**
 * @brief gpio_chkclr - clear outputs by timeout
 */
static void gpio_chkclr(){
    double tnow = dtime();
    for(int i = 0; i < GPIO_OUT_NUMBER; ++i){
        if(gpio_clear_time[i] < 0.) continue;
        if(tnow - gpio_clear_time[i] < GPIO_TIMEOUT) continue;
        gpio_set_output(gpio_outputs[i]);
    }
}

/**
 * @brief gpio_open_device - open GPIO device
 * @param path - path to device
 * @return device fd or -1 if error
 */
int gpio_open_device(const char *path){
    FNAME();
    gpiofd = open(path, O_RDONLY);
    if(gpiofd < 0){
        LOGERR("Unabled to open %s: %s", path, strerror(errno));
        WARNX("Can't open GPIO device %s", path);
        return -1;
    }
    struct gpiochip_info info;

    // Query GPIO chip information
    if(-1 == ioctl(gpiofd, GPIO_GET_CHIPINFO_IOCTL, &info)){
        LOGERR("Unable to get chip info from ioctl: %s", strerror(errno));
        WARNX("Unable to get chip info");
        close(gpiofd);
        return -1;
    }
    verbose(2, "Chip name: %s", info.name);
    verbose(2, "Chip label: %s", info.label);
    verbose(2, "Number of lines: %d", info.lines);
    rq_in.fd = -1;
    rq_out.fd = -1;
    return gpiofd;
}

/**
 * @brief gpio_set_outputs - set output pins (ACTIVE_LOW!!! so we need to invert incoming data for proper work)
 * @return rq.fd or -1 if failed
 */
int gpio_setup_outputs(){
    FNAME();
    bzero(&rq_out, sizeof(rq_out));
    for(int i = 0; i < GPIO_OUT_NUMBER; ++i)
        rq_out.offsets[i] = gpio_outputs[i];
    snprintf(rq_out.consumer, GPIO_MAX_NAME_SIZE-1, "outputs");
    rq_out.num_lines = GPIO_OUT_NUMBER;
    rq_out.config.flags = GPIO_V2_LINE_FLAG_OUTPUT | GPIO_V2_LINE_FLAG_BIAS_DISABLED;
    rq_out.config.num_attrs = 0;
    if(-1 == ioctl(gpiofd, GPIO_V2_GET_LINE_IOCTL, &rq_out)){
        LOGERR("Unable setup outputs: %s", strerror(errno));
        WARNX("Can't setup outputs");
        return -1;
    }
    gpio_chkclr(); // set all outputs
    DBG("Outputs are ready");
    return rq_out.fd;
}

static int gpio_setreset(int output, int set){
    int idx = -1;
    for(int i = 0; i < GPIO_OUT_NUMBER; ++i){
        if(gpio_outputs[i] == output){
            idx = i; break;
        }
    }
    if(idx < 0 || idx > GPIO_OUT_NUMBER) return FALSE;
    if(set == 0 && (dtime() - gpio_clear_time[idx] < GPIO_SETTMOUT)) return FALSE; // time!
    struct gpio_v2_line_values values;
    bzero(&values, sizeof(values));
    uint64_t val = (1<<idx) & GPIO_OUT_MASK;
    values.mask = val;
    values.bits = set ? val : 0;
    DBG("mask=%" PRIu64 ", val=%" PRIu64, values.mask, values.bits);
    if(-1 == ioctl(rq_out.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values)){
        LOGERR("Unable to change GPIO values (mask=%" PRIu64 ", val=%" PRIu64 ": %s", values.mask, values.bits, strerror(errno));
        WARNX("Can't change GPIO values");
        return FALSE;
    }
    // change last event time
    if(set == 0) gpio_clear_time[idx] = dtime();
    else gpio_clear_time[idx] = -1.;
    return TRUE;
}

/**
 * @brief gpio_set_output - set to 1 out pin according to input number
 * @param input - number of input pin
 * @return true if all OK, false if failed
 */
int gpio_set_output(int output){
    DBG("GPIO SET");
    return gpio_setreset(output, 1);
}
/**
 * @brief gpio_clear_output - clear to 0 output pin
 * @param input - number of input pin
 * @return true if all OK, false if failed
 */
int gpio_clear_output(int output){
    DBG("GPIO CLEAR");
    return gpio_setreset(output, 0);
}


int gpio_setup_inputs(){
    FNAME();
    bzero(&rq_in, sizeof(rq_in));
    for(int i = 0; i < GPIO_IN_NUMBER; ++i)
        rq_in.offsets[i] = gpio_inputs[i];
    snprintf(rq_in.consumer, GPIO_MAX_NAME_SIZE-1, "inputs");
    rq_in.num_lines = GPIO_IN_NUMBER;
    rq_in.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_BIAS_PULL_UP | GPIO_V2_LINE_FLAG_EDGE_FALLING | GPIO_V2_LINE_FLAG_EDGE_RISING;
    rq_in.config.num_attrs = 0;
    if(-1 == ioctl(gpiofd, GPIO_V2_GET_LINE_IOCTL, &rq_in)){
        LOGERR("Unable to setup inputs: %s", strerror(errno));
        WARNX("Can't setup inputs");
        return -1;
    }
    return rq_in.fd;
}

/**
 * @brief gpio_poll - poll inputs, return only last event
 * @return bit mask of changing inputs (edge falling), 0 if nothing happen or -1 if error
 */
int gpio_poll(uint32_t *up, uint32_t *down){
    struct pollfd pfd;
    struct gpio_v2_line_event event;
    bzero(&pfd, sizeof(pfd));
    bzero(&event, sizeof(event));
    if(up) *up = 0;
    if(down) *down = 0;
    gpio_chkclr(); // clear old outputs
    pfd.fd = rq_in.fd;
    pfd.events = POLLIN | POLLPRI;
    int p = poll(&pfd, 1, 1);
    if(p == 0) return 0; // nothing happened
    else if(p == -1){
        LOGERR("poll() error: %s", strerror(errno));
        WARNX("GPIO poll() error");
        return -1;
    }
    DBG("Got GPIO event!");
    int r = read(rq_in.fd, &event, sizeof(struct gpio_v2_line_event));
    if(r != sizeof(struct gpio_v2_line_event)){
        LOGERR("Error reading GPIO data");
        WARNX("Error reading GPIO data");
        return -1;
    }
    int idx = event.offset;
    double tnow = dtime();
    // omit same events or bouncing
    if(gpio_in_event_id[idx] == event.id  || tnow - gpio_in_time[idx] < GPIO_DEBOUNSE_TIMEOUT) return 0;
    gpio_in_event_id[idx] = event.id;
    gpio_in_time[idx] = tnow;
    verbose(1, "Got event:\n\ttimestamp=%" PRIu64 "\n\tid=%d\n\toff=%d\n\tseqno=%d\n\tlineseqno=%d\n\ttnow=%.3f",
        event.timestamp_ns, event.id, idx, event.seqno, event.line_seqno, tnow);
    if(up){
        if(event.id == GPIO_V2_LINE_EVENT_RISING_EDGE){
            *up = idx;
        }else *up = 0;
    }
    if(down){
        if(event.id == GPIO_V2_LINE_EVENT_FALLING_EDGE){
            *down = idx;
        }else *down = 0;
    }
    return idx;
}

void gpio_close(){
    if(gpiofd > -1){
        close(gpiofd);
        if(rq_in.fd > -1) close(rq_in.fd);
        if(rq_out.fd > -1) close(rq_out.fd);
    }
}

