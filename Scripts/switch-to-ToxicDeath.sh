#!/bin/sh
#
#   Script:  Switch-to-waste.sh
#   Script to switch from snow or lava to toxic waste
#
cd ~/src/SARndbox-2.6.1/share/SARndbox-2.6.1/Shaders
echo 'Switching to Toxic Death'
cp SurfaceAddWaterColor-ToxicDeath.fs SurfaceAddWaterColor.fs
cd ~/src/SARndbox-2.6.1
