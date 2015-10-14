/*
*	StellaCam controller using an Arduino Yun
*	Dallan Porter
*	MMT Observatory, University of Arizona
*	Oct 13 2015
*
*	
The MIT License (MIT)

Copyright (c) 2105 MMT Observatory, University of Arizona

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#include <Console.h>
#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define DAC_RESOLUTION    (12)
#define MAX_BUFFER_LENGTH  100
#define DEBUG true

/*********************************
StellaCam Arduino REST interface
----------------------------------
Set the framerate:
/frames/frame_<n>    where n=0,1,2,4,8,16,32,64,128,256

Set the freeze frame on or off:
/freeze/<value>      where value = on or off

Set the gain value:
/gain/<value>	     where value is an integer value between 0 and 100 (percent)

Set the gamma value:
/gamma/<value>	     where value is a string value of 'off', 'low' or 'hi'

Set the iris open or ctl
/iris/<value>	     where value is a string value of 'open' or 'ctl'
*********************************/
Adafruit_MCP4725 dac;
int pin_Black = 10;
//int pin_RedAndWhite = 0; // Ground
int pin_BrownAndWhite = A3;
//int pin_BlackAndWhite = 0; // Ground
int pin_White = 9;
int pin_Yellow = A1;
int pin_Green = 8;
//int pin_Blue = 0; // 5V
//int pin_Pink = 0; // Vin
int pin_Orange = A2;
int pin_Brown = 3;
int pin_Red = A0;
int pin_Mint = 4;
int pin_Gray = 5;
int pin_Purple = 6;
int pin_LightBlue = 7;

YunServer server;

typedef enum GammaMode {
  gamma_off, gamma_low, gamma_hi 
};

typedef enum State {
    state_command, state_value
} State;

typedef enum Frames {
  frames_off, frames_1, frames_2, frames_4, frames_8,
  frames_16, frames_32, frames_64, frames_128, frames_256 
};

void setup() {
  Bridge.begin();
  Console.begin(); // So we can log into the Yun and see the Arduino serial output.
  
  dac.begin(0x62); //For Adafruit MCP4725A1 the address is 0x62 (default)
  server.listenOnLocalhost();
  server.begin();
  
  configurePins();
  resetCamera();
}

void loop() {
  YunClient client = server.accept();
  
  if( client )
  {
    process( client );
    client.stop(); 
  }
  
  delay( 50 );
}

