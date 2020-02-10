#!/bin/sh
#
#   Script:  Switch-to-lava.sh
#   Script to switch from water or snow to lava
#
cd ~/src/SARndbox-2.6.1/share/SARndbox-2.6.1/Shaders
echo 'Switching to Lava'
cp SurfaceAddWaterColor-Lava.fs SurfaceAddWaterColor.fs
cd ~/src/SARndbox-2.6.1
