/*
  main_stateMachine.ino

  Description: State machine that measureas atmospheric quality indexes
  and forwards them to a server taking into account battery level

  Code Style Standard: snake_case -> (I know it's a stupid statement, it's just there to remind myself and ignore it actively...)

  Author: Sebastian Mirasol
*/

#include <FiniteStateMachine.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <AHT10.h>
#include <MQ135.h>
#include <MQ7.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

#define DEVICE_ID           01
#define DEBUG_MODE              //Comment out in order to turn off debug mode.
#define LOW_BATTERY_LVL     40
#define CRTICAL_BATTERY_LVL 10
#define SERVER_IP           
#define PORT                

//pin allocation
#define PIN_A_MQ135   A1
#define PIN_A_MQ7     A0
#define PIN_RX_PM     10
#define PINT_TX_PM    11
#define SIM_RXD       12
#define SIM_TXD       13
#define pinCS_SD      53

//AT Messages & variables
#define DEFAULT_TIMEOUT   10000
const char AT_RSP_OK[] PROGMEM = "OK";
const char AT_RSP_CREG[] PROGMEM = ",5";
const char AT_RSP_CPIN[] PROGMEM = "+CPIN: READY";
const char AT_RSP_CGATT[] PROGMEM = "+CGATT: 1";
const char AT_RSP_CIFSR[] PROGMEM = ".";
const char AT_RSP_CIPSTART[] PROGMEM = "CONNECT";
const char AT_RSP_CFUN[] PROGMEM = "NOT READY";
int crlfToWait = 2;
int startIdx = 0;
bool enableDebug = true;
bool register_flag = false;
uint16_t internalBufferSize = 128;
char *internalBuffer = (char*) malloc(internalBufferSize);

//define states and working functions
State HEAT = State(sensor_heating);
State NOMINAL = State(nominal_sensing);
State MINIMAL = State(minimal_sensing);
State CRITICAL = State(critical_sensing);
FSM Sensing = FSM(HEAT);

//sample time config
const long int nominal_sample_period = 5000;   //default: 300000
const long int minimal_sample_period = 900000;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

//number of data transmission config
const int nominal_samples_transmission = 10;    //default: 12
const int minimal_samples_transmission = 60;

//conection configuration
String msg_disconnect ="!O";

String serial_concat;

