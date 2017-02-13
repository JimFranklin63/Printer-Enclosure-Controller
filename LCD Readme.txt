Written for the Arduino UNO 
using an I2c 16x2 LCD Display



2 = Button 1 - Fan Control / hold on power up for setup mode / +++ in setup
3 = Button 2 - LED Control / --- in setup
7 = Lights Control output - MUST drive through a power transistor 
9 = Fan Control output - MUST drive through a power transistor 
A0= Temperature NTC input = also pulled to 0v by 10k resistor
A4= I2c SCL to LCD display
A5= I2c SDA to LCD display

