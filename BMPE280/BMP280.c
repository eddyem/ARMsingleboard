/*
 * This file is part of the bmp280 project.
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

#include "i2c.h"
#include "BMP280.h"

/**
 * BMP280 registers
 */
#define BMP280_REG_HUM_LSB      0xFE
#define BMP280_REG_HUM_MSB      0xFD
#define BMP280_REG_HUM          (BMP280_REG_HUM_MSB)
#define BMP280_REG_TEMP_XLSB    0xFC /* bits: 7-4 */
#define BMP280_REG_TEMP_LSB     0xFB
#define BMP280_REG_TEMP_MSB     0xFA
#define BMP280_REG_TEMP         (BMP280_REG_TEMP_MSB)
#define BMP280_REG_PRESS_XLSB   0xF9 /* bits: 7-4 */
#define BMP280_REG_PRESS_LSB    0xF8
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_PRESSURE     (BMP280_REG_PRESS_MSB)
#define BMP280_REG_ALLDATA      (BMP280_REG_PRESS_MSB) // all data: P, T & H
#define BMP280_REG_CONFIG       0xF5 /* bits: 7-5 t_sb; 4-2 filter; 0 spi3w_en */
#define BMP280_REG_CTRL         0xF4 /* bits: 7-5 osrs_t; 4-2 osrs_p; 1-0 mode */
#define BMP280_REG_STATUS       0xF3 /* bits: 3 measuring; 0 im_update */
#define BMP280_STATUS_MSRNG     (1<<3) // measuring flag
#define BMP280_STATUS_UPDATE    (1<<0) // update flag
#define BMP280_REG_CTRL_HUM     0xF2 /* bits: 2-0 osrs_h; */
#define BMP280_REG_RESET        0xE0
    #define BMP280_RESET_VALUE     0xB6
#define BMP280_REG_ID           0xD0

#define BMP280_REG_CALIBA       0x88
#define BMP280_CALIBA_SIZE      (26)  // 26 bytes of calibration registers sequence from 0x88 to 0xa1
#define BMP280_CALIBB_SIZE      (7)   // 7 bytes of calibration registers sequence from 0xe1 to 0xe7
#define BMP280_REG_CALIB_H1     0xA1  // dig_H1
#define BMP280_REG_CALIBB       0xE1

#define BMP280_MODE_FORSED      (1)  // force single measurement
#define BMP280_MODE_NORMAL      (3)  // run continuosly

static struct {
    // temperature
    uint16_t dig_T1;    // 0x88 (LSB), 0x98 (MSB)
    int16_t  dig_T2;    // ...
    int16_t  dig_T3;
    // pressure
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;    // 0x9e, 0x9f
    // humidity (partially calculated from EEE struct)
    uint8_t unused;     // 0xA0
    uint8_t dig_H1;     // 0xA1
    int16_t dig_H2;     // 0xE1...
    uint8_t dig_H3;     // only from EEE
    uint16_t dig_H4;
    uint16_t dig_H5;
    int8_t dig_H6;
    // data is ready
    uint8_t  rdy;
} __attribute__ ((packed)) CaliData = {0};

// data for humidity calibration of BME280
static uint8_t EEE[BMP280_CALIBB_SIZE] = {0};

static struct{
    BMP280_Filter filter;       // filtering
    BMP280_Oversampling p_os;   // oversampling for pressure
    BMP280_Oversampling t_os;   // -//- temperature
    BMP280_Oversampling h_os;   // -//- humidity
    uint8_t ID;                 // identificator
    uint8_t regctl;             // control register base value [(params.t_os << 5) | (params.p_os << 2)]
} params = {
    .filter = BMP280_FILTER_OFF,
    .p_os   = BMP280_OVERS16,
    .t_os   = BMP280_OVERS16,
    .h_os   = BMP280_OVERS16,
    .ID     = 0
};

static BMP280_status bmpstatus = BMP280_NOTINIT;

BMP280_status BMP280_get_status(){
    return bmpstatus;
}

// setters for `params`
void BMP280_setfilter(BMP280_Filter f){
    params.filter = f;
}
void BMP280_setOSt(BMP280_Oversampling os){
    params.t_os = os;
}
void BMP280_setOSp(BMP280_Oversampling os){
    params.p_os = os;
}
void BMP280_setOSh(BMP280_Oversampling os){
    params.h_os = os;
}