//variables
bool read_sensors_flag = false;
bool sensors_heated = false;
int sample_counter = 0;
const char *monthName[12] = {  "Jan", "Feb", "Mar", "Apr", "May", "Jun",  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
tmElements_t tm;

//serial processing related variables
String readString, substringDate, substringTime;
const byte numChars = 255;      // input buffer size
char receivedChars[numChars];   // an array to store the received data

//sensor data
AHT10Class AHT10;
MQ135 mq135_sensor(PIN_A_MQ135);
MQ7 mq7(PIN_A_MQ7, 5);
String sensor_data;
float temperature = 0;   
int humidity = 0;      
float CO = 0;
float CO2 = 0;
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

SoftwareSerial pmsSerial =  SoftwareSerial(PIN_RX_PM, PINT_TX_PM);
SoftwareSerial GSM_serial =  SoftwareSerial(SIM_RXD, SIM_TXD);

//storage variables
File dataLog;

/*
*   Setup
*/
void setup() {
  //initialize serial coms
  #ifdef DEBUG_MODE
    Serial.begin(9600);
    Serial.println("[SYSTEM] Initializing subsystems, sensors...");
    enableDebug = true;
  #endif //DEBUG_MODE

  bool parse=false;
  bool config=false;

  //initialize GPIO & SoftwareSerials
  pmsSerial.begin(9600);              //initialize Software Serial communication with PM sensor
  Wire.begin();                       //initialize i2C communication with temp & humidity sensor
  AHT10.begin(eAHT10Address_Low);     //locate I2C sensor
  GSM_serial.begin(9600);             //initialize Software Serial communication with GPRS
  pinMode(pinCS_SD, OUTPUT);          //SPI chip select

  GSM_serial.println("AT"); //Sync baudrate
  delay(500);
  GSM_serial.println("AT+CFUN=0"); //Set to low power
  delay(500);

  //clear our input buffer
  while(GSM_serial.available()>0){  //flush the OK from the input buffer;
    GSM_serial.read();
  }

  //set RTC
  if (getDate(__DATE__) && getTime(__TIME__)) {
    parse = true;
    // and configure the RTC with this info
    if (RTC.write(tm)) {
      config = true;
    }
  }

  #ifdef DEBUG_MODE
    Serial.print("[RTC] Initialized: ");
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm.Day);
    Serial.write('/');
    Serial.print(tm.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.println();
  #endif //DEBUG_MODE

  //SD card initialization
  if (SD.begin()){
     if (SD.exists("logger.csv")) {
      #ifdef DEBUG_MODE
        Serial.println("[SD] Ready for operation");
      #endif //DEBUG_MODE
    } else {
    #ifdef DEBUG_MODE
      Serial.println("[SD] ERROR: No file in card");
    #endif //DEBUG_MODE
    }
  } else {
    #ifdef DEBUG_MODE
      Serial.println("[SD] ERROR: No card in slot");
    #endif //DEBUG_MODE
  }

  // clear our contents from SD
  SD.open("logger.csv", FILE_WRITE | O_TRUNC);

  //calibrate MQ7 sensor
  mq7.calibrate();

  #ifdef DEBUG_MODE
    Serial.println("[SYSTEM] ----- Initilization completed -----");
  #endif //DEBUG_MODE
}

/*
*   Main loop
*/
void loop() {
  //TO DO: Update battery reading
  currentMillis = millis();

  if(currentMillis - previousMillis >= nominal_sample_period){
    previousMillis = currentMillis;
    read_sensors_flag = true;
  }

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
}

/*
*   Finite State Machines
*/
void sensor_heating(){
  #ifdef DEBUG_MODE
    Serial.println("[FSM] Heating up sensors");
  #endif //DEBUG_MODE

  //TO DO: GPIO operations to turn on and heat up sensors
  delay(5000);

  sensors_heated = true;
}

void nominal_sensing(){
  if(readPMSdata() && read_sensors_flag){
    read_sensor_data();
    store_data();
    read_sensors_flag = false;
  }

  if(sample_counter >= nominal_samples_transmission){
    send_data();
  }
}

void minimal_sensing(){
  if(readPMSdata() && read_sensors_flag){
    read_sensor_data();
    store_data();
    read_sensors_flag = false;
  }

  if(sample_counter >= minimal_samples_transmission){
    send_data();
  }
}

void critical_sensing(){
 if(readPMSdata() && read_sensors_flag){
    read_sensor_data();
    store_data();
    read_sensors_flag = false;
  }
}

/*
*   GRPS functions
*/

void connect_to_network(){
  bool network_flag = true;
  GSM_serial.listen();

  do{
    send_command("AT+CFUN=1");
    if(!check_response(DEFAULT_TIMEOUT, AT_RSP_CPIN, crlfToWait)) {
      if(enableDebug) Serial.println("[GSM] ERROR: AT+CFUN = 1");
    }
      
    send_command("AT+CIPSHUT");
    if(!check_response(DEFAULT_TIMEOUT, AT_RSP_OK, crlfToWait)) {
      if(enableDebug) Serial.println("[GSM] ERROR: AT+CIPSHUT");
    }

    send_command("AT+CPIN?");
    if(!check_response(DEFAULT_TIMEOUT, AT_RSP_CPIN, crlfToWait)) {
      if(enableDebug) Serial.println("[GSM] ERROR: AT+CPIN?");
    }

    do{
      send_command("AT+CREG?");
      if(!check_response(DEFAULT_TIMEOUT, AT_RSP_CREG, crlfToWait)) {
        if(enableDebug) Serial.println("[GSM] ERROR: Did not register to network");
        network_flag = false;
      } else {
        network_flag = true;
      }
    }while(network_flag == false);

    do{
      send_command("AT+CGATT?");
      if(!check_response(DEFAULT_TIMEOUT, AT_RSP_CGATT, crlfToWait)) {
        if(enableDebug) Serial.println("[GSM] ERROR: Could not attach to network");
        network_flag = false;
      } else {
        network_flag = true;
      }
    }while(network_flag == false);

    send_command("AT+CIPMUX=0");
    if(!check_response(DEFAULT_TIMEOUT, AT_RSP_OK, crlfToWait)) {
      if(enableDebug) Serial.println("[GSM] ERROR: AT+CIPMUX=0");
    }

    send_command("AT+CSTT = \"TM\",\" \",\" \"");
    if(!check_response(DEFAULT_TIMEOUT, AT_RSP_OK, crlfToWait)) {
      if(enableDebug) Serial.println("[GSM] ERROR: AT+CSTT");
    }

    send_command("AT+CIICR");
    if(!check_response(DEFAULT_TIMEOUT, AT_RSP_OK, crlfToWait)) {
      if(enableDebug) Serial.println("[GSM] ERROR: Could not bring up GPRS connection");
      network_flag = false;
    } else {
      network_flag = true;
    }

    send_command("AT+CIFSR");
    if(!check_response(DEFAULT_TIMEOUT, AT_RSP_CIFSR, crlfToWait)) {
      if(enableDebug) Serial.println("[GSM] ERROR: AT+CIFSR");
    }
  }while(network_flag == false);
}

void connect_to_server(){
  GSM_serial.listen();

  send_command("AT+CIPSTART=\"TCP\",\"170.253.59.48\",\"1121\"");
  if(!check_response(120000, AT_RSP_CIPSTART, 4)) {
    if(enableDebug) Serial.println("[GSM] ERROR: AT+CIPSTART");
  }
}

void send_message(String msg){
  GSM_serial.listen();

  int payload_size = msg.length();
  int header_size   = String(payload_size).length();

  char header_size_str[header_size]; // Define a suitable size for the char array to hold the header size as a string
  char payload_size_str[payload_size]; // Define a suitable size for the char array to hold the payload size as a string

  // Convert payload_size and header_size to char arrays
  itoa(payload_size, payload_size_str, 10);
  itoa(String(payload_size).length(), header_size_str, 10);

  send_command("AT+CIPSEND=",header_size_str);
  send_command(payload_size_str);
  if(!check_response(60000, AT_RSP_OK, 3)) {
    if(enableDebug) Serial.println("[GSM] ERROR: AT+CIPSEND");
  }

  send_command("AT+CIPSEND=",payload_size_str);
  send_command(msg.c_str());
  if(!check_response(60000, AT_RSP_OK, 3)) {
    if(enableDebug) Serial.println("[GSM] ERROR: AT+CIPSEND");
  }
}

void disconnect_from_network(){
  GSM_serial.listen();

  send_command("AT+CIPSHUT");
  if(!check_response(DEFAULT_TIMEOUT, AT_RSP_OK, crlfToWait)) {
    if(enableDebug) Serial.println("[GSM] ERROR: AT+CIPSHUT");
  }

  send_command("AT+CFUN=0");
  if(!check_response(DEFAULT_TIMEOUT, AT_RSP_CFUN, crlfToWait)) {
    if(enableDebug) Serial.println("[GSM] ERROR: AT+CFUN=0");
  }
}

// GPRS Auxiliary Functions

void send_command(const char* command){
  GSM_serial.listen();

  delay(500);
  purge_serial();
  GSM_serial.write(command);
  GSM_serial.write("\r\n");
  purge_serial();
}

void send_command(const char* command,const char* message){
  GSM_serial.listen();
  
  delay(500);
  purge_serial();
  GSM_serial.write(command);
  GSM_serial.write(message);
  GSM_serial.write("\r\n");
  purge_serial();
}

void purge_serial() {
  GSM_serial.listen();

  GSM_serial.flush();
  while (GSM_serial.available()) {
    GSM_serial.read();
  }
  GSM_serial.flush();
}

bool check_response(uint16_t timeout, const char* expectedAnswer, uint8_t crlfToWait) {
  GSM_serial.listen();

  if(readResponse(timeout, crlfToWait)) {
    // Prepare the local expected answer
    char rspBuff[16];
    strcpy_P(rspBuff, expectedAnswer);

    // Check if it's the expected answer
    int16_t idx = strIndex(internalBuffer, rspBuff);
    if(idx > 0) {
      return true;
    }
  }
  return false;
}

bool readResponse(uint16_t timeout, uint8_t crlfToWait) {
  uint16_t currentSizeResponse = 0;
  bool seenCR = false;
  uint8_t countCRLF = 0;

  GSM_serial.listen();

  // First of all, cleanup the buffer
  initInternalBuffer();

  uint32_t timerStart = millis();

  while(1) {
    // While there is data available on the buffer, read it until the max size of the response
    if(GSM_serial.available()) {
      // Load the next char
      internalBuffer[currentSizeResponse] = GSM_serial.read();

      // Detect end of transmission (CRLF)
      if(internalBuffer[currentSizeResponse] == '\r') {
        seenCR = true;
      } else if (internalBuffer[currentSizeResponse] == '\n' && seenCR) {
        countCRLF++;
        if(countCRLF == crlfToWait) {
          if(enableDebug) Serial.println(F("SIM800L : End of transmission"));
          break;
        }
      } else {
        seenCR = false;
      }

      // Prepare for next read
      currentSizeResponse++;

      // Avoid buffer overflow
      if(currentSizeResponse == internalBufferSize) {
        if(enableDebug) Serial.println(F("SIM800L : Received maximum buffer size"));
        break;
      }
    }

    // If timeout, abord the reading
    if(millis() - timerStart > timeout) {
      if(enableDebug) Serial.println(F("SIM800L : Receive timeout"));
      // Timeout, return false to parent function
      return false;
    }
  }

  if(enableDebug) {
    Serial.print(F("SIM800L : Receive \""));
    Serial.print(internalBuffer);
    Serial.println(F("\""));
  }

  // If we are here, it's OK ;-)
  return true;
}

void initInternalBuffer() {
  for(uint16_t i = 0; i < internalBufferSize; i++) {
    internalBuffer[i] = '\0';
  }
}

int strIndex(const char* str, const char* findStr) {
  int16_t firstIndex = -1;
  int16_t sizeMatch = 0;
  int16_t startIdx = 0;
  for(int16_t i = startIdx; i < strlen(str); i++) {
    if(sizeMatch >= strlen(findStr)) {
      break;
    }
    if(str[i] == findStr[sizeMatch]) {
      if(firstIndex < 0) {
        firstIndex = i;
      }
      sizeMatch++;
    } else {
      firstIndex = -1;
      sizeMatch = 0;
    }
  }

  if(sizeMatch >= strlen(findStr)) {
    return firstIndex;
  } else {
    return -1;
  }
}

/*
*   Additional functions
*/

void read_sensor_data(){
  #ifdef DEBUG_MODE
    Serial.println("[Sensors] Reading air quality sensors");
  #endif //DEBUG_MODE

  RTC.read(tm);
  substringTime = String(tm.Hour) + ":" + String(tm.Minute) + ":" + String(tm.Second);
  substringDate = String(tm.Day) + "/" + String(tm.Month) + "/" + String(tm.Year);

  temperature = AHT10.GetTemperature();                       //read sensors
  humidity = AHT10.GetHumidity();
  CO2 = mq135_sensor.getCorrectedPPM(temperature, humidity);
  CO = mq7.readPpm();

  sensor_data = DEVICE_ID;                                    //create log string
  sensor_data.concat(";");
  sensor_data.concat(substringDate);
  sensor_data.concat(";");
  sensor_data.concat(substringTime);
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

  sample_counter = sample_counter + 1;

  #ifdef DEBUG_MODE
    Serial.println("[Sensors] Sensor data: " + sensor_data);
  #endif //DEBUG_MODE
}

void store_data(){
  #ifdef DEBUG_MODE
    Serial.println("[SD] Storing sensor data in SD card");
  #endif //DEBUG_MODE

  dataLog = SD.open("logger.csv", FILE_WRITE);

  if (dataLog) {
    dataLog.println();
    dataLog.print(sensor_data);
    dataLog.close();
  }
  else{
    #ifdef DEBUG_MODE
      Serial.println("[SD] ERROR: could not store data");
    #endif //DEBUG_MODE
  }
}

void send_data(){
  #ifdef DEBUG_MODE
    Serial.println("[GSM] Reading data from SD card and sending to server");
  #endif //DEBUG_MODE

  connect_to_network();
  connect_to_server();

  File dataLog = SD.open("logger.csv");

  dataLog.read(); //read on empty the first line of the file since it will be a '\n\r'

  // Read line and send it
  if (dataLog) 
  {
    while (dataLog.available())
    {
      Serial.write(dataLog.read());
      String dataArray = dataLog.readStringUntil('\r');
      send_message(dataArray);
      delay(100);
    }
    dataLog.close();
  }  

  disconnect_from_network();

  // empty the file on the SD since the data has been sent
  SD.open("logger.csv", FILE_WRITE | O_TRUNC);

  sample_counter=0;
}

boolean readPMSdata() {
  pmsSerial.listen();

  if (! pmsSerial.available()) {
    return false;
  }
  
  // Read a byte at a time until we get to the special '0x42' start-byte
  if (pmsSerial.peek() != 0x42) {
    pmsSerial.read();
    return false;
  }
 
  // Now read all 32 bytes
  if (pmsSerial.available() < 32) {
    return false;
  }
    
  uint8_t buffer[32];    
  uint16_t sum = 0;
  pmsSerial.readBytes(buffer, 32);
 
  // get checksum ready
  for (uint8_t i=0; i<30; i++) {
    sum += buffer[i];
  }
 
  /* debugging
  for (uint8_t i=2; i<32; i++) {
    Serial.print("0x"); Serial.print(buffer[i], HEX); Serial.print(", ");
  }
  Serial.println();
  */
  
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i=0; i<15; i++) {
    buffer_u16[i] = buffer[2 + i*2 + 1];
    buffer_u16[i] += (buffer[2 + i*2] << 8);
  }
 
  // put it into a nice struct :)
  memcpy((void *)&data, (void *)buffer_u16, 30);
 
  if (sum != data.checksum) {
    Serial.println("Checksum failure");
    return false;
  }
  // success!
  return true;
}

/*
*   RTC Functions
*/

bool getTime(const char *str)
{
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  tm.Hour = Hour;
  tm.Minute = Min;
  tm.Second = Sec;
  return true;
}

bool getDate(const char *str)
{
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3) return false;
  for (monthIndex = 0; monthIndex < 12; monthIndex++) {
    if (strcmp(Month, monthName[monthIndex]) == 0) break;
  }
  if (monthIndex >= 12) return false;
  tm.Day = Day;
  tm.Month = monthIndex + 1;
  tm.Year = CalendarYrToTm(Year);
  return true;
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}

