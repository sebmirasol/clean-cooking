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
#include <AHT10.h>
#include <MQ135.h>
#include <MQ7.h>

#define DEVICE_ID 1
#define DEBUG_MODE //Comment out in order to turn off debug mode.
#define LOW_BATTERY_LVL     40
#define CRTICAL_BATTERY_LVL 10

//pin allocation
#define PIN_A_MQ135 A1
#define PIN_A_MQ7 A0
#define PIN_RX_PM 2
#define PINT_TX_PM 3

//define states and working functions
State HEAT = State(sensor_heating);
State NOMINAL = State(nominal_sensing);
State MINIMAL = State(minimal_sensing);
State CRITICAL = State(critical_sensing);
FSM Sensing = FSM(HEAT);

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
int sample_counter = 0;

//sensor data
AHT10Class AHT10;
MQ135 mq135_sensor(PIN_A_MQ135);
MQ7 mq7(PIN_A_MQ7, 5);
String sensor_data;
int temperature = 21;   //assume for initial measurment, will be overwritten
int humidity = 25;      //assume for initial measurment, will be overwritten
int CO = 0;
int CO2 = 0;
int bat = 100;

struct pms5003data {
  uint16_t framelen;
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t unused;
  uint16_t checksum;
};

struct pms5003data data;

SoftwareSerial pmSerial =  SoftwareSerial(PIN_RX_PM, PINT_TX_PM);

//storage variables
File dataLog;

/*
*   Setup
*/
void setup() {
  //initialize serial coms
  #ifdef DEBUG_MODE
    Serial.begin(9600);
    Serial.print("Initializing set up...");
  #endif //DEBUG_MODE

  //initialize GPIO

  #ifdef DEBUG_MODE
    Serial.begin(9600);
    Serial.println("DONE");
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
    read_sensors_flag = false;
  }

  if(sample_counter > nominal_samples_transmission){
    send_data();
  }
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
    read_sensors_flag = false;
  }

  if(sample_counter > minimal_samples_transmission){
    send_data();
  }

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
    read_sensors_flag = false;
  }
}

/*
*   Additional functions
*/

void read_sensor_data(){
  temperature = AHT10.GetTemperature();
  humidity = AHT10.GetHumidity();

  CO2 = mq135_sensor.getCorrectedPPM(temperature, humidity);
  CO = mq7.readPpm();

  sensor_data = DEVICE_ID;
  sensor_data.concat(";");
  sensor_data.concat(temperature);
  sensor_data.concat(";");
  sensor_data.concat(humidity);
  sensor_data.concat(";");
  sensor_data.concat(CO);
  sensor_data.concat(";");
  sensor_data.concat(CO2);
  sensor_data.concat(";");
  sensor_data.concat(data.pm10_env);
  sensor_data.concat(";");
  sensor_data.concat(data.pm25_env);
  sensor_data.concat(";");
  sensor_data.concat(data.pm100_env);
  sensor_data.concat(";#");
}

void store_data(){
  while (!SD.begin(10)) {;}

  dataLog = SD.open("log.txt", FILE_WRITE);

  if (dataLog) {
    dataLog.println(sensor_data);
    dataLog.close();
  }
}

void send_data(){
  //TO DO: count how many lines are pending to be sent
  //Send all data in the file
  //Erase data on the file
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