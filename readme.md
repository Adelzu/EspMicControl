Simple esp32 push button app that will toggle the mic mute if the button is pressed, and will be as push to talk if held.

You will need:
* ESP32
* Push button, one lead is connected to pin 15, the other to GND.
* WS2812 LED Strip connected to pin 5 (Set the LED_COUNT)

Just install the following pre-requisits.
```
pip install pyserial
pip install pycaw
pip install comtypes
```

and run the python script
```
python espmiccontrol.py
```

Work in progress
