/* Controller Code
 *
 * Author: Jim Franklin
 * Date:   20/01/2017
 */

#include <LiquidCrystal_I2C.h>
#include <stdlib.h>
#include <EEPROM.h>
//---------------------------------------------------------------------
// Prototypes
//---------------------------------------------------------------------
void SetupController(); 
void Normal();
void FanTimerReset();
void FanTimerClear();
void SetupController();
uint8_t GetTemp();
void FanON( uint8_t speed );
void FanOFF( uint8_t manual );
void FanUP( uint8_t roll );
void FanToggle();
void LedToggle();
void Show( uint8_t row, uint8_t col, String strValue );
double GetTempReal();
uint8_t FanTimerPassed();
uint8_t SetupPassed();
uint8_t Get( uint8_t varToGet);
void Set( uint8_t varToGet, uint8_t value );
void ChangeVar( uint8_t mode, uint8_t dir );

//---------------------------------------------------------------------

LiquidCrystal_I2C lcd( 0x3F, 16, 2 );		// Address, Chars, Lines

#define button1_Pin 2		// button 1 is on line 2
#define button2_Pin 3		// button 2 is on line 3 
#define Temp_Pin A0			// NTC is online A0 with a pulldown of 10k to GND
#define ON 1			
#define OFF 0
#define Fan_Pin 9			// Fan output is on line 9
#define Led_Pin 7			// LED lighting output is on line 9

// Defines and array for the config values
#define VALUE 0
#define MIN 1
#define MAX 2
#define UP 1
#define DOWN 2

#define vcheck 0					// EEPROM validity check
#define vmaxTemp 1					// Fan ON temp
#define vminTemp 2					// Fan OFF temp
#define vminDelta 3					// min temp drop in deltaTime
#define vmaxDelta 4					// max temp drop in deltaTime
#define vdeltaTime 5				// temp change check time
#define vfanMode 6					// 0 = Full,  1= Proportional
#define vdisplay 7					// 0 = C, 1 = F
#define vMAXVAR ( vfanMode + 1 )
uint8_t varList[ vMAXVAR ][ 3 ];    // value, min, max

enum fanModes_T {
  FullSpeed,
  Proportional
};
fanModes_T fanModes;

uint8_t fanSpeed = 0;          // signed int
uint8_t fanStatus = OFF;
uint8_t button1;
uint8_t button2;
uint8_t ledStatus = OFF;
uint8_t currentTemp;
uint8_t temperatureA = 0;
uint16_t FanTimer = 0;      // millis /�1000 = seconds
uint8_t fanOverride = false;
uint8_t setupMode = 0;
uint8_t setupTimeout = 5;
uint16_t setupTimer = 0;
uint8_t oldVar = 0;
//---------------------------------------------------------------------
void setup() {
	Serial.begin( 9600 );
	DEBUG("Starting up v1.0");

	lcd.begin();
	lcd.clear();
	lcd.backlight();
	lcd.noAutoscroll();
	lcd.print( "Starting up v1.0" );


	pinMode( button1_Pin, INPUT_PULLUP );
	pinMode( button2_Pin, INPUT_PULLUP );

	pinMode( Fan_Pin, OUTPUT );
	pinMode( Led_Pin, OUTPUT );
	pinMode( Temp_Pin, INPUT ); // analog

	FanTimerClear();
	fanSpeed = 0; // PWM Off
	fanStatus = OFF;
	ledStatus = OFF;

	LoadSettings();

  digitalWrite( Led_Pin, LOW );
  digitalWrite( Fan_Pin, LOW );

	button1 = digitalRead( button1_Pin );
 
	lcd.clear();

	if( button1 == OFF ) {
		InitInterrupts();
		SetupController();
	} else {
		InitInterrupts();
    fanStatus = OFF;
    ledStatus = OFF;
//   varList[ vfanMode ][ VALUE ] = 1;        //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
		Normal();
	}
}

