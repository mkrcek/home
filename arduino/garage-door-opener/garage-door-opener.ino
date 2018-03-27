
// ---------------------------------------------------------------------------
// Garage Door Opener by HTTP GET & POST + Apple HOMEKIT
//    DEVICE
// ---------------------------------------------------------------------------

/*
Functionality:
- open door
- close door
- report all States (Open/Closed/Opening/Closing/Stop/Obstruction) 
- report percentage of opening
- set size (lenght) of garage door: GarageMin, GarageMax, MeasureDelay by HTTP POST
    look at >>handleConfigSetDoor<<
- compatible with Apple HOMEKIT
    look at >>server garage-door-opener.go<<


TODO
- stop at target position (e.g. 40%)

NOTE:
- inside the code is more functinality - not used in this case. E.G. SWITCH, LAMP, LED PWM, PIR senson, Light Sensor

*/

#include <NewPing.h>    //https://bitbucket.org/teckel12/arduino-new-ping/wiki/Home

#include <math.h>       

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>    //JSON https://bblanchon.github.io/ArduinoJson/


//                 PIN:{ 0, 1, 2, 3, 4,  5,  6,  7,  8 }    | PIN cislo [5] = LED modrá na boardu
int digitalPins[] = {16, 5, 4, 0, 2, 14, 12, 13, 15};   //mapování PINů pro RobotDyn Wifi D1R2: https://www.wemos.cc/product/d1.html
int pwmPins[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //uložení hodnoty pwm, když je (-1), tak není pwm nastaveno
String namePins[] = { "", "", "", "", "", "Motor", "ultrasonic_Echo", "ultrasonic_Trigger", ""};  //popis
String locationPins[] = { "", "", "", "", "", "", "", "", ""};        //popis umístění
bool digitalWritePins[] = { false, false, false, false, false, true, false, false, false};   //piny 5 jsou vystupní
bool relayOnHighPins[] = { true, true, true, true, true, true, true, true, true}; //čím se sepne relé true = HIGH

bool inusePin[] = { false, false, false, false, false, true, true, true, false};       //jeli pin používán
int clickPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-click se přepne uvedený PIN. -1 je nenastaveno
int click2Pin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-double click se přepne uvedený PIN. -1 je nenastaveno
int clickHoldStartPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-Začne držet ... se přepne uvedený PIN. -1 je nenastaveno
int clickHoldDoPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-Drží se ....se přepne uvedený PIN. -1 je nenastaveno
int clickHoldEndPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-Pustí držení .... se přepne uvedený PIN. -1 je nenastaveno



// ************* ARDUINO BORAD configuration *****

String deviceSwVersion = "2018-03-27";
String deviceBoard = "RobotDyn Wifi D1R2";
char deviceId[] = "esp8266garage";
String deviceName = "GarageController";
String deviceLocation = "Garage";


// ************* LOCAL WIFI configuration *****
/* 
 * Wifi credential are stored in local config file, the same local folder. See example file called "wifi_config_example"
 * Filename: wifi_config.h
 * And inside of the file are those lines
 * #define ssid_config "your-wifi-ssid-name" 
 * #define pws_config "your-wifi-ssid-password")
 * //end the SERVER IP adress :
 * #define target_Server "192.168.0.100"
*/
#include "wifi_config.h"   
const char* ssid = ssid_config; 
const char* password = pws_config;

IPAddress arduinoIP (arduino_IP1, arduino_IP2, arduino_IP3, arduino_IP4); //static IP address
IPAddress gateway (gateway_IP1, gateway_IP2, gateway_IP3, gateway_IP4); // set gateway to match your network
IPAddress subnet(255, 255, 0, 0); // set subnet mask to match your network


//changable via HTTP POST to 192.168.0.44/device {"targetServer":"192.168.110.117","httpPort":9091}
 char targetServer[] = target_Server;    //server IP address - where he is listening
 int httpPort = target_Server_Port;     //and port to listen to
  


// ************* Garage Door configuration *****
  const int CurrentDoorStateOpen = 0;
  const int CurrentDoorStateClosed = 1;
  const int CurrentDoorStateOpening = 2;
  const int CurrentDoorStateClosing = 3;
  const int CurrentDoorStateStopped = 4;
  const int TargetDoorStateOpen = 0;
  const int TargetDoorStateClosed = 1;
  
  int DoorMaxCm = 100; //257;    //MAX: 258-260 test: 70 | real position in cm, when the door are open    
  int DoorMinCm = 8;      //MIX: 8 | test: 20 | real position in cm, when the door are closed 
  
  int CurrentDoorState = CurrentDoorStateClosed;
  int TargetDoorState = TargetDoorStateClosed;
  int TargetDoorOpen = 0;   //how many percents you want to open the door? Target positoin.
  int doorPositionLast = 100; //Last door position 
  int doorLenght = DoorMaxCm - DoorMinCm; //Size of the door 
  int doorMeasureTimeDelay = 1000;    //how often will be measered the position of the garade door
                                     // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
                                     //slow door => higher value, fast door => smaller
  int doorMeasureTimeTemp = 0;    //temp time
  int arduinoTime = 0;            //arduino real time
  

  bool ObstructionDetected = false; //ne nějaká překážka ve vratech? Pokud ano, tak true
  bool doorIsMoving = false;        //jsou li vrata v pohybu , tak true
  bool changeDoorState = false;     //požadavek uživatele na změnu pozice dveří

  int realDoorPosition = 0;     //skutečná vzdálenost naměřená v cm
  int doorPosition = 0 ;         //přepočítaná vzdálenost na procenta

  int NewTargetDoorState = TargetDoorState;    //nová pozice zadaná uživatelem
  int NewTargetDoorOpen = TargetDoorStateClosed;     //nová pozice zadaná uživatelem

  int historyDoorPosition[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  //last 10 positions of the door - use as a filter and count average position
  int historyDoorDirection[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  //last 10 directions of the door - 0=same, -1 decreasing, +1 increasing
  int averageDoorDirection = 0;

  
  
// ************* ULTRASONIC configuration *****
//ULTRASONIC senson on +5V, 3.3 is not enought

#define GARAGE_MOTOR_PIN  5  // Arduino pin with relay for GARAGE MOTOR-ENGINE
#define ECHO_PIN     digitalPins[6]  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define TRIGGER_PIN  digitalPins[7]  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define MAX_DISTANCE 300 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.




// ************* inicialisation of the serven on the port: 80 *****
ESP8266WebServer server(80);



int newAverageDoorPosition (int p) {      //get new average position & diretion from history of measurements
    int historyLength = 5;   //max 10 size of array
//    Serial.print("history: ");
    float avrg = 0.0;       //to get the right numbers 
    float avDir = 0.0;
    
    for (int i = 0; i < (historyLength-1) ; i++) {
      historyDoorPosition[i] = historyDoorPosition[i+1];
      historyDoorDirection[i] = historyDoorDirection [i+1];
      avrg = avrg + historyDoorPosition[i];
      avDir = avDir + historyDoorDirection[i];
//      Serial.print(historyDoorPosition[i]);
//      Serial.print("/");
//      Serial.print(historyDoorDirection[i]);
//      Serial.print(",");
    }
    
    if (historyDoorPosition[(historyLength-1)] > p) { 
        historyDoorDirection[(historyLength-1)] = 1;
    }
    if (historyDoorPosition[(historyLength-1)] < p) { 
        historyDoorDirection[(historyLength-1)] = -1;
    }
    if (historyDoorPosition[(historyLength-1)] == p) { 
        historyDoorDirection[(historyLength-1)] = 0;
    }
    
    historyDoorPosition[(historyLength-1)] = p;
//    Serial.print(historyDoorPosition[(historyLength-1)]);
//    Serial.print("/");
//    Serial.print(historyDoorDirection[(historyLength-1)]);
//    Serial.print(" = ");
    avrg = avrg + historyDoorPosition[(historyLength-1)];
    avrg = avrg / historyLength;
    avrg = round(avrg);                   // and round it
//    Serial.print(avrg);
//    Serial.print(" // ");
    
    avDir = avDir + historyDoorDirection[(historyLength-1)];
    avDir = avDir / historyLength;
    avDir = round (avDir);
    averageDoorDirection = avDir;  
//    Serial.print(avDir);
//    Serial.print(" // ");
       
    return (int) avrg;    //
}


void myGaradeDoor() {     //manager for garame-door-opener
  
  //get real - actual door position
  realDoorPosition = sonar.ping_cm();     //get real lenght of the door in cm 
   
  realDoorPosition = newAverageDoorPosition(realDoorPosition);
  doorPosition = (realDoorPosition - DoorMinCm) *100 / doorLenght ;         //re-count from cm to percentage
  if (doorPosition < 0)   { doorPosition = 0; }
  if (doorPosition > 100) {doorPosition = 100; }
  doorPosition = 100 - doorPosition; //revert the orientation. (the shortest distance = garage is open) 


//  Serial.print("real: ");
//  Serial.print( realDoorPosition ); // real door position in cm
//  Serial.print("cm, ");
//  Serial.print( doorPosition ); // door position in %
//  Serial.print(" %, ");
//  if (doorIsMoving) { 
//      Serial.print("door-is-moving"); 
//    } else { 
//      Serial.print("door-is-NOT-moving"); 
//  }
//  

//new request to MOVE the door?  
  if (changeDoorState == true) { 
    if (NewTargetDoorState == CurrentDoorState) {  //if door is in target position = dont do anything
       TargetDoorState = NewTargetDoorState;
       changeDoorState = false;
     } else {                                      //if door is NOT in target position = start motor
       TargetDoorState = NewTargetDoorState;
//       Serial.println("");
//       Serial.println("RUN THE ENGINE / MOTOR - Wrrrrrrw");
//       Serial.println("");
       PressRelayButton(GARAGE_MOTOR_PIN);
       changeDoorState = false;
       doorIsMoving = true;
     }
  } //end IF 
  

//measure and report the position 

if (doorPosition <= 5) {                        //if the door is CLOSED (+/- 5 % )
      CurrentDoorState = CurrentDoorStateClosed;
      ObstructionDetected = false;
      doorPositionLast = doorPosition;
//     Serial.println("....CLOSED");
      if ( TargetDoorState == TargetDoorStateClosed ) {
        doorIsMoving = false;
      }
    } else {
    if (doorPosition >= 95) {                      //if the door is OPEN (+/- 5 % )
      CurrentDoorState = CurrentDoorStateOpen;
      ObstructionDetected = false;
      doorPositionLast = doorPosition;
//      Serial.println("....OPEN");
      if ( TargetDoorState == TargetDoorStateOpen ) {
          doorIsMoving = false;
        }
      } else {
          if (averageDoorDirection == 1) {
            CurrentDoorState = CurrentDoorStateOpening;
            ObstructionDetected = false;
            doorPositionLast = doorPosition;
//            Serial.println("....opening UP");
            }
          if (averageDoorDirection == -1) {
            CurrentDoorState = CurrentDoorStateClosing;
            ObstructionDetected = false;
            doorPositionLast = doorPosition;
//            Serial.println("....closing DOWN");
            }  
          if (averageDoorDirection == 0) {           //if door STOPed
            if (doorPosition == TargetDoorOpen) {
              CurrentDoorState = CurrentDoorStateStopped;
              ObstructionDetected = false;
              doorPositionLast = doorPosition;
//              Serial.println("....Target position");
            } else {
              CurrentDoorState = CurrentDoorStateStopped;
              ObstructionDetected = true;
              doorPositionLast = doorPosition;
//              Serial.println("....Obstruction Detected");
            }
          } //end if-STOPed
        }
    } //end of the loooong IF->ELSE 
}  


void handleGetDoor() {    //information about DOOR to HTTP GET

  //nastavení hlavicky
  server.sendHeader("Connection", "keep-alive"); //BODY bude delší, tak se nenastavuje na NULU
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");

  String message = "";

  {

    //==JSON vytvoreni obsahu ==
    StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
    JsonObject& root = jsonBuffer.createObject();
    root["door"] = doorPosition;
    root["CurrentDoorState"] = CurrentDoorState;
    root["TargetDoorState"] = TargetDoorState;
    root["TargetDoorOpen"] = TargetDoorOpen;
    root["ObstructionDetected"] = ObstructionDetected;

    //==JSON generuje vystup ==
    int len = root.measureLength() + 1; //delka cele zpravy
    char rootstr[len];
    root.printTo(rootstr, len);         //vygeneruje JSON
    message += rootstr;

    server.send(200, "application/javascript", message);
  }
}

void handleSetDoor() {     //set OPEN/CLOSE door ... later on the Target Position like 40%

  String message = "";
  //==JSON cteni obsahu ==
  StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.parseObject(server.arg(0));   //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  if (!root.success())  {           //když je špatně JSON
    server.send(400, "text/plain", "Nepodarilo se precist JSON");
    return;
  }
  // Retrieve the values
  NewTargetDoorState = root["TargetDoorState"];    //set open / close
  NewTargetDoorOpen = root["TargetDoorOpen"];      //set target postion like 40 %  - not implemented yet
  //==
  
  changeDoorState = true;                         //New task: change the postion 
  
  server.send(200, "text/plain", "Konfigurace nastavena");


} //konec fce


void handleConfigSetDoor() {     //Set new values for Max and Mix for garage door
  /*  
   *  HOW TO DO IT:
   * 1st: setup max value : 
   * - close the garage door - max Length from ultrasonic sensor  
   * - send POST {"setDoorMax":true, "setDoorMin":false}
   * when it is done, do:
   * 2nd: setup min value: 
   * - open the garage door - min Length from ultrasonic sensor  
   * - send POST {"setDoorMax":false, "setDoorMin":true}
   * 
   * to set Delay
   * POST :  {"setDoorMax":false, "setDoorMin":false, "setDoorDelay":1000}
  */
  String message = "";
  bool setDoorMax = false;      //I want to setup Max 
  bool setDoorMin = false;      //I want to setup Mim
  int setDoorDelay = 0;         //Delay for measure
  
  //==JSON cteni obsahu ==
  StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.parseObject(server.arg(0));   //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  if (!root.success())  {           //when JSON is in not right format - error
    server.send(400, "text/plain", "Nepodarilo se precist JSON");
    return;
  }
  // Retrieve the values
  setDoorMax = root["setDoorMax"];    //I want to setup Max: set up one-by-one
  setDoorMin = root["setDoorMin"];      //I want to setup Min - set up one-by-one
  setDoorDelay = root["setDoorDelay"];      //how often will be measered the position of the garade door, min 50ms

  //==

  if (setDoorMax) {
    DoorMaxCm = sonar.ping_cm();     //in v cm  
  }
  if (setDoorMin) {
    DoorMinCm = sonar.ping_cm();     //in v cm  
  }
  doorLenght = DoorMaxCm - DoorMinCm; //calculate new Size/Lenght of the door 

  if (setDoorDelay > 50) {
    doorMeasureTimeDelay = setDoorDelay;
    }
  
  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce

void handleGetConfigDoor() { //Get configuration of curent Max and Mix for garage door

  
  //nastavení hlavicky
  server.sendHeader("Connection", "keep-alive"); //BODY bude delší, tak se nenastavuje na NULU
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");

  String message = "";

  {

    //==JSON vytvoreni obsahu ==
    StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
    JsonObject& root = jsonBuffer.createObject();
    root["DoorMaxCm"] = DoorMaxCm;
    root["DoorMinCm"] = DoorMinCm;
    root["setDoorDelay"] = doorMeasureTimeDelay;
    

    //==JSON generuje vystup ==
    int len = root.measureLength() + 1; //size of the message
    char rootstr[len];
    root.printTo(rootstr, len);         //generate JSON
    message += rootstr;

    server.send(200, "application/javascript", message);
  }

} //konec fce

void handleOptionsDoor() {   //korekce pro CORS
  //pokud je POST z jiné "domeny" - což je skoro vzdy viz CORS https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request

  server.sendHeader("Content-Length", "0");
  server.sendHeader("Connection", "keep-alive");

  //server.sendHeader("Access-Control-Allow-Origin", "*");  //možná tady je podruhé?
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");
  //server.send(200, "text/plain", "");

  server.send(204);
}






void handleGetDigiPin() {     //zjistí stav digitalního PINu

  String message = "";

  //==cislo PINu z URL
  String urlUri = server.uri();
  char uriArray[] = "/pins/x";       //přečte pořadí PINU: x
  int pinNumber;                    //prevede na cislo
  urlUri.toCharArray(uriArray, 10);       //nakupíruje text do pole: Copies the string's characters to the supplied buffer. (9 je malo)
  pinNumber = atoi(&uriArray[6]);        //z pole zjistí cislo - převede znak na číslo (x) - jen jednociferne
  //==

  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<200> jsonBuffer;                           //Reserve memory space
  int value  = 0;
  value = digitalRead(digitalPins[pinNumber]);
  JsonObject& root = jsonBuffer.createObject();

  if (relayOnHighPins[pinNumber]) {
    value = value;
  } else {
    if (value == 1) {
      value = 0;
    } else {
      value = 1;
    }
  }

  root["value"] = value;

  if (pwmPins[pinNumber] >= 0) {
    root["pwm"] = pwmPins[pinNumber];
  }
  //==

  //==JSON generuje vystup ==
  int len = root.measureLength() + 1; //delka cele zpravy
  char rootstr[len];
  root.printTo(rootstr, len);         //vygeneruje JSON
  message += rootstr;
  //==

  server.send(200, "text/plain", message);

} //konec fce

void handleSetDigiPin() {     //nastaví digitální PIN na 0/1 nebo PWM

  String message = "";

  //==cislo PINu z URL
  String urlUri = server.uri();
  char uriArray[] = "/pins/x";       //přečte hodnotu PINU: x
  int pinNumber;                    //prevede na cislo
  urlUri.toCharArray(uriArray, 10);       //nakupíruje text do pole: Copies the string's characters to the supplied buffer. (8 je malo)
  pinNumber = atoi(&uriArray[6]);        //z pole zjistí cislo - převede znak na číslo (x) - jen jednociferne
  //==


  //==JSON cteni obsahu ==
  StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.parseObject(server.arg(0));   //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  if (!root.success())  {           //když je špatně JSON
    server.send(400, "text/plain", "Nepodarilo se precist JSON");
    return;
  }
  // Retrieve the values
  String pinIdS  = root["pinId"];     //nepouziva se, nahrazeno (x) z  url "/pins/x
  String valueS  = root["value"];
  String pwmS     = root["pwm"];
  //==


  //==vlastní funkcnost zapinani

  //nově
  int pwm = pwmS.toInt();

  if (pwm > 0) {                 //
    pwmPins[pinNumber] = pwm;
    analogWrite (digitalPins[pinNumber], pwm);
  } else {                          //kdyz PWM je nastaveno na 0
    if (pwmPins[pinNumber] >= 0) {                  //vypne pwm tím, ze nastavi na 0
      analogWrite (digitalPins[pinNumber], 0);
      pwmPins[pinNumber] = -1;
    }
    int value = valueS.toInt();                     //teprve potom muze nastavit ON/OFF
    if (relayOnHighPins[pinNumber]) {
      digitalWrite(digitalPins[pinNumber], value);
    } else {
      if (value == 1) {
        digitalWrite(digitalPins[pinNumber], 0);
      } else {
        digitalWrite(digitalPins[pinNumber], 1);
      }
    }
  } //konec else (když pwm == 0)



  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce

void handleOptionsDigiPin() {   //korekce pro CORS
  //pokud je POST z jiné "domeny" - což je skoro vzdy viz CORS https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request

  server.sendHeader("Content-Length", "0");
  server.sendHeader("Connection", "keep-alive");


  //server.sendHeader("Access-Control-Allow-Origin", "*");  //možná tady je podruhé?
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");
  //server.send(200, "text/plain", "");

  server.send(204);
}

void handleGetDevice() {    //informace o zařízení - celé arduino desce

  //nastavení hlavicky
  // TADY NE: server.sendHeader("Content-Length", "0"); //BODY bude delší, tak se nenastavuje na NULU
  server.sendHeader("Connection", "keep-alive"); //BODY bude delší, tak se nenastavuje na NULU
  //server.sendHeader("Access-Control-Allow-Origin","*");    //někde je 2x * - možná tady? Pokud by nefungovalo, ta toto odkomentovat
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");

  String message = "";
  String mDNS = deviceId;

  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<300> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.createObject();

  root["deviceName"] = deviceName;
  root["deviceLocation"] = deviceLocation;

  //jen pro cteni:
  root["deviceSwVersion"] = deviceSwVersion;
  root["deviceBoard"] = deviceBoard;
  root["deviceIP"] = WiFi.localIP().toString();
  root["devicemDNS"] = mDNS + ".local" ;
  root["deviceId"] = deviceId;
  root["ssid"] = ssid;
  root["targetServer"] = targetServer;
  root["httpPort"] = httpPort;
  
  //==

  //==JSON generuje vystup ==
  int len = root.measureLength() + 1; //delka cele zpravy
  char rootstr[len];
  root.printTo(rootstr, len);         //vygeneruje JSON
  message += rootstr;

  //==ktedy formát?
  //server.send(200, "text/plain", message);
  //server.send(200, "application/json", message);
  //server.send(200, "text/json", message);
  server.send(200, "application/javascript", message);

}

void handleOptionsDevice() {
  //pokud je POST z jiné "domeny" - což je skoro vzdy viz CORS https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request

  server.sendHeader("Content-Length", "0");
  server.sendHeader("Connection", "keep-alive");


  //server.sendHeader("Access-Control-Allow-Origin", "*");  //někde je 2x * - možná tady? Pokud by nefungovalo, ta toto odkomentovat
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");
  //server.send(200, "text/plain", "");

  server.send(204);   //prázdná odpoveď
}

void handlePostDevice() {     //nastaví informace o zařízení(desce( podle přijatého JSON)

  //nastavení hlavicky
  //server.sendHeader("Content-Length", "0"); //BODY bude delší, tak se nenastavuje na NULU
  server.sendHeader("Connection", "keep-alive");
  //server.sendHeader("Access-Control-Allow-Origin", "*");  //někde je 2x * - možná tady? Pokud by nefungovalo, ta toto odkomentovat
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");


  String message = "";
  message += server.arg(0);


  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space

  JsonObject& root = jsonBuffer.parseObject(server.arg(0));   //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  if (!root.success())  {           //když je špatně JSON
    message += "\nNepodarilo se precist JSON";
    server.send(400, "text/plain", message);
    return;
  }
  //   Retrieve the values
  String deviceNameS         = root["deviceName"];
  String deviceLocationS     = root["deviceLocation"];

//example: HTTP POST {"targetServer":"192.168.110.117","httpPort":9091}
//  String targetServerS = root["targetServer"] ;
//  targetServer = targetServerS;
//  httpPort = root["httpPort"] ;
  String servername = root["targetServer"] ;
  servername.toCharArray (targetServer, servername.length()+1);
  httpPort = root["httpPort"] ;
  
  //==

  if (deviceNameS != "") {
    deviceName = deviceNameS;           //ulozi do globalnich promennych
  }

  if (deviceLocationS != "") {
    deviceLocation = deviceLocationS;          //ulozi do globalnich promennych
  }

  server.send(200, "text/plain", "Konfigurace nastavena");

}



void handleOptionsPin() {   //korekce pro CORS
  //pokud je POST z jiné "domeny" - což je skoro vzdy viz CORS https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request

  server.sendHeader("Content-Length", "0");
  server.sendHeader("Connection", "keep-alive");


  //server.sendHeader("Access-Control-Allow-Origin", "*");  //možná tady je podruhé?
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");
  //server.send(200, "text/plain", "");

  server.send(204);
}

void handleConfigPin() {    //konfigurace jednotlivých pinů

  //==cislo PINu z URL
  String urlUri = server.uri();
  char uriArray[] = "/pins/x/config";       //přečte hodnotu PINU: x
  int pinNumber;                    //prevede na cislo
  urlUri.toCharArray(uriArray, 10);       //nakupíruje text do pole: Copies the string's characters to the supplied buffer. (8 je malo)
  pinNumber = atoi(&uriArray[6]);        //z pole zjistí cislo - převede znak na číslo (x) - jen jednociferne
  //==

  //==JSON cteni obsahu ==
  //  StaticJsonBuffer<200> jsonBuffer;                           //Reserve memory space
  //  JsonObject& root = jsonBuffer.parseObject(server.arg(0));   //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  //  if (!root.success())  {           //když je špatně JSON
  //    server.send(400, "text/plain", "Nepodarilo se precist JSON");
  //    return;
  //  }

  //==JSON cteni obsahu ==
  const size_t bufferSize = JSON_OBJECT_SIZE(10) + 180; //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  DynamicJsonBuffer jsonBuffer(bufferSize);             //Reserve memory space
  JsonObject& root = jsonBuffer.parseObject(server.arg(0));
  if (!root.success())  {           //když je špatně JSON
    server.send(400, "text/plain", "Nepodarilo se precist JSON");
    return;
  }


  // Retrieve the values
  String nameS            = root["name"];
  String locationS        = root["location"];
  String DigitalWriteS    = root["digitalWrite"];
  String RelayOnHighS     = root["relayOnHigh"];
  bool inUseS = root["inUse"];

  String bClickS           = root["buttonClick"];   //ktery pin se prepne pri kliknuti
  String bClickS2          = root["buttonDoubleClick"];   //ktery pin se prepne pri kliknuti doubleclick
  String bClickHStart      = root["buttonHoldStart"];   //ktery pin se prepne pri začátku držení
  String bClickH           = root["buttonHold"];   //ktery pin se prepne pri při držení
  String bClickHEnd        = root["buttonHoldEnd"];   //ktery pin se prepne pri ukončení držení

  //==


  //==vlastní funkcnost zapinani

  if (inUseS) {                 //je li pin aktivní - viditelný - použitý
    inusePin[pinNumber] = true;
  } else {
    inusePin[pinNumber] = false;
  }

  if (nameS != "") {                 //zmeni jmeno
    namePins[pinNumber] = nameS;
  }

  if (locationS != "") {                 //zmeni lokalitu
    locationPins[pinNumber] = locationS;
  }

  if (DigitalWriteS != "") {
    //zmeni mode PINu: DigitalWrite = true, nebo DigitalRead=false
    if (DigitalWriteS == "true") {
      pinMode(digitalPins[pinNumber], OUTPUT);
      digitalWritePins[pinNumber] = true;
    } else {          //jedná se o tlačítko
      //nastaví se PIN jako vstupní
      digitalWritePins[pinNumber] = false;
      pinMode(digitalPins[pinNumber], INPUT_PULLUP);
      digitalWrite(digitalPins[pinNumber], LOW );
    }
  }

  if (RelayOnHighS != "") {                 //zmeni cim se spina rele na PINu (HIGH)
    if (RelayOnHighS == "true") {
      relayOnHighPins[pinNumber] = true;
    } else {
      relayOnHighPins[pinNumber] = false;
    }
  }

  //if (bClickS != "") {                 //nastaveni pinů pro tlačítko, které se ovlada
  //      clickPin[pinNumber] = bClickS.toInt();  //podle parametru se prevede click.PIN na cislenou hodnutu, uschová se nastavení tlacitko-click // -1 je nenastaveno
  //  }

  if (bClickS != "") {                 //nastaveni pinů pro tlačítko, které se ovlada
    clickPin[pinNumber] = bClickS.toInt();  //podle parametru se prevede click.PIN na cislenou hodnutu, uschová se nastavení tlacitko-click // -1 je nenastaveno
  }

  if (bClickS2 != "") {                 //nastaveni pinů pro tlačítko, které se ovlada
    click2Pin[pinNumber] = bClickS2.toInt();  //podle parametru se prevede click.PIN na cislenou hodnutu, uschová se nastavení tlacitko-click // -1 je nenastaveno
  }

  if (bClickHStart != "") {                 //nastaveni pinů pro tlačítko, které se ovlada
    clickHoldStartPin[pinNumber] = bClickHStart.toInt();  //podle parametru se prevede click.PIN na cislenou hodnutu, uschová se nastavení tlacitko-click // -1 je nenastaveno
  }

  if (bClickH != "") {                 //nastaveni pinů pro tlačítko, které se ovlada
    clickHoldDoPin[pinNumber] = bClickH.toInt();  //podle parametru se prevede click.PIN na cislenou hodnutu, uschová se nastavení tlacitko-click // -1 je nenastaveno
  }

  if (bClickHEnd != "") {                 //nastaveni pinů pro tlačítko, které se ovlada
    clickHoldEndPin[pinNumber] = bClickHEnd.toInt();  //podle parametru se prevede click.PIN na cislenou hodnutu, uschová se nastavení tlacitko-click // -1 je nenastaveno
  }





  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce

void handleGetConfigPin() { //načtení konfigurace jednotlivých pinů

  String message = "";

  //==cislo PINu z URL
  String urlUri = server.uri();
  char uriArray[] = "/pins/x/config";       //přečte hodnotu PINU: x
  int pinNumber;                    //prevede na cislo
  urlUri.toCharArray(uriArray, 10);       //nakupíruje text do pole: Copies the string's characters to the supplied buffer. (8 je malo)
  pinNumber = atoi(&uriArray[6]);        //z pole zjistí cislo - převede znak na číslo (x) - jen jednociferne
  //==

  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<200> jsonBuffer;                           //Reserve memory space

  int value  = 0;
  value = digitalRead(digitalPins[pinNumber]);
  JsonObject& root = jsonBuffer.createObject();

  root["name"] = namePins[pinNumber];
  root["location"] = locationPins[pinNumber];
  root["digitalWrite"] = digitalWritePins[pinNumber];
  root["relayOnHigh"] = relayOnHighPins[pinNumber];
  root["inUse"] = inusePin[pinNumber];
  root["buttonClick"] = clickPin[pinNumber];
  root["buttonDoubleClick"] = click2Pin[pinNumber];
  root["buttonHoldStart"] = clickHoldStartPin[pinNumber];
  root["buttonHold"] = clickHoldDoPin[pinNumber];
  root["buttonHoldEnd"] = clickHoldEndPin[pinNumber];

  //==

  //==JSON generuje vystup ==
  int len = root.measureLength() + 1; //delka cele zpravy
  char rootstr[len];
  root.printTo(rootstr, len);         //vygeneruje JSON
  message += rootstr;
  //==

  server.send(200, "text/plain", message);

} //konec fce

void handleGetPinList() {   //informace o používaných PINech (inUse)

  String message = "";
  int i;
  int pinCount = 0;

  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& data = root.createNestedArray("pin");

  pinCount = sizeof(digitalPins) / sizeof(digitalPins[0]);  //sizeof je mnozstvi bytu v digitalPins, ne pocet elementu. Proto se musi vydelit
  for (i = 0; i < pinCount ; i++) {
    if (inusePin[i]) {
      data.add(i);
    }
  }
  //==

  //==JSON generuje vystup ==
  int len = root.measureLength() + 1; //delka cele zpravy
  char rootstr[len];
  root.printTo(rootstr, len);         //vygeneruje JSON
  message += rootstr;
  //==

  server.send(200, "text/plain", message);

}


void handleRoot() {         //help v rootu IP adresy
  String message = "";

  message += "<h1>Door meeter </h1>";
  message += "<p>Verze: " + String(deviceSwVersion) + "</p>";
  message += "<p>IP adresa: http://" +  WiFi.localIP().toString() + "</p>";
  message += "<p>mDNS adresa: http://" +  String(deviceId) + ".local</p>";

  message += "<h2>Popis REST API</h2>";
  message += "<p>Zobrazeni vzdalenosti k predmetu,";
  message += "<a href=\"http://\\" + String(deviceId) + ".local/door \">http://" +  String(deviceId) + ".local/door</a></p>";
  
  message += "<p>PIN 5 = relé otevreni vrat</p>";
 

  server.send(200, "text/html", message);
}

void handleNotFound() {     //pokud URL není nalezeno, používám pro laděni REST API
  String message = "";
  message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void resetPins() {          //prvotní nastavení - nastav vystupni PIN na vypnuto


  int pinCount = 0;
  pinCount = sizeof(digitalPins) / sizeof(digitalPins[0]);  //sizeof je mnozstvi bytu v digitalPins, ne pocet elementu. Proto se musi vydelit
  for (int i = 0; i < pinCount ; i++) {

    if (digitalWritePins[i]) {          //nastaví pouze ty PINy, které jsou nasteveny jako výstupní
      pinMode(digitalPins[i], OUTPUT);
      if (relayOnHighPins[i]) {         //pokud je vystupní rele spinano 1, tak nastaví 0, jinak obracene
        digitalWrite(digitalPins[i], 0);
      } else {
        digitalWrite(digitalPins[i], 1);
      }
    }

  }
}

void PressRelayButton(int pinNumber) {                  //PIN nastaví na zapnuto a na 1/2sekundu na vypnuto (zohlední i relayOnHighPins )
  if (relayOnHighPins[pinNumber]) {
    digitalWrite(digitalPins[pinNumber], 1);
    delay (500);
    digitalWrite(digitalPins[pinNumber], 0);
  } else {
    digitalWrite(digitalPins[pinNumber], 0);
    delay (500);
    digitalWrite(digitalPins[pinNumber], 1);
  }
}




void setup() {
  Serial.begin(9600); // Open serial monitor at 115200 baud to see ping results.

  resetPins();      //nastaví digitální vystupní piny na vystupni a vypne je
  //nezpůsobí to problém se Ultrazvukem ??



 // ******* nastavení a připojení WIFI
 //dns adresa
  //WiFi.begin(ssid, password);
  //Serial.println("");


  // ******* nastavení a připojení WIFI
  //Unlike WiFi.begin() which automatically configures the WiFi shield to use DHCP, WiFi.config() allows you to manually set the network address of the shield.
  //https://www.arduino.cc/en/Reference/WiFiConfig
  WiFi.config(arduinoIP, gateway, subnet);
  WiFi.mode(WIFI_STA);      //není AP a nezobrazuje své SSID
  WiFi.begin(ssid, password);
  Serial.println("");
  
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }


  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(deviceId)) {       //mDNS je podporovano! Na deviceId.local je k nalezeni tento DEVICE
    Serial.println("MDNS responder started");
  }

// ******* nastavení poslouchání na GET/POST

  server.on("/", handleRoot);         //zde je i zakladni help a popis
  server.on("/door", HTTP_GET, handleGetDoor);  //informace o dveřích
  server.on("/door", HTTP_POST, handleSetDoor); //nastaví otvirani dveří
  server.on("/door", HTTP_OPTIONS, handleOptionsDoor);  //option pro dveře
  server.on("/door/config", HTTP_GET, handleGetConfigDoor);  //informace o dveřích
  server.on("/door/config", HTTP_POST, handleConfigSetDoor); //nastaví otvirani dveří
  server.on("/door/config", HTTP_OPTIONS, handleOptionsDoor);  //option pro dveře




  server.on("/device", HTTP_GET, handleGetDevice);  //informace o zařízení
  server.on("/device", HTTP_POST, handlePostDevice);  //nastavi o zařízení
  server.on("/device", HTTP_OPTIONS, handleOptionsDevice);  //nastavi o zařízení

  server.on("/pins", handleGetPinList);            //vypise seznam pinů

  //cteni PINu pomoci GET
  server.on("/pins/5", HTTP_GET, handleGetDigiPin);             //Garage Door Engine

  //nastaveni PINu pomoci POST
  server.on("/pins/5", HTTP_POST, handleSetDigiPin);            //Garage Door Engine

  //osetreni stavu CORS, pomoci OPTIONS
  server.on("/pins/5", HTTP_OPTIONS, handleOptionsDigiPin);     //Garage Door Engine



 //cteni konfigurace jednotlivých PINu pomoci GET
  server.on("/pins/0/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/1/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/2/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/3/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/4/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/5/config", HTTP_GET, handleGetConfigPin);    //Garage Door Engine
  //server.on("/pins/6/config", HTTP_GET, handleGetConfigPin);    //Garage Door
  //server.on("/pins/7/config", HTTP_GET, handleGetConfigPin);    //Garage Door
  server.on("/pins/8/config", HTTP_GET, handleGetConfigPin);


  //konfigurace jednotlivých PINu pomoci POST
  server.on("/pins/0/config", HTTP_POST, handleConfigPin);
  server.on("/pins/1/config", HTTP_POST, handleConfigPin);
  server.on("/pins/2/config", HTTP_POST, handleConfigPin);
  server.on("/pins/3/config", HTTP_POST, handleConfigPin);
  server.on("/pins/4/config", HTTP_POST, handleConfigPin);
  server.on("/pins/5/config", HTTP_POST, handleConfigPin);    //Garage Door Engine
  //server.on("/pins/6/config", HTTP_POST, handleConfigPin);  //Garage Door
  //server.on("/pins/7/config", HTTP_POST, handleConfigPin);  //Garage Door
  server.on("/pins/8/config", HTTP_POST, handleConfigPin);

  //osetreni stavu CORS, pomoci OPTIONS
  server.on("/pins/0/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/1/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/2/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/3/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/4/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/5/config", HTTP_OPTIONS, handleOptionsPin);  //Garage Door Engine
  //server.on("/pins/6/config", HTTP_OPTIONS, handleOptionsPin);  //Garage Door
  //server.on("/pins/7/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/8/config", HTTP_OPTIONS, handleOptionsPin);



  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");



}



void loop() {


 
 //  GARAGE Door Position
  arduinoTime = millis();         // get Arduino real time
  if (arduinoTime > doorMeasureTimeTemp + doorMeasureTimeDelay) {         //read GarageDoorPosition every: doorMeasureTimeDelay
    myGaradeDoor();                                                       //run Garage Door Manager :-)
    doorMeasureTimeTemp = arduinoTime;
  }



  //listening on HTTP  for GET & POST
  server.handleClient();

  
}