// This is where we grab the command stream from the webserver and use that to
// get the command name and the value from the URL in the form http://<ipaddress>/arduino/<command>/<value>
// It will then call the appropriate function to set the camera accordingly. It also sends back a JSON string 
// indicating a success or failure.
void process( YunClient client )
{
  if( DEBUG && Console )
  {
    Console.println( "Processing client request..." );
  }
  
  char raw_command[ MAX_BUFFER_LENGTH * 2 ];
  int raw_length = 0;
  char delimiter = '/';
  
  char buffer[ MAX_BUFFER_LENGTH ];
  char cmd[ MAX_BUFFER_LENGTH ];
  char value[ MAX_BUFFER_LENGTH ];
  int cmd_length = 0;
  int value_length = 0;
  State current_state = state_command;
  int index = 0;
   
  while( client.available() > 0 )
  {
    char c = client.read();
    if( c == 0xA || c == 0xD )
    {
        continue;
    }
    if( current_state == state_command && c == '/' )
    {
        current_state = state_value;
        continue; 
    }
    
    if( DEBUG && Console )
    {
      Console.print( " 0x" );
      Console.print( c, HEX );
      
    }
    
    if( current_state == state_command )
    {
        cmd[ cmd_length++ ] = c;
        if( cmd_length > MAX_BUFFER_LENGTH )
        {
           // Overflow
           if( DEBUG && Console )
           {
               Console.println( "Command buffer overlow!");
               return; 
           }
        } 
    }
    else if( current_state == state_value )
    {
        value[ value_length++ ] = c;
        if( value_length > MAX_BUFFER_LENGTH )
        {
            if( DEBUG && Console )
            {
                Console.println( "Value buffer overlow!");
                return; 
            }
        } 
    }
    
  }
  
  // Null terminate the strings.
  cmd[ cmd_length++ ] = '\0';
  value[ value_length++ ] = '\0';
    
  if( cmd != NULL && DEBUG && Console )
  {
      Console.print( "Command:" );
      Console.print( cmd );
      Console.print( "--\n" );
      
       for( int i=0; i<cmd_length; i++)
      {
         Console.print("0x");
         Console.print( cmd[i], HEX );
         Console.print( "\n" );
      }
  }
  if( value != NULL && DEBUG && Console )
  {
      Console.print( "Value:" );
      Console.print( value );
      Console.print( "--\n" );
      
      for( int i=0; i<value_length; i++)
      {
         Console.print("0x");
         Console.print( value[i], HEX );
         Console.print( "\n" );
      }
      
  }
  
  // Now parse the command and value and send the commands to the camera.
  // Probably an elegant way to do this with bit shifting, but I'll do it
  // it the long way today.
  if( strcmp( cmd, "frames" ) == 0 )
  {
      if( strcmp( value, "frames_0" ) == 0 )
      {
          setFrames( frames_off );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_1" ) == 0 )
      {
          setFrames( frames_1 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_2" ) == 0 )
      {
          setFrames( frames_2 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_4" ) == 0 )
      {
          setFrames( frames_4 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_8" ) == 0 )
      {
          setFrames( frames_8 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_16" ) == 0 )
      {
          setFrames( frames_16 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_32" ) == 0 )
      {
          setFrames( frames_32 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_64" ) == 0 )
      {
          setFrames( frames_64 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_128" ) == 0 )
      {
          setFrames( frames_128 );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "frames_256" ) == 0 )
      {
          setFrames( frames_256 );
          sendResponse( client, 200, "OK" );
      }
      else
      {
           sendResponse( client, 500, "Error. Invalid frame value." );
      }
  }
  else if( strcmp( cmd, "freeze" ) == 0 )
  {
       if( strcmp( value, "on" ) == 0 )
       {
           setFreeze( true );
           sendResponse( client, 200, "OK" );
       }
       else if( strcmp( value, "off" ) == 0 )
       {
           setFreeze( false );
           sendResponse( client, 200, "OK" );
       }
       else
       {
           sendResponse( client, 500, "Error. Invalid freeze value." );
       }
  }
  else if( strcmp( cmd, "gain" ) == 0 )
  {
      int int_val = atoi( value );
      if( DEBUG && Console )
      {
          Console.print( "Gain int value = " );
          Console.print( int_val );
          Console.print( "\n" ); 
      }
      if( int_val >= 0 && int_val <= 100 )
      {
          setGain( int_val );
          sendResponse( client, 200, "OK" );
      }
      else
      {
          sendResponse( client, 500, "Error. Invalid gain value." );
      }
  }
  else if( strcmp( cmd, "gamma" ) == 0 )
  {
      if( strcmp( value, "off" ) == 0 )
      {
          setGamma( gamma_off );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "low" ) == 0 )
      {
          setGamma( gamma_low );
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "hi" ) == 0 )
      {
          setGamma( gamma_hi );
          sendResponse( client, 200, "OK" );
      }
      else
      {
          sendResponse( client, 500, "Error. Invalid gamma value." );
      }
  }
  else if( strcmp( cmd, "iris" ) == 0 )
  {
      if( strcmp( value, "open" ) == 0 )
      {
          setIrisOpen();
          sendResponse( client, 200, "OK" );
      }
      else if( strcmp( value, "ctl" ) == 0 )
      {
          setIrisCtrl();
          sendResponse( client, 200, "OK" );
      }
      else
      {
          sendResponse( client, 500, "Error. Invalid iris value." );
      }
  }
  else
  {
      sendResponse( client, 500, "Error. Invalid command." );
  }
 
}

void sendResponse( YunClient client, int status_code, char* message )
{
  if( client != NULL )
  {
     client.print( "Status: " );
     client.print( status_code );
     client.print( "\n" );
     client.println( "Content-type: application/json" );
     
     client.println( ); // manditory blank line
     client.print( "{ status:" );
     client.print( status_code );
     client.print( ", message:\"" );
     client.print( message );
     client.print( "\" }" );  
  }
}

void configurePins()
{
 pinMode( pin_Black,  OUTPUT );
 pinMode( pin_BrownAndWhite, OUTPUT );
 pinMode( pin_White,  OUTPUT );
 pinMode( pin_Yellow, OUTPUT );
 pinMode( pin_Green,  OUTPUT );
 pinMode( pin_Orange, OUTPUT );
 pinMode( pin_Brown,  OUTPUT );
 pinMode( pin_Red,    OUTPUT );
 pinMode( pin_Mint,   OUTPUT );
 pinMode( pin_Gray,   OUTPUT );
 pinMode( pin_Purple, OUTPUT );
 pinMode( pin_LightBlue, OUTPUT );
 
}

void resetCamera()
{
 // Freeze Frame
 setFreeze( false );
 
 // IRIS control
 setIrisCtrl();

 // GAMMA
 setGamma( gamma_off );
 
 // GAIN
 setGain( 0 );

 // Frames
 setFrames( frames_off ); // 0 = off
 
}

void setFreeze( bool do_freeze )
{
  if( do_freeze )
  {
     digitalWrite( pin_BrownAndWhite, 1 ); 
  }
  else
  {
    digitalWrite( pin_BrownAndWhite, 0 );
  }
}

void setIrisOpen()
{
  digitalWrite( pin_Green, 0 );
}

void setIrisCtrl()
{
  digitalWrite( pin_Green, 1 );
}


void setGain( int percent )
{
  if( percent < 0 )
  {
   percent = 0; 
  }
  else if( percent > 100 )
  {
   percent = 100; 
  }
  // We go from 0V -> 3.27V
  // Our range on the analog is 0-5V
  int max_int = 2678; // 3.27V (3.27/5 * 4096)
  float p = (float)percent * 0.01;
  int val = max_int * p;
  if( DEBUG && Console )
  {
      Serial.print( "gain percent = " );
      Serial.print( val );
      Serial.print( "\n" );
  }
  
  //analogWrite( pin_Brown, val ); 	// Nope. PWM does not work with the analog gain control.
  									// Results in a flashing screen. This could probably be
  									// worked around with some resistors and capacitors. 
  									// Luckily I had the Adafruit DAC on hand already.
  // Here we are using the Adafruit MCP4725 I2C DAC
  dac.setVoltage( val, false );
}

void setGamma( int mode )
{
  if( mode == gamma_off )
  {
    digitalWrite( pin_Black, 0 );
    digitalWrite( pin_White, 0 );
  }
  else if( mode == gamma_low )
  {
    digitalWrite( pin_Black, 1 );
    digitalWrite( pin_White, 1 );
  }
  else if( mode == gamma_hi )
  {
    digitalWrite( pin_Black, 0 );
    digitalWrite( pin_White, 1 );
  }
}

void setFrames( int number )
{
  if( DEBUG && Console )
  {
      Console.println( "**** Sending Frames command to the camera." ); 
  }
  
  if( number == frames_1 )
  {
    digitalWrite( pin_Mint,      1 );
    digitalWrite( pin_Gray,      0 );
    digitalWrite( pin_Purple,    1 );
    digitalWrite( pin_LightBlue, 0 );
  }
  else if( number == frames_2 )
  {
    digitalWrite( pin_Mint,      1 );
    digitalWrite( pin_Gray,      1 );
    digitalWrite( pin_Purple,    1 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else if( number == frames_4 )
  {
    digitalWrite( pin_Mint,      1 );
    digitalWrite( pin_Gray,      0 );
    digitalWrite( pin_Purple,    1 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else if( number == frames_8 )
  {
    digitalWrite( pin_Mint,      1 );
    digitalWrite( pin_Gray,      1 );
    digitalWrite( pin_Purple,     0 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else if( number == frames_16 )
  {
    digitalWrite( pin_Mint,      1 );
    digitalWrite( pin_Gray,      0 );
    digitalWrite( pin_Purple,    0 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else if( number == frames_32 )
  {
    digitalWrite( pin_Mint,      0 );
    digitalWrite( pin_Gray,      1 );
    digitalWrite( pin_Purple,    1 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else if( number == frames_64 )
  {
    digitalWrite( pin_Mint,      0 );
    digitalWrite( pin_Gray,      0 );
    digitalWrite( pin_Purple,    1 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else if( number == frames_128 )
  {
    digitalWrite( pin_Mint,      0 );
    digitalWrite( pin_Gray,      1 );
    digitalWrite( pin_Purple,    0 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else if( number == frames_256 )
  {
    digitalWrite( pin_Mint,      0 );
    digitalWrite( pin_Gray,      0 );
    digitalWrite( pin_Purple,    0 );
    digitalWrite( pin_LightBlue, 1 );
  }
  else
  {
    digitalWrite( pin_Mint,      1 );
    digitalWrite( pin_Gray,      1 );
    digitalWrite( pin_Purple,    1 );
    digitalWrite( pin_LightBlue, 0 );
  }
}