//---------------------------------------------------------------------
void InitInterrupts() {
  attachInterrupt( digitalPinToInterrupt( button1_Pin ), Button1_Int, FALLING );
  attachInterrupt( digitalPinToInterrupt( button2_Pin ), Button2_Int, FALLING );
}

//---------------------------------------------------------------------
void LoadSettings() {
	uint8_t adr = 0;
	// check magic settings at EE positions 0,1,2  (0x55, 0xAA, 0x55)(85, 170, 85)
	varList[ vcheck ][ VALUE ] 	= EEPROM.read( 0 );
	varList[ vcheck ][ MIN ] 		= EEPROM.read( 1 );
	varList[ vcheck ][ MAX ] 		= EEPROM.read( 2 );

	Serial.print( "Magic Value =" );
	Serial.print( varList[ 0 ][ VALUE ], DEC );
	Serial.print( varList[ 0 ][ MIN ], DEC );
	Serial.println( varList[ 0 ][ MAX ], DEC );

  //varList[ vcheck ][ VALUE ]   =  0 ; // force load <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

	// check the golden 55 AA 55 in first 3 EE locations
  	if( varList[ vcheck ][ VALUE ] == 0x55 && varList[ vcheck ][ MIN ] == 0xAA && varList[ vcheck ][ MAX ] == 0x55 ) {
    	DEBUG( "Reading settings from EEPROM" );
    	adr = 0;
    	for( uint8_t ind = 0; ind < vMAXVAR; ind++ ) {
      		adr = ind * 3;
      		varList[ ind ][ VALUE ] = EEPROM.read( adr );
      		varList[ ind ][ MIN ]   = EEPROM.read( adr + 1 );
      		varList[ ind ][ MAX ]   = EEPROM.read( adr + 2 );
    	}
  	} else  {
    	DEBUG("Writing default settings to EEPROM");
	    // write Defaults
	    varList[ 0 ][ VALUE ] 		  	= 0x55;	varList[ 0 ][ MIN ] 		= 0xAA; varList[ 0 ][ MAX ] 		= 0x55;
	    varList[ vmaxTemp ][ VALUE ]  	= 28;  	varList[ vmaxTemp ][ MIN ]  = 10;   varList[ vmaxTemp ][ MAX] 	= 60;
	    varList[ vminTemp ][ VALUE ]  	= 25;  	varList[ vminTemp ][ MIN ] 	= 10;   varList[ vminTemp ][ MAX] 	= 60;
	    varList[ vminDelta ][ VALUE ] 	= 2;  	varList[ vminDelta ][ MIN ] = 1;    varList[ vminDelta ][ MAX] 	= 10;
	    varList[ vmaxDelta ][ VALUE ] 	= 4;  	varList[ vmaxDelta ][ MIN ] = 1;    varList[ vmaxDelta ][ MAX] 	= 10;
	    varList[ vdeltaTime ][ VALUE ]  = 60;	varList[ vdeltaTime ][ MIN ]= 10;	varList[ vdeltaTime ][ MAX] = 60;
	    varList[ vfanMode ][ VALUE ]	= 0;   	varList[ vfanMode ][ MIN ] 	= 0;    varList[ vfanMode ][ MAX] 	= 1;
	    varList[ vdisplay ][ VALUE ]	= 0;   	varList[ vdisplay ][ MIN ] 	= 0;    varList[ vdisplay ][ MAX] 	= 1;

    	for(uint8_t ind = 0; ind < vMAXVAR; ind++) {
      		adr = ind * 3;
      		EEPROM.update( adr, 		varList[ ind ][ VALUE ]);
      		EEPROM.update( adr + 1, 	varList[ ind ][ MIN ]);
      		EEPROM.update( adr + 2, 	varList[ ind ][ MAX ]);
    	}
  	}
}

//---------------------------------------------------------------------
void DEBUG( String strToSend ) {
	Serial.println( strToSend );
}
//---------------------------------------------------------------------
void loop() {
  	Normal();
}

