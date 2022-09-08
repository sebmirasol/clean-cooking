/*
  main_stateMachine.ino

  Description: State machine that measureas atmospheric quality indexes
  and forwards them to a server taking into account battery level

  Code Style Standard: snake_case -> (I know it's a stupid statement, it's just there to remind myself...)

  Author: Sebastian Mirasol
*/

#include <FiniteStateMachine.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <SD.h>
#include <SPI.h>

#define DEBUG_MODE //Comment out in order to turn off debug mode.
#define LOW_BATTERY_LVL     40
#define CRTICAL_BATTERY_LVL 10

//define states and working functions
State HEAT = State(sensor_heating);
State NOMINAL = State(nominal_sensing);
State MINIMAL = State(minimal_sensing);
State CRITICAL = State(critical_sensing);

//sample time config
const long int nominal_sample_period = 60000;
const long int minimal_sample_period = 900000;
unsigned long time_now = 0;

//number of data transmission config
const int nominal_samples_transmission = 15;
const int minimal_samples_transmission = 60;

//variables
bool read_sensors_flag = false;
bool sensors_heated = false;

/*
*   Setup
*/
void setup() {
  //initialize serial coms
  #ifdef DEBUG_MODE
    Serial.begin(9600);
    Serial.print("Initializing set up...")
  #endif //DEBUG_MODE

  //initialize GPIO

  #ifdef DEBUG_MODE
    Serial.begin(9600);
    Serial.println("DONE")
  #endif //DEBUG_MODE
}

/*
*   Main loop
*/
void loop() {
  //TO DO: Update battery reading

  if(sensors_heated == false){
    Sensing.transitionTo(HEAT);
  }else if(bat<=CRTICAL_BATTERY_LVL){
    Sensing.transitionTo(CRITICAL);
  }else if(bat>CRTICAL_BATTERY_LVL && bat<LOW_BATTERY_LVL){
    Sensing.transitionTo(MINIMAL);
  }else if(bat>=LOW_BATTERY_LVL){
    Sensing.transitionTo(NOMINAL);
  }else{;}
      
  Sensing.update();

  #ifdef DEBUG_MODE
    Serial.println("Looping around...");
  #endif //DEBUG_MODE
}

/*
*   Finite State Machines
*/
void sensor_heating(){
  #ifdef DEBUG_MODE
    Serial.println("FSM: HEATING UP SENSORS");
  #endif //DEBUG_MODE

  //TO DO: GPIO operations to turn on and heat up sensors
  sensors_heated = true;
}

void nominal_sensing(){
  #ifdef DEBUG_MODE
    Serial.println("FSM: NOMINAL SENSING");
  #endif //DEBUG_MODE

  if((unsigned long)(millis() - time_now) > nominal_sample_period){
        time_now = millis();
        read_sensors_flag = true;
  }

  if(readPMSdata() && read_sensors_flag){
    read_sensor_data();
    store_data();
  }

  read_sensors_flag = false;
}

void minimal_sensing(){
  #ifdef DEBUG_MODE
    Serial.println("FSM: MINIMAL SENSING");
  #endif //DEBUG_MODE

  if((unsigned long)(millis() - time_now) > minimal_sample_period){
        time_now = millis();
        read_sensors_flag = true;
  }

  if(readPMSdata() && read_sensors_flag){
    read_sensor_data();
    store_data();
  }

  read_sensors_flag = false;
}

void critical_sensing(){
  #ifdef DEBUG_MODE
    Serial.println("FSM: CRITICAL SENSING");
  #endif //DEBUG_MODE

  if((unsigned long)(millis() - time_now) > minimal_sample_period){
        time_now = millis();
        read_sensors_flag = true;
  }

  if(readPMSdata() && read_sensors_flag){
    read_sensor_data();
    store_data();
  }

  read_sensors_flag = false;
}

/*
*   Additional functions
*/

void read_sensor_data(){

}

void store_data(){

}

boolean readPMSdata() {
  if (! pmSerial.available()) {
    return false;
  }
  
  // Read a byte at a time until we get to the special '0x42' start-byte
  if (pmSerial.peek() != 0x42) {
    pmSerial.read();
    return false;
  }
 
  // Now read all 32 bytes
  if (pmSerial.available() < 32) {
    return false;
  }
    
  uint8_t buffer[32];    
  uint16_t sum = 0;
  pmSerial.readBytes(buffer, 32);
 
  // get checksum ready
  for (uint8_t i=0; i<30; i++) {
    sum += buffer[i];
  }
  
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i=0; i<15; i++) {
    buffer_u16[i] = buffer[2 + i*2 + 1];
    buffer_u16[i] += (buffer[2 + i*2] << 8);
  }
 
  // put it into a nice struct :)
  memcpy((void *)&data, (void *)buffer_u16, 30);
 
  if (sum != data.checksum) {
    return false;
  }
  // success!
  return true;
}