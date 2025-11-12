I want to use an IoT data logger board from sparkfun, with some qwiic connected sensors, to generate some time series data sets. 

The hardware I have is:
- Sparkfun's DataLogger IoT(no built in IMU) - DEV-22462
- SparkFun 6DoF IMU Breakout - ISM330DHCX (Qwiic) - SEN-19764
- SparkFun Qwiic Button - Red LED (Qwiic) - BOB-15932
- Useful Sensors Tiny Code Reader (Qwiic) - SEN-23352

This gives me a physical data generating device that I can then use for replay/analysis/etc.

Some requirements:
- I want to be able to 'frame' data (almost like an epoch) with start times or markers of some sort(maybe using the button or a QR code with a title). And an end time. And LED of some sort should indicate whether data is currently recording(ideally an onboard LED and not an external one)
- Record to microSD in short term
- (future) Transmit via MQTT
- (future) add additional sensors (also QWIIC)(GPS, RFID and VoC sensor)
- (future) Uses a QR code for configuration(SSID, passkey, MQTT broker config, etc)
- (future) uses an AdaFruit 'NeoPixel Stick - 8 x 5050 RGB LED with Integrated Drivers' for status indication


Example workflow:
1. Short Press Button
	- LED begins blinking
	- QR code reader is enabled
2. Scan QR code that encodes a JSON object with a test=(string) and labels=(array) properties
	- Start event is recorded/transmitted
    - Begins recording sensor data to MicroSD
    - (future) Begins transmitting sensor data via MQTT
    - LED is solid during recording
3. Short Press Button
    - Data Recording ends
    - (future) data transmission ends
    - LED is off when recording ends