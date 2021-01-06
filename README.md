# ESP32-R4ge-Pro
<p>
Software and resources for the ESP32 R4ge Pro custom badge. See documents folder for schematic.  
</p>

![alt text](https://raw.githubusercontent.com/DigiTorus86/ESP32-R4ge-Pro/master/images/esp32_pro_audio.jpg)

<h3>Key Components/Requirements</h3>
<p>
- <a href="https://www.pcbway.com/project/shareproject/ESP32_R4ge_Pro.html">Custom PCB</a><br>
- ESP32 DevKitC w/38 pins (other ESP32 boards can be used with minimal PIN changes)<br>
- ILI9341 320x240 TFT with touch and SD card slot<br>
- 2 Analog joysticks<br>
- PCM5102a I2S DAC w/headphone jack<br>
- PAM8403 amplifier and 8 Ohm/1W speakers and/or powered external speakers w/3.5mm jack<br>
- Microphone (i.e. ADMP401)<br>
- Some sketches have additional hardware requirements (components attached to I2C or Serial pins, or SD card)<br>
- See BOM file in the documents folder for full list of components.<br>
- STL file for printing a simple case also included in the documents folder.<br>
</p>
<h3>App Descriptions</h3>
<p>
- Audio:   Allows for WAV recording, play back, serialization, and frequency analysis.  Requires SD card for some functions.<br>
- BLE Controller:   Allows you to use the joysticks and buttons of the ESP32 R4ge Pro to control another ESP32/Bluetooth device operating in the central role, such as a robot tank.  
- Drummer: A 4-channel sequencer using percussion samples.<br>
- Gravitack:  Classic space shooter. (Port from original ESP32 Conference Badge) <br>
- Prong:   A simple pong-type game for 1 or 2 players.<br>
- Synth:   A graphical 4-operator/4-voice FM synth, playable via on-screen keyboard + stylus, 8 step sequencer, or MIDI input on the RX2 pin. 
- Test1:   Used to verify the operation of the board components and do any calibration (microphone/joysticks).<br> 
- Time and Weather:  Clock and weather station. (Port from original ESP32 Conference Badge)<br>
- Tombstone:  Port of the classic TI game.<br>    
- Web Radio:  Internet radio station selector and player.<br>
</p>
<p>
Check for additional info on my blog at https://twobittinker.com<br> 
</p>




