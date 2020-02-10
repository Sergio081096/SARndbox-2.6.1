#!/bin/sh
#
#   Script:  Switch-to-water.sh
#   Script to switch from snow or lava to water
#
cd ~/src/SARndbox-2.6.1/share/SARndbox-2.6.1/Shaders
echo 'Switching to Water'
cp SurfaceAddWaterColor-Water.fs SurfaceAddWaterColor.fs
cd ~/src/SARndbox-2.6.1
