/*
 Remote Kill Switch - On the Car
 By: Nathan Seidle
 SparkFun Electronics
 Date: March 23rd, 2016
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 This is the part of the remote kill switch that lives on the car.

 Designed the for Arduino Pro Mini 3.3V / 8MHz (because the RFM69 is 3.3V)

 If we receive 'R' (kill) system turns off the relay.
 If we receive 'Y' (pause) from the remote then set pin PAUSE high to signal master
 computer to pause.
 If we receive 'G' (go) system turns on relay, sets PAUSE pin to low.
 If we don't receive a system status from the remote every MAX_TIME_WITHOUT_OK ms, system goes into safety shutdown (relay off).

 Locomotion controller requires a 5V FTDI. The kill switch requires a 3.3V FTDI.
*/

#include <SPI.h>
#include <RH_RF69.h> //From: http://www.airspayce.com/mikem/arduino/RadioHead/s
#include <SimpleTimer.h> //https://github.com/jfturcot/SimpleTimer
#include <avr/wdt.h> //We need watch dog for this program

//If we don't get ok after this number of milliseconds then go into safety-shutdown
//This must be longer than MAX_DELIVERY_FAILURES * CHECKIN_PERIOD
//250ms is good
//L defines the value as a long. Needed for millisecond times larger than int (+32,767) but doesn't hurt to have.
#define MAX_TIME_WITHOUT_OK 250L

unsigned long lastCheckin = 0;

RH_RF69 rf69;

#define RELAY_CONTROL 9
#define PAUSE_PIN 6

#define LED_RED 5
#define LED_YLW 4
#define LED_GRN 3

//Define the various system states
#define RED 'R'
#define YELLOW 'Y'
#define GREEN 'G'
#define DISCONNECTED 'D'

char systemState;

void setup()
{
  wdt_reset(); //Pet the dog
  wdt_disable(); //We don't want the watchdog during init

  Serial.begin(9600);

  pinMode(RELAY_CONTROL, OUTPUT);
  turnOffRelay(); //During power up turn off power

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YLW, OUTPUT);
  pinMode(LED_GRN, OUTPUT);

  pinMode(PAUSE_PIN, OUTPUT);
  digitalWrite(PAUSE_PIN, LOW); //Resume

  if (!rf69.init())
    Serial.println("init failed");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf69.setFrequency(915.0))
    Serial.println("setFrequency failed");

  // If you are using a high power RF69, you *must* set a Tx power in the range 14 to 20 like this:
  rf69.setTxPower(20);

  //This key is the same on the remote. Pick your own random sequence.
  uint8_t key[] = { 0xAB, 0x1C, 0x0E, 0x39, 0xF8, 0xFF, 0xA6, 0xFC,
                    0x7B, 0x44, 0xC3, 0xC0, 0x2D, 0x2D, 0x2D, 0xD2
                  };
  rf69.setEncryptionKey(key);

  systemState = RED; //On power up start in red state
  setLED(LED_RED);

  Serial.println("Power Wheels Kill Switch Online");

  wdt_reset(); //Pet the dog
//  wdt_enable(WDTO_1S); //Unleash the beast
}

void loop()
{
  if (millis() - lastCheckin > MAX_TIME_WITHOUT_OK)
  {
    if (systemState != DISCONNECTED)
    {
      setLED(LED_RED); //Turn on LED
      turnOffRelay();
      systemState = DISCONNECTED;

      Serial.println("Remote failed to check in! Turn off relay!");
    }
  }

  if (rf69.available())
  {
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf69.recv(buf, &len))
    {
      sendResponse(); //Respond back to the remote that we heard it loud and clear

      Serial.print("Received: ");
      Serial.println((char*)buf);

      if (buf[0] == RED || buf[0] == YELLOW || buf[0] == GREEN || buf[0] == DISCONNECTED) lastCheckin = millis(); //Reset timeout

      if (buf[0] == RED)
      {
        if (systemState != RED)
        {
          setLED(LED_RED); //Turn on LED
          turnOffRelay();
          systemState = RED;

          Serial.println("Kill!");
        }
      }
      else if (buf[0] == YELLOW)
      {
        if (systemState != YELLOW)
        {
          setLED(LED_YLW); //Turn on LED
          digitalWrite(PAUSE_PIN, HIGH);
          systemState = YELLOW;

          Serial.println("Pause!");
        }
      }
      else if (buf[0] == GREEN)
      {
        if (systemState != GREEN)
        {
          digitalWrite(PAUSE_PIN, LOW); //Turn off pause
          setLED(LED_GRN); //Turn on LED
          turnOnRelay();
          systemState = GREEN;

          Serial.println("Go!");
        }
      }
      else if (buf[0] == DISCONNECTED)
      {
        //If we've received a 'D' from the remote it means
        //it is trying to get back in touch
        setLED(LED_RED); //Turn on LED
        turnOffRelay();
        systemState = DISCONNECTED; //Remote will move the state from disconnected

        Serial.println("Reconnecting!");
      }

      Serial.print("RSSI: ");
      Serial.println(rf69.lastRssi(), DEC);
    }
  }
}

//If we receive a system state we send a response
void sendResponse()
{
  uint8_t response[] = "O"; //Send OK
  rf69.send(response, sizeof(response));
  rf69.waitPacketSent(50); //Block for 50ms before moving on

  Serial.println("Sent a reply");
}

//Turns on a given LED
void setLED(byte LEDnumber)
{
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YLW, LOW);
  digitalWrite(LED_GRN, LOW);

  digitalWrite(LEDnumber, HIGH);
}

//Turn on the relay
void turnOnRelay()
{
  digitalWrite(RELAY_CONTROL, HIGH);
}

//Turn off the relay
void turnOffRelay()
{
  digitalWrite(RELAY_CONTROL, LOW);
}


