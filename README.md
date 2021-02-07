# MOTOR and Servo Drives 
** DRAFT work in progress **
Purpose is to create a standard interface as much as possible in MQTT 
```
dst/<device>/<motor>/rpmTarget => target RPM
src/<device>/<motor>/rpmMeasured => measured RPM
```
A device can drive different motors. 
## TRIAC Motor with Tacho from washing machine
The Triac ESP32 control uses the MCPWM and is synchronized by the zero detect.
The MCPWM frequency is at 2 x the net frequency. As the triac needs to be triggered in each quadrant.
The MCPWM unit gives a pulse of 1% of the 100 Hz ( 2 x 50 HZ net frequency Europe ), so about 100µsec. This pulse is long enough to trigger the triac. 
The delay between sybc pin and the PWM pulse is scaled from 0-> 1000. 0.1 percent per step.
See API MCPWM : https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/mcpwm.html
The mcpwm_sync_enable() API : specifies delay between zero detect and pwm pulse.
As the zero detect has a small delay, specifying phase delay of 100% ( 180 degrees ) will get into the next quadrant and drive fully, while it should be 0 !


This has shown to be enough to trigger the triac
Schematic can be found here : https://easyeda.com/lieven.merckx/triac-module
Errata : see notes in schematic before creating any PCB !

The schematic doesn't contain the tacho generator yet. 

Next step : tacho measure. 

## Stepper Motor with Magnetic angle detector
## DC Bike motor with rotation encoder