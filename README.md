# arduWatering, a low-power automatic watering
 - Battery saver. Arduino is set in watch dog mode and it is managed by interruptions.
 - Watering starts once a day and it lasts for 6 minutes.
 - A solenoid valve is activated through pin 4.
 - A hardware interruption on pin 2 (for example, by using a switch) shows the remaining time to start the watering.
 - When battery life is very low it blinks the led 13.

# References:
 - http://arduino.stackexchange.com/questions/6/what-are-or-how-do-i-use-the-power-saving-options-of-the-arduino-to-extend-bat
 - http://www.gammon.com.au/forum/?id=11497
 - http://oregonembedded.com/batterycalc.htm
 - http://www.fiz-ix.com/2012/11/low-power-arduino-using-the-watchdog-timer/
 - http://donalmorrissey.blogspot.com.es/2010/04/putting-arduino-diecimila-to-sleep-part.html


# TODO:
 - Show time in the 4-digit display
 - Buy pipes, cube with valve, splitters, battery, pump/solenoid valve, case
 - Remove Serial references
 - Add more information in README.md (images, schematics...)
