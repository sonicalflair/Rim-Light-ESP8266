
#define FASTLED_ALLOW_INTERRUPTS 0  //Not sure if this is screwing up my POV
#include "FastLED.h"
#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

const char *ssid = "Warehouse Access Point"; // The name of the Wi-Fi network that will be created
const char *password = "thereisnospoon";   // The password required to connect to it, leave blank for an open network

const char *OTAName = "ESP8266";           // A name and a password for the OTA service
const char *OTAPassword = "123456";

// How many leds in your strip?
#define NUM_LEDS 85
#define DATA_PIN 2

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// Define the array of leds
CRGB leds[NUM_LEDS];

// set pin numbers:
//const int hall_sensor_Pin = 11;     // the number of the pushbutton pin for Promini
const int hall_sensor_Pin = 16;     // the number of the pushbutton pin for esp8266

// Variables will change:
byte ledState = HIGH;                // the current state of the output pin
byte current_hall_sensor_state;      // the current reading from the input pin
byte last_hall_sensor_state = LOW;   // the previous reading from the input pin

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.

unsigned long current_time = 0;          // Keeps track of the start of each loop time
unsigned long prev_hall_trigger_time = 0;// Keeps track of when the last wheel revolution was
int led_counter = 0;                     // Keeps track of the current addressable LED
int prev_led_counter = 0;                // Keeps track of the last LED address that was lit
long hall_effect_period = 0;             // Used to store the current period between wheel revolution
long prev_hall_effect_period = 0;        // Used to store the last period between wheel revolution
unsigned long prev_led_time = 0;         // Used to keep track of the last time an LED was lit for the annimation
int num_leds_animate = 20;               // Number of LEDS to show for the annimation
boolean is_bike_speeding_up = false;

unsigned long led_start_time = 0; //timer used to figure out how long it takes for LEDs to finish a whole rotation

unsigned long fastest_wheel_period = 10000;

// Variables used to figure out how much to stagger the initial led position by
byte stagger_led_array[NUM_LEDS];
byte stagger_leds_by = 50; //So LEDs will start at position 50

// Variables used to for a Stationary Bike State
boolean bike_stationary = false;
boolean prev_bike_stationary = false;
byte stationary_counter = 0; //Used to keep track of the rotations after the bike is stationary
byte moving_counter = 0;     //Used to keep track of the rotations after the bike is moving

unsigned long stationary_time_trigger = 3 * 1000UL * 1000UL;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void setup() {

  delay(2000);  //Delayed added upon start up, to initalize things before the LEDs turn on.

  Serial.begin(57600);

  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection

  startOTA();                  // Start the OTA service

  //Set up LED array to move the Start LED to the user defined place
  //50 works well for the front lights.
  for (byte i = 0; i < (NUM_LEDS - stagger_leds_by); i++) {
    stagger_led_array[i] = stagger_leds_by + i;
  }
  for (byte i = 0; i < stagger_leds_by; i++) {
    stagger_led_array[i + (NUM_LEDS - stagger_leds_by)] = i;
  }

  pinMode(hall_sensor_Pin, INPUT);

  LEDS.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);
  LEDS.setBrightness(150);
  //LEDS.setMaxRefreshRate(0); //Doesn't work well for WS2812 lights,
  LEDS.setDither(0); //Recommended by Wiki to turnoff when using POV
}

