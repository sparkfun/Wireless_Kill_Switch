/*
 Remote Kill Switch - Hand Held Controller
 By: Nathan Seidle
 SparkFun Electronics
 Date: March 23rd, 2016
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 This is the part of the remote kill switch that the user holds in their hand.

 If red button pressed we send 'R' (kill). Turn on red LED.
 If yellow button pressed we send 'Y' (pause). Turn on yellow LED.
 If green button pressed we send 'G' (go). Turn on green LED.
 Send the system state every CHECKIN_PERIOD ms. Car turns off if nothing is received after MAX_TIME_WITHOUT_OK ms.

 Indicator LEDs: Green/Yellow/Red

 Measured current: 42mA roughly

*/

#include <SPI.h>
#include <RH_RF69.h>
#include <SimpleTimer.h> //https://github.com/jfturcot/SimpleTimer
#include <avr/wdt.h> //We need watch dog for this program

RH_RF69 rf69;

SimpleTimer timer;
long secondTimerID;

#define BUTTON_RED 9
#define BUTTON_GND 8
#define BUTTON_GRN 7
#define BUTTON_YLW 6

#define LED_RED 5
#define LED_YLW 4
#define LED_GRN 3

//Define the various system states
#define RED 'R'
#define YELLOW 'Y'
#define GREEN 'G'
#define DISCONNECTED 'D'

//Number of milliseconds between broadcasting our system state to the vehicle
//L defines the value as a long. Needed for millisecond times larger than int (+32,767) but doesn't hurt to have.
//25ms is good.
#define CHECKIN_PERIOD 25L

//Number of milliseconds to block for sending packets and waiting for the radio to receive repsonse packets
//This should be not be longer than the CHECKIN_PERIOD
//10ms is good. 
#define BLOCKING_WAIT_TIME 10L

//How many failed responses should be allowed from car until we go into disconnect mode
#define MAX_DELIVERY_FAILURES 3
byte failCount = 0;

char systemState;

unsigned long lastBlink = 0;
#define BLINK_RATE 500 //Amount of milliseconds for LEDs to toggle when disconnected

void setup()
{
  wdt_reset(); //Pet the dog
  wdt_disable(); //We don't want the watchdog during init

  Serial.begin(9600);

  pinMode(BUTTON_RED, INPUT_PULLUP);
  pinMode(BUTTON_YLW, INPUT_PULLUP);
  pinMode(BUTTON_GRN, INPUT_PULLUP);
  pinMode(BUTTON_GND, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YLW, OUTPUT);
  pinMode(LED_GRN, OUTPUT);

  digitalWrite(BUTTON_GND, LOW);

  secondTimerID = timer.setInterval(CHECKIN_PERIOD, checkIn); //Call checkIn every 500ms

  if (!rf69.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  // No encryption

  if (!rf69.setFrequency(915.0))
    Serial.println("setFrequency failed");

  // If you are using a high power RF69, you *must* set a Tx power in the range 14 to 20 like this:
  rf69.setTxPower(20);

  //This key is the same on the car. Pick your own random sequence.
  uint8_t key[] = { 0xAB, 0x1C, 0x0E, 0x39, 0xF8, 0xFF, 0xA6, 0xFC,
                    0x7B, 0x44, 0xC3, 0xC0, 0x2D, 0x2D, 0x2D, 0xD2
                  };
  rf69.setEncryptionKey(key);

  systemState = RED; //On power up start in red state
  setLED(LED_RED);

  Serial.println("Remote Controller Online");

  wdt_reset(); //Pet the dog
//  wdt_enable(WDTO_1S); //Unleash the beast
}

void loop()
{
  timer.run(); //Update any timers we are running
  wdt_reset(); //Pet the dog

  if (digitalRead(BUTTON_RED) == HIGH) //Top priority (Red is NC to ground so high = pressed)
  {
    systemState = RED;
    setLED(LED_RED); //Turn on LED

    //Check the special case of hitting all three buttons
    if (digitalRead(BUTTON_YLW) == LOW && digitalRead(BUTTON_GRN) == LOW) shutDown(); //Go into low power sleep mode
  }
  else if (digitalRead(BUTTON_YLW) == LOW)
  {
    systemState = YELLOW;
    setLED(LED_YLW); //Turn on LED
  }
  else if (digitalRead(BUTTON_GRN) == LOW)
  {
    systemState = GREEN;
    setLED(LED_GRN); //Turn on LED
  }
}

//Powers down all LEDs, radio, etc and sleeps until button interrupt
void shutDown()
{
  //Turn off LEDs
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YLW, LOW);
  digitalWrite(LED_GRN, LOW);

  //Turn off radio

  //Sleep microcontroller and wake up on button interrupt

  Serial.println("Powering down");
}

//Send the system status notification every CHECKIN_PERIOD number of ms
void checkIn()
{
  wdt_reset(); //Pet the dog

  if(systemState == RED)
  {
    sendPacket("R");
  }
  else if (systemState == YELLOW)
  {
    sendPacket("Y");
  }
  else if (systemState == GREEN)
  {
    sendPacket("G");
  }
  else if (systemState == DISCONNECTED)
  {
    if(millis() - lastBlink > BLINK_RATE)
    {
      lastBlink = millis();
      digitalWrite(LED_RED, !digitalRead(LED_RED));
      digitalWrite(LED_YLW, !digitalRead(LED_YLW));
      digitalWrite(LED_GRN, !digitalRead(LED_GRN));

      sendPacket("D"); //Attempt to re-establish connection
    }
  }
}

//Sends a packet
//If we fail to send packet or fail to get a response, time out and go to DISCONNECTED system state
void sendPacket(char* thingToSend)
{
  Serial.print("Sending: ");
  Serial.println(thingToSend);
  
  rf69.send((uint8_t*)thingToSend, sizeof(thingToSend));

  rf69.waitPacketSent(BLOCKING_WAIT_TIME); //Wait for a bit of time

  //Wait for a "O" response from the car
  boolean responseFromCar = rf69.waitAvailableTimeout(BLOCKING_WAIT_TIME); //Wait some ms time to get a response

  //Read in any response from car
  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  if (rf69.recv(buf, &len))
  {
    Serial.print("Heard from car: ");
    Serial.println((char*)buf);
  }

  if(responseFromCar == true) //We got a response
  {
    failCount = 0; //Reset the count
    
    if(systemState != DISCONNECTED)
    {
      Serial.println("System status delivered");
    }
    else if(systemState == DISCONNECTED)
    {
      //We are back online!
      Serial.println("Back online!");
      setLED(LED_RED);
      systemState = RED; //Default to stop if we are regaining connection
    }
  }
  else if (responseFromCar == false)
  {
    Serial.println("No response from car");

    //Go into triple blink mode indicating disconnect
    if(systemState != DISCONNECTED)
    {
      if(failCount++ > MAX_DELIVERY_FAILURES)
      {
        failCount = MAX_DELIVERY_FAILURES; //Don't let it increase and roll over
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_YLW, HIGH);
        digitalWrite(LED_GRN, HIGH);
        systemState = DISCONNECTED;
      }
    }

    rf69.setModeIdle(); //This clears the buffer so that rf69.send() does not lock up
  }
}

//Turns on a given LED
void setLED(byte LEDnumber)
{
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YLW, LOW);
  digitalWrite(LED_GRN, LOW);

  digitalWrite(LEDnumber, HIGH);
}