//---------------------------------------------------------------------
// Button 1 - upper button - FAN or +++   hold to enter setup on boot
//---------------------------------------------------------------------
void Button1_Int()	{
	static unsigned long last_interrupt_time1 = 0;
  	unsigned long interrupt_time = millis();
                 
  	if( interrupt_time - last_interrupt_time1 > 200 ) { // If interrupts come faster than 200ms, assume it's a bounce and ignore
    	if( setupMode == 0) {
        	if( Get( vfanMode ) == Proportional ) {       // not setup, but normal mode
           // Serial.println("Proportional Fan UP with toggle");
        		FanUP( true );                              // true = allow wrap to 0, false= stop at 100
        	} else {
          		FanToggle();
          		fanOverride = ( fanStatus == ON ) ? true : false;
        	}
     	} else {
        	ChangeVar( setupMode, UP );
     	}
  	}
  	last_interrupt_time1 = interrupt_time;
}
//---------------------------------------------------------------------
// Button 2 - lower button - LED or ---
//---------------------------------------------------------------------
void Button2_Int() {
  	static unsigned long last_interrupt_time2 = 0;
  	unsigned long interrupt_time = millis();
                  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  	if ( interrupt_time - last_interrupt_time2 > 200 ) {
    	if( setupMode == 0 ) {       // not setup, but normal mode
        	LedToggle();
    	} else {
      	ChangeVar( setupMode, DOWN );
    	}
  	}
  	last_interrupt_time2 = interrupt_time;
}

//---------------------------------------------------------------------
// Change the variable up or down subject to the max/min for that var
//---------------------------------------------------------------------
void ChangeVar( uint8_t mode, uint8_t dir ) {
    uint8_t var = Get(mode);  // get the variable being edited
    oldVar = var;

    if( dir == UP ) {
      var++;
    } else {
      var--;
    }

    if( var > varList[ mode ][ MAX ] ) 	var = varList[ mode ][ MAX ];
    if( var < varList[ mode ][ MIN ] || var == 0) var = varList[ mode ][ MIN ];

    
    Set( mode, var );   // varList[mode][VALUE] = var;  
    SetupReset();   // reset the timer
}



//---------------------------------------------------------------------
// Configure the controller
//---------------------------------------------------------------------

void SetupController() {
  	const char *names[ 7 ];
	names[ vmaxTemp ] = "Fan On Temp";
	names[ vminTemp ] = "Fan Off Temp";
	names[ vminDelta ] = "Min Delta";
	names[ vmaxDelta ] = "Max Delta";
	names[ vdeltaTime ] = "Delta Time";
	names[ vfanMode ] = "Fan Full/Prop";
	names[ vdisplay ] = "Display C / F";
	Show( 1, 1,  "Setup Mode" );
	delay( 2000 );                // wait secs
	lcd.clear();

  	for( setupMode = 1; setupMode < vMAXVAR; setupMode++ ) {
    	lcd.clear();
    	Show( 1, 1, "+" );
    	ShowFromArr( 1, 3, names[ setupMode ] );
    	ShowNum( 2, 5, Get( setupMode ) );
	    Show( 2, 1, "-" );
	    SetupReset();
	    while( SetupPassed() < setupTimeout ) {
      
      	uint8_t currentVal = Get( setupMode );
      	if( oldVar != currentVal ) {
	        Show( 2, 5, "   " );
        	ShowNum( 2, 5, currentVal );
        	oldVar = currentVal;
      	}
    	}  
  	}
 
  	// Write the updates
  	for( uint8_t ind = 0; ind < vMAXVAR; ind++ )  {
	  	EEPROM.update( ind, varList[ ind ][ VALUE ] );
  	}	

	Show( 1, 1, "Setup Complete" );
	Show( 2, 1, "Return to Normal" );
	delay( 2000 );
	lcd.clear();
	return;
}

//---------------------------------------------------------------------
// Get/Set the specified value from/to the list
//---------------------------------------------------------------------
uint8_t Get( uint8_t varToGet ) {
  return varList[ varToGet ][ VALUE ];
}
//---------------------------------------------------------------------
void Set(uint8_t varToSet, uint8_t value) {
  varList[ varToSet ][ VALUE ] = value;
}
//---------------------------------------------------------------------
// Setup Timers
//---------------------------------------------------------------------
void SetupReset() {
  setupTimer = millis() / 1000;
}

