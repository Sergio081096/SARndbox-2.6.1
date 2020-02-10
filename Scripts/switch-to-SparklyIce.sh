#!/bin/sh
#
#   Script:  Switch-to-SparklyIce.sh
#   Script to switch from snow or lava to Sparkly Ice
#
cd ~/src/SARndbox-2.6.1/share/SARndbox-2.6.1/Shaders
echo 'Switching to Sparkly Ice'
cp SurfaceAddWaterColor-SparklyIce.fs SurfaceAddWaterColor.fs
cd ~/src/SARndbox-2.6.1
