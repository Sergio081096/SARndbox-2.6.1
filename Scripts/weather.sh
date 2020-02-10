# weather.sh
#!/bin/bash

# pick up current state
  weather=$( cat ~/src/Scripts/weather_file.tmp )
  echo "Current state is " $weather
 
# use case statement to set the assignment to the next cycle value
    case $weather in
     "rain") echo "Changed rain to lava"
            sh ~/src/Scripts/switch-to-lava.sh
            echo "lava" > ~/src/Scripts/weather_file.tmp
            ;;
     "lava") echo "Changed lava to Toxic Waste"
            sh ~/src/Scripts/switch-to-ToxicWaste.sh 
            echo "waste" > ~/src/Scripts/weather_file.tmp
            ;;
     "waste") echo "Changed Toxic Waste to snow"
            sh ~/src/Scripts/switch-to-snow.sh 
            echo "snow" > ~/src/Scripts/weather_file.tmp
            ;;
    "snow") echo "Changed snow to rain"
            sh ~/src/Scripts/switch-to-water.sh 
            echo "rain" > ~/src/Scripts/weather_file.tmp
            ;;
    esac