void loop() {

  current_time = micros(); // Take a snapshot of the current time in micros()

  ArduinoOTA.handle();                        // listen for OTA events

  EVERY_N_MILLISECONDS( 5 ) {
    gHue++;  //Used to change rainbow variable
  }

  // Read the state of the hall effect sensor into a local variable:
  byte hall_sensor_reading = digitalRead(hall_sensor_Pin);
  current_hall_sensor_state = hall_sensor_reading;

  // If the current reading is different from the last reading
  if (hall_sensor_reading != last_hall_sensor_state) {

    last_hall_sensor_state = hall_sensor_reading;  // Remember the current setting

    if (hall_sensor_reading == HIGH) { //Sensor HIGH means no magnet, Sensor LOW means the magnet is in range

      prev_hall_effect_period = hall_effect_period;

      hall_effect_period = current_time - prev_hall_trigger_time;  //Calculate the time between each rotation

      //Serial.print("New Hall Effect Period: "); Serial.println(hall_effect_period);
      //Serial.print("New LED Period: "); Serial.println(hall_effect_period / NUM_LEDS);
      //Serial.println();

      //If the bike is stationary, count a few rotations before starting the spinning wheel animation
      if (bike_stationary == true) {
        stationary_counter++;
      }

      if (bike_stationary == false) {
        moving_counter++;
      }

      prev_hall_trigger_time = current_time; //Remember the time when the sensor was triggered. Used to
      blackout(); //Clear all the LEDS

      float speed_bias = .1;

      //        if (hall_effect_period > prev_hall_effect_period) {
      //          //This means i'm slowing down
      //          is_bike_speeding_up = false;
      //          hall_effect_period = (hall_effect_period + prev_hall_effect_period)/2
      //        } else {
      //          //This means i'm speeding up or staying the same
      //          is_bike_speeding_up = true;
      //          hall_effect_period = hall_effect_period * (1 - speed_bias);
      //        }

      //      //Trying to smooth the LED position out
      //      if(led_counter > 5){
      //        //This means that the moving led annimation is faster than the wheel
      //        led_counter = led_counter / 2;
      //      }else if (led_counter < 0){
      //        //This means that the moving led annimation is slower than the wheel
      //        led_counter = (led_counter + 84) / 2;
      //      }else{
      //        led_counter = 0;
      //      }

      led_counter = 0; // Variable used for keeping track of the current addressable LED
      // Resetting this to 0 makes things pretty jerky.

    }

  }

  // Calculating the fastest Wheel Period
  //  if(hall_effect_period < fastest_wheel_period && hall_effect_period != 0){
  //    fastest_wheel_period = hall_effect_period;
  //    Serial.print("Faster Wheel Period Set: "); Serial.println(fastest_wheel_period);
  //    EEPROMWritelong(77, fastest_wheel_period);
  //  }

  if ((current_time - prev_hall_trigger_time) >= stationary_time_trigger) {
    //TESTCODE, should be true, setting to false to eliminate my stationary annimation
    bike_stationary = true;
    // bike_stationary = false;
    stationary_counter = 0;
  } else if (stationary_counter >= 3) {
    bike_stationary = false;
  }

  if (bike_stationary != prev_bike_stationary) {
    prev_bike_stationary = bike_stationary;

    if (bike_stationary == true) { //If true for the first time
      stationary_counter = 0;     //Set the counter to 0, Counter is incremented in Hall Logic Loop
    }

    if (bike_stationary == false) { // If false for the first time
      prev_led_time = 0;
      moving_counter = 0;
    }

  }

  if (bike_stationary == true) {  //If the bike is stationary, show the Rainbow Animation
    rainbow(); FastLED.show();
  }

  //Serial.print("Current_time - prev_led_time: "); Serial.println(current_time - prev_led_time);

  unsigned long time_per_led = hall_effect_period / NUM_LEDS;

  //bike_stationary = false;
  //time_per_led = 0;

  if (bike_stationary == false) {

    if ((current_time - prev_led_time) >= time_per_led) {

      //prev_led_time = prev_led_time + time_per_led;
      prev_led_time = micros();

      if (led_counter >= NUM_LEDS) {

        //        Serial.print("LED Counter Resetting on: "); Serial.println(led_counter);
        //        Serial.print("Hall Effect Period: "); Serial.println(hall_effect_period);
        //        Serial.print("Hall Effect Period / NUM_LEDS: "); Serial.println(time_per_led);
        //        Serial.print("Time for LEDs to complete: "); Serial.println(current_time - led_start_time);
        //        long offset_time = hall_effect_period - (current_time - led_start_time);
        //        Serial.print("Offset between Wheel Period and LED: "); Serial.println(offset_time);
        //        Serial.println("");

        led_start_time = current_time;
        prev_led_counter = 84;
        led_counter = 0;
      }

      for (byte i = 0; i < num_leds_animate; i++) {
        leds[stagger_led_array[addmod8(led_counter, i, NUM_LEDS)]] = CRGB::Gold ;
      }

      leds[stagger_led_array[prev_led_counter]] = CRGB::Black;

      FastLED.show();

      prev_led_counter = led_counter; //keep track of the previous led counter
      led_counter++;
    }
  }

}

void blackout (void) {
  FastLED.clear();
  FastLED.show();
}

void rainbow() {
  fill_rainbow( leds, NUM_LEDS, gHue, 7); // FastLED's built-in rainbow generator
}

void startWiFi() { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  wifiMulti.addAP("Sleepy_Red_Panda", "myronrose");   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED && WiFi.softAPgetStationNum() < 1) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  if (WiFi.softAPgetStationNum() == 0) {     // If the ESP is connected to an AP
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());             // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  } else {                                   // If a station is connected to the ESP SoftAP
    Serial.print("Station connected to ESP8266 AP");
  }
  Serial.println("\r\n");
}

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}


