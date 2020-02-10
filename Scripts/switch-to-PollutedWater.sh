#!/bin/sh
#
#   Script:  Switch-to-PollutedWater.sh
#   Script to switch from snow or lava to Polluted Water
#
cd ~/src/SARndbox-2.6.1/share/SARndbox-2.6.1/Shaders
echo 'Switching to Polluted Water'
cp SurfaceAddWaterColor-PollutedWater.fs SurfaceAddWaterColor.fs
cd ~/src/SARndbox-2.3
