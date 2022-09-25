# N64 controller reset
This Arduino code can be used to reset your N64 with a N64 controller, which is very usefull if you own an Everdrive. I have used the Arduino Nano, with pin 2 connected to controller player 1 and pin 3 connected to the reset button of the N64. In case you use a different microcontroller, the timing may be different. For this code, the button L, R and C Down need to be pressed to reset the N64. You can change the code in case you want to use a different button combination.

The code is based on the NintendoSpy Arduino code:
https://github.com/jaburns/NintendoSpy
