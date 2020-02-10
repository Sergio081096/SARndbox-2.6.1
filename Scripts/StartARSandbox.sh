#!/bin/sh
#
#   Script:  StartARSandbox.sh
#   Script to start the AR Sandbox software
#
echo $(date) >> /home/sergio/src/SARndbox-2.6.1/log
sleep 10s
./SARndbox-2.6.1/bin/SARndbox -uhm -fpv -rer 20 100 -evr -0.01 
#-loadInputGraph ~/src/SARndbox-2.3/etc/SARndbox-2.3/DMNSInputGraph.inputgraph
