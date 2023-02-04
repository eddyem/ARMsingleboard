#!/bin/bash

ctr=1

function getx() {
	~/MLX90640_OrangePi/mlx -s ${2} -a ${1} | head -n 36 | tail -n 24 > data.dat && ~/MLX90640_OrangePi/plotimage
	mv sensor.jpg sensor_${1}_${2}_${TIMEST}.jpg
	mv data.dat ${TIMEST}_${1}_${2}.dat

}

while true; do
	h=$(date +"%H")
	[ $h -gt 7 -a $h -lt 17 ] && exit 0
	TIMEST=$(date +%d-%H:%M)
	wget http://zarch.sao.ru/lastMONO.jpg?0.$((ctr++)) -O allsky_${TIMEST}.jpg

	for a in 0x33 0x37; do for i in 0 1 2; do
		getx $a $i
	done; done

	sleep 60
done