// get compensation data, return 1 if OK
static int readcompdata(){
    FNAME();
    if(!i2c_read_data8(BMP280_REG_CALIBA, BMP280_CALIBA_SIZE, (uint8_t*)&CaliData)){
        DBG("Can't read calibration A data");
        return FALSE;
    }
    /*
    // convert big-endian into little-endian
    uint8_t *arr = (uint8_t*)&CaliData;
    for(int i = 0; i < (int)sizeof(CaliData); i+=2){
        register uint8_t val = arr[i];
        arr[i] = arr[i+1];
        arr[i+1] = val;
    }*/
    if(params.ID == BME280_CHIP_ID){
        if(!i2c_read_reg8(BMP280_REG_CALIB_H1, &CaliData.dig_H1)){
            WARNX("Can't read dig_H1");
            return FALSE;
        }
        if(!i2c_read_data8(BMP280_REG_CALIBB, BMP280_CALIBB_SIZE, EEE)){
            WARNX("Can't read rest of dig_Hx");
            return FALSE;
        }
        // E5 is divided by two parts so we need this sex
        CaliData.dig_H2 = (EEE[1] << 8) | EEE[0];
        CaliData.dig_H3 = EEE[2];
        CaliData.dig_H4 = (EEE[3] << 4) | (EEE[4] & 0x0f);
        CaliData.dig_H5 = (EEE[5] << 4) | (EEE[4] >> 4);
        CaliData.dig_H6 = EEE[6];
    }
    CaliData.rdy = 1;
    DBG("Calibration rdy");
    return TRUE;
}

// do a soft-reset procedure
int BMP280_reset(){
    if(!i2c_write_reg8(BMP280_REG_RESET, BMP280_RESET_VALUE)){
        DBG("Can't reset\n");
        return FALSE;
    }
    return TRUE;
}

// read compensation data & write registers
int BMP280_init(){
    bmpstatus = BMP280_NOTINIT;
    if(!i2c_read_reg8(BMP280_REG_ID, &params.ID)){
        DBG("Can't read BMP280_REG_ID");
        return FALSE;
    }
    DBG("Got device ID: 0x%02x", params.ID);
    if(params.ID != BMP280_CHIP_ID && params.ID != BME280_CHIP_ID){
        WARNX("Not BMP/BME\n");
        return FALSE;
    }
    if(!BMP280_reset()){
        WARNX("Can't reset");
        return FALSE;
    }
    uint8_t reg = 1;
    while(reg & BMP280_STATUS_UPDATE){ // wait while update is done
        if(!i2c_read_reg8(BMP280_REG_STATUS, &reg)){
            DBG("Can't read status");
            return FALSE;
        }
    }
    if(!readcompdata()){
        DBG("Can't read calibration data\n");
        return FALSE;
    }else{
        DBG("T: %d, %d, %d", CaliData.dig_T1, CaliData.dig_T2, CaliData.dig_T3);
        DBG("\P: %d, %d, %d, %d, %d, %d, %d, %d, %d", CaliData.dig_P1, CaliData.dig_P2, CaliData.dig_P3,
            CaliData.dig_P4, CaliData.dig_P5, CaliData.dig_P6, CaliData.dig_P7, CaliData.dig_P8, CaliData.dig_P9);
        if(params.ID == BME280_CHIP_ID){ // read H compensation
            DBG("H: %d, %d, %d, %d, %d, %d", CaliData.dig_H1, CaliData.dig_H2, CaliData.dig_H3,
                CaliData.dig_H4, CaliData.dig_H5, CaliData.dig_H6);
        }
    }
    // write filter configuration
    reg = params.filter << 2;
    if(!i2c_write_reg8(BMP280_REG_CONFIG, reg)){
        DBG("Can't save filter settings\n");
        return FALSE;
    }
    reg = (params.t_os << 5) | (params.p_os << 2); // oversampling for P/T, sleep mode
    if(!i2c_write_reg8(BMP280_REG_CTRL, reg)){
        DBG("Can't write settings for P/T\n");
        return FALSE;
    }
    params.regctl = reg;
    if(params.ID == BME280_CHIP_ID){ // write CTRL_HUM only AFTER CTRL!
        reg = params.h_os;
        if(!i2c_write_reg8(BMP280_REG_CTRL_HUM, reg)){
            DBG("Can't write settings for H\n");
            return FALSE;
        }
    }
    DBG("OK, inited");
    bmpstatus = BMP280_RELAX;
    return TRUE;
}

// @return 1 if OK, *devid -> BMP/BME
void BMP280_read_ID(uint8_t *devid){
    if(devid) *devid = params.ID;
}

// start measurement, @return 1 if all OK
int BMP280_start(){
    if(!CaliData.rdy || bmpstatus == BMP280_BUSY){
        DBG("rdy=%d, status=%d", CaliData.rdy, bmpstatus);
        return FALSE;
    }
    uint8_t reg = params.regctl | BMP280_MODE_FORSED; // start single measurement
    if(!i2c_write_reg8(BMP280_REG_CTRL, reg)){
        DBG("Can't write CTRL reg\n");
        return FALSE;
    }
    bmpstatus = BMP280_BUSY;
    return TRUE;
}

