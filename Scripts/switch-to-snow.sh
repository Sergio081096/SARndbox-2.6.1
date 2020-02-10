#!/bin/sh
#
#   Script:  Switch-to-snow.sh
#   Script to switch from water or lava to snow
#
cd ~/src/SARndbox-2.6.1/share/SARndbox-2.6.1/Shaders
echo 'Switching to Snow'
cp SurfaceAddWaterColor-Snow.fs SurfaceAddWaterColor.fs
cd ~/src/SARndbox-2.6.1
