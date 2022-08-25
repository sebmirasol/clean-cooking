/*
  main_stateMachine.ino

  Description: State machine that measureas atmospheric quality indexes
  and forwards them to a server taking into account battery level

  Code Style Standard: snake_case

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

/*
*   Setup
*/
void setup() {
  //initialize serial coms
  #ifdef DEBUG_MODE
    Serial.begin(9600);
  #endif //DEBUG_MODE

  //initialize GPIO
}

/*
*   Main loop
*/
void loop() {
  //TO DO: Update battery reading

  if(flag_heated == false){
    Sensing.transitionTo(HEAT);
  }else if(bat<=CRTICAL_BATTERY_LVL){
    Sensing.transitionTo(CRITICAL);
  }else if(bat>CRTICAL_BATTERY_LVL && bat<LOW_BATTERY_LVL){
    Sensing.transitionTo(MINIMAL);
  }else if(bat>=LOW_BATTERY_LVL){
    Sensing.transitionTo(NOMINAL);
  }else{;}
      
  //TO DO: Execute sensor operations

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
}

void nominal_sensing(){
  #ifdef DEBUG_MODE
    Serial.println("FSM: NOMINAL SENSING");
  #endif //DEBUG_MODE
}

void minimal_sensing(){
  #ifdef DEBUG_MODE
    Serial.println("FSM: MINIMAL SENSING");
  #endif //DEBUG_MODE
}

void critical_sensing(){
  #ifdef DEBUG_MODE
    Serial.println("FSM: CRITICAL SENSING");
  #endif //DEBUG_MODE
}

/*
*   Additional functions
*/