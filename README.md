# Vibratory-weighing-filler
Vibratory filler is designed to fill small batches of free-flowing powders precisely. The product is designed with small batches in mind. The mechanism is supposed to be easier to clean than the auger filler machine.


## Code
### Display
It uses 128x64 pixel OLED. There are currently four windows:
- ### **First Page**
  - Top value shows set weight
  - Bottom value shows current weight

<img src="https://user-images.githubusercontent.com/4378608/165688870-980533e4-5af6-4818-8cb2-c575991bb505.jpg" width="100" height="100">



- **Set Weight**
  - Both whole and decimal part can be edited for quickly setting weights. Acceleration is also implemented in rotary encoder which reduces number of turns required.
<img src="https://user-images.githubusercontent.com/4378608/165688893-b7077d1a-de26-4541-9b74-40a95a0ebf22.jpg" width="100" height="100">


- **Preset Selection Page**
  - Preset allows user to choose different filling profile depending on powder
<img src="https://user-images.githubusercontent.com/4378608/165688914-92c2b5c3-eb68-491c-80ac-ff46555e2b6c.jpg" width="100" height="100">


### Powder filling
It uses both lookup table and PID controller to control speed of vibrator motor and gate opening. There is option to change lookup table using HMI. 


## Demo Video

https://user-images.githubusercontent.com/4378608/165664122-12178154-0821-4f16-8d1d-a91e1e030d97.mp4


### Top View
<img src="https://user-images.githubusercontent.com/4378608/165680280-6d1a402c-cab6-4ac9-9237-5fd8ae3a9aa6.jpg" height="250">

### HMI
<img src="https://user-images.githubusercontent.com/4378608/165680287-dcc59aba-ece7-4aab-98bd-6764c1806981.jpg" height="250">

### Dispenser
<img src="https://user-images.githubusercontent.com/4378608/165680289-3427a877-aed5-4dd4-a4c5-e9af98574080.jpg" height="250">

## Component list:
1. Stainless steel Pancake batter dispenser
2. ATmega2560 Rev3 development board
3. MG90S 9G Micro Mini Servo
4. 5V Micro RS-360 Vibration Motor
5. HX711 Load Cell Amplifier
6. Load cell 100g
7. Capacitive Touch Sensor
8. 128x64 pixel monochrome OLED display
9. Rotary Encoder
10. MOSFET Power Controller
11. Stainless Steel Threaded Rod M8
12. Steel Fixed Levelling Feet M8
13. Lock nut, Hex Nut, Rubber washers - M8