//---------------------------------------------------------------------
void SetupClear() {
  setupTimer = 0;
}

//---------------------------------------------------------------------
uint8_t SetupPassed() {
    if( setupTimer != 0 )  {
    	return ( millis() / 1000 ) - setupTimer;
  	} else  {
    	return 0;
  	}
}

//---------------------------------------------------------------------
void Normal() {
	while(1) {
		currentTemp = GetTemp();
		Show( 1, 7,  "Temp" );
		Show( 1, 16, "c" );

		uint8_t displayTemp  = currentTemp;

		if( Get(vdisplay) == 1 ) {
			float f  = ( currentTemp * 1.8 ) + 32;
			displayTemp = (uint8_t) f;
		}

		ShowNum( 1, 13, displayTemp );


		if( fanStatus == ON ) {
		  Show( 1, 1, "FAN" );
		} else {
		  Show( 1, 1, "fan" );
		}

		Show( 2, 7,  "Fan" );
		Show( 2, 12, ( fanStatus == ON ) ? "On " : "Off     " );

    
		if( (fanStatus == ON) && (Get( vfanMode ) == Proportional ) ) {
			Show( 2, 13, "   " );		// clean up in case it had 100 in there previously
			ShowNum( 2, 13, fanSpeed );
			Show( 2, 16, "%" );
		}

		if(ledStatus == ON) {
			Show(2,1, "LED");
		} else {
			Show( 2, 1, "led");
		}
    
		currentTemp = GetTemp();
      
		switch( Get( vfanMode ) )  {
    		case FullSpeed:
    			if( currentTemp >= Get( vmaxTemp ) ) FanON( 100 );
    			if( currentTemp <= Get( vminTemp ) ) FanOFF( false );
    			break;

    		case Proportional:
    			if( currentTemp >= Get( vmaxTemp ) )  {
    				switch( fanStatus ) 	{
    					case OFF:
    						FanON( 33 );              // initialise at 33%
    						temperatureA = currentTemp;
    						FanTimerReset();
    						break;

    					case ON:
    						if( FanTimerPassed() >= 60) {          // ok been on for 1 min
    							uint8_t newTemp = GetTemp();
    							if( abs( newTemp - temperatureA ) < Get( vminDelta ) ) { // if not cooled enough, speed up a notch
    								if( fanSpeed < 99 ) {
    									FanUP( false );
    								}
    							} else if( abs( newTemp - temperatureA ) > Get( vmaxDelta ) ) { // if cooled too much slow down a notch
    								if( fanSpeed > 33 ) {
    									FanDOWN( false );
    								}
    							}

    							temperatureA = newTemp;
    							FanTimerReset();
    						}
    						break;

    					default:
    						break;
    				}   // end of switch(fanstatus)
    			} else {
    				if(currentTemp <= Get( vminTemp) ){
    					FanOFF(false);
    					FanTimerClear();    // off
    				}
    			}
    			break;

    		default:
    			break;
		} // end of switch(fanmode)
	}	// end of while(1)
}

//---------------------------------------------------------------------
void ShowFromArr( uint8_t row, uint8_t col, const char *strValue ) {
  	lcd.setCursor( col - 1, row - 1 );
  	lcd.print( strValue );
}

//---------------------------------------------------------------------
void Show( uint8_t row, uint8_t col, String strValue ) {
  	lcd.setCursor( col - 1, row - 1 );
  	lcd.print( strValue );
}

//---------------------------------------------------------------------
void ShowNum( uint8_t row, uint8_t col, uint8_t iValue ) {
    lcd.setCursor( col - 1, row - 1 );
    lcd.print( String( iValue, DEC ) );
}

//---------------------------------------------------------------------
void FanTimerReset() {
  	FanTimer = millis() / 1000;
}

//---------------------------------------------------------------------
void FanTimerClear() {
  	FanTimer = 0;
}