// return T in degC
static inline float compTemp(int32_t adc_temp, int32_t *t_fine){
    int32_t var1, var2;
	var1 = ((((adc_temp >> 3) - ((int32_t) CaliData.dig_T1 << 1)))
			* (int32_t) CaliData.dig_T2) >> 11;
	var2 = (((((adc_temp >> 4) - (int32_t) CaliData.dig_T1)
			* ((adc_temp >> 4) - (int32_t) CaliData.dig_T1)) >> 12)
			* (int32_t) CaliData.dig_T3) >> 14;
	*t_fine = var1 + var2;
	return ((*t_fine * 5 + 128) >> 8) / 100.f;
}

// return P in Pa
static inline float compPres(int32_t adc_press, int32_t fine_temp) {
	int64_t var1, var2, p;
	var1 = (int64_t) fine_temp - 128000;
	var2 = var1 * var1 * (int64_t) CaliData.dig_P6;
	var2 = var2 + ((var1 * (int64_t) CaliData.dig_P5) << 17);
	var2 = var2 + (((int64_t) CaliData.dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t) CaliData.dig_P3) >> 8)
			+ ((var1 * (int64_t) CaliData.dig_P2) << 12);
	var1 = (((int64_t) 1 << 47) + var1) * ((int64_t) CaliData.dig_P1) >> 33;
	if (var1 == 0){
		return 0;  // avoid exception caused by division by zero
	}
	p = 1048576 - adc_press;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = ((int64_t) CaliData.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
	var2 = ((int64_t) CaliData.dig_P8 * p) >> 19;
	p = ((p + var1 + var2) >> 8) + ((int64_t) CaliData.dig_P7 << 4);
	return p/256.f;
}

// return H in percents
static inline float compHum(int32_t adc_hum, int32_t fine_temp){
    int32_t v_x1_u32r;
	v_x1_u32r = fine_temp - (int32_t) 76800;
	v_x1_u32r = ((((adc_hum << 14) - (((int32_t)CaliData.dig_H4) << 20)
			- (((int32_t)CaliData.dig_H5) * v_x1_u32r)) + (int32_t)16384) >> 15)
			* (((((((v_x1_u32r * ((int32_t)CaliData.dig_H6)) >> 10)
					* (((v_x1_u32r * ((int32_t)CaliData.dig_H3)) >> 11)
							+ (int32_t)32768)) >> 10) + (int32_t)2097152)
					* ((int32_t)CaliData.dig_H2) + 8192) >> 14);
	v_x1_u32r = v_x1_u32r
			- (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7)
					* ((int32_t)CaliData.dig_H1)) >> 4);
	v_x1_u32r = v_x1_u32r < 0 ? 0 : v_x1_u32r;
	v_x1_u32r = v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r;
	return (v_x1_u32r >> 12)/1024.f;
}

void BMP280_process(){
    if(bmpstatus != BMP280_BUSY) return;
    // BUSY state: poll data ready
    uint8_t reg;
    if(!i2c_read_reg8(BMP280_REG_STATUS, &reg)) return;
    if(reg & BMP280_STATUS_MSRNG) return; // still busy
    bmpstatus = BMP280_RDY; // data ready
}

// read data & convert it
int BMP280_getdata(float *T, float *P, float *H){
    if(bmpstatus != BMP280_RDY) return FALSE;
    bmpstatus = BMP280_RELAX;
    uint8_t datasz = 8; // amount of bytes to read
    if(params.ID != BME280_CHIP_ID){
        DBG("Not BME!\n");
        if(H) *H = 0;
        datasz = 6;
    }
    uint8_t data[8];
    if(!i2c_read_data8(BMP280_REG_ALLDATA, datasz, data)){
        DBG("Can't read data");
        return FALSE;
    }
#ifdef EBUG
    printf("\tgot data: ");
    for(int i = 0; i < datasz; ++i){
        printf("0x%02x ", data[i]);
    }
    printf("\n");
#endif
    int32_t p = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    DBG("puncomp = %d", p);
    int32_t t = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    DBG("tuncomp = %d", t);
    int32_t t_fine;
    float Temp = compTemp(t, &t_fine);
    DBG("tfine = %d", t_fine);
    if(T) *T = Temp;
    if(P) *P = compPres(p, t_fine);
    if(H && params.ID == BME280_CHIP_ID){
        int32_t h = (data[6] << 8) | data[7];
        DBG("huncomp = %d", h);
        *H = compHum(h, t_fine);
    }
    return TRUE;
}