//---------------------------------------------------------------------
uint8_t FanTimerPassed() {
	if( FanTimer != 0 )   {
		return ( millis() / 1000 ) - FanTimer;
	} else {
		return 0;	
	}
}

//---------------------------------------------------------------------
uint8_t GetTemp() {
	double temp = GetTempReal();
	return (uint8_t)temp;
}

//---------------------------------------------------------------------
// Turn the Fan ON with a speed (PWM) as stated
//---------------------------------------------------------------------
void FanON( uint8_t speed ) {
	if( Get( vfanMode ) == FullSpeed )   {
    	digitalWrite( Fan_Pin, HIGH );
  	} else {
   		fanSpeed = speed;
    	analogWrite( Fan_Pin, speed );
  	}
  	fanStatus = ON;
}

//---------------------------------------------------------------------
// Turn the Fan OFF (PWM =0)
//---------------------------------------------------------------------
void FanOFF( uint8_t manual ) {
  if( !fanOverride || manual ) {
    if( Get( vfanMode ) == FullSpeed )	{
      digitalWrite( Fan_Pin, LOW );
    } else {
      fanSpeed = 0;
      analogWrite( Fan_Pin, 0 );
    }
    fanStatus = OFF;
 	}
}

//---------------------------------------------------------------------
// Increase fan speed, ignore if not proportional fan
// roll: true = allow wrap to 0, false= stop at 100
//---------------------------------------------------------------------
void FanUP( uint8_t roll ) {
    if( Get( vfanMode ) == Proportional) {
      fanSpeed +=33;
      
      if( roll == true ) {
        if( fanSpeed > 99 ) fanSpeed = 0;
      } else {
       	if( fanSpeed > 99 ) fanSpeed = 100;
      }
      
      Serial.println(fanSpeed, DEC);
    }
}

//---------------------------------------------------------------------
// Decrease fan speed, ignore if not proportional fan
// roll: true = allow wrap to 99, false= stop at 0
//---------------------------------------------------------------------
void FanDOWN( uint8_t roll ) {
  if( Get( vfanMode ) == Proportional ) {
    if(fanSpeed != 0) {
      fanSpeed -= 33;
    } else {
      if( roll == true ) {
     	   fanSpeed = 99;
      }
    }
  }
}

//---------------------------------------------------------------------
// Toggle the Fan
//---------------------------------------------------------------------
void FanToggle() {
    if( fanStatus == ON ) {
    	FanOFF( true );		// manual OFF
    } else {
    	FanON( fanSpeed );
    }
}

//---------------------------------------------------------------------
// Toggle the LED Lighting
//---------------------------------------------------------------------
void LedToggle() {
    digitalWrite( Led_Pin,  (ledStatus == ON ) ? LOW : HIGH);
    ledStatus = !ledStatus;
}

//---------------------------------------------------------------------
// Inputs ADC Value from Thermistor and outputs Temperature in Celsius
//---------------------------------------------------------------------
double GetTempReal() {
  int RawADC = analogRead( Temp_Pin );

  long Resistance;
  double Temp;

  // Assuming a 10k Thermistor.  Calculation is actually: Resistance = (1024/ADC)
  Resistance=( ( 10240000 / RawADC ) - 10000 );

  /******************************************************************/
  /* Utilizes the Steinhart-Hart Thermistor Equation:        */
  /*    Temperature in Kelvin = 1 /� {A + B[ln(R)] + C[ln(R)]^3}   */
  /*    where A = 0.001129148, B = 0.000234125 and C = 8.76741E-08  */
  /******************************************************************/
  Temp = log( Resistance );
  Temp = 1 / ( 0.001129148 + ( 0.000234125 * Temp ) + ( 0.0000000876741 * Temp * Temp * Temp ) );
  Temp = Temp - 273.15;  // Convert Kelvin to Celsius

  // Temp = (Temp * 9.0)/ 5.0 + 32.0; // Convert to Fahrenheit
  return Temp;  // Return the Temperature
}



