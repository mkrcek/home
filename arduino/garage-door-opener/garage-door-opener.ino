/*
2017/11/03

TODO
- translate comments to English
- change direction (100=0 and 0 is 100 (open is close)
- add and config params (mix/max) via CONFIG

*/

// ---------------------------------------------------------------------------
// Garage Door Opener by HTTP GET and HTTP POST
// ---------------------------------------------------------------------------

#include <NewPing.h>    //https://bitbucket.org/teckel12/arduino-new-ping/wiki/Home

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>    //knihovna na JSON https://bblanchon.github.io/ArduinoJson/


//                 PIN:{ 0, 1, 2, 3, 4,  5,  6,  7,  8 }    | PIN cislo [5] = LED modrá na boardu
int digitalPins[] = {16, 5, 4, 0, 2, 14, 12, 13, 15};   //mapování PINů pro RobotDyn Wifi D1R2: https://www.wemos.cc/product/d1.html
int pwmPins[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //uložení hodnoty pwm, když je (-1), tak není pwm nastaveno
String namePins[] = { "", "", "", "", "", "Motor", "ultrasonic_Echo", "ultrasonic_Trigger", ""};  //popis
String locationPins[] = { "", "", "", "", "", "", "", "", ""};        //popis umístění
bool digitalWritePins[] = { false, false, false, false, false, true, false, false, false};   //piny 5 jsou vystupní
bool relayOnHighPins[] = { true, true, true, true, true, true, true, true, true}; //čím se sepne relé true = HIGH

bool inusePin[] = { false, false, false, false, true, false, true, true, false};       //jeli pin používán
int clickPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-click se přepne uvedený PIN. -1 je nenastaveno
int click2Pin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-double click se přepne uvedený PIN. -1 je nenastaveno
int clickHoldStartPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-Začne držet ... se přepne uvedený PIN. -1 je nenastaveno
int clickHoldDoPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-Drží se ....se přepne uvedený PIN. -1 je nenastaveno
int clickHoldEndPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-Pustí držení .... se přepne uvedený PIN. -1 je nenastaveno


//int pocet = 0; //ULTARZVUK pro výpočet počtu průchodů - počet zboží na projíždějícím páse
//musí být napajeno +5V, 3.3 nestačí

//rele připojeno na PIN 5
#define TRIGGER_PIN  digitalPins[7]  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     digitalPins[6]  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 350 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.


/*wifi configuration
 * Wifi credential are stored in local config file, the same local folder. See example file called "wifi_config_example"
 * Filename: wifi_config.h
 * And inside of the file are those lines
 * #define ssid_config "your-wifi-ssid-name" 
 * #define pws_config "your-wifi-ssid-password")
*/
#include "wifi_config.h"   
const char* ssid = ssid_config; 
const char* password = pws_config;

String deviceSwVersion = "2017-10-1";
String deviceBoard = "RobotDyn Wifi D1R2";
char* deviceId = "esp8266garage";
String deviceName = "GarageController";
String deviceLocation = "Garage";

  //server IP address - where he is listening
  //nastavení serveru, na který budu poslouchat a posílat POST
  //const char* targetServer = "192.168.0.40"; //IP adresa lokálního serveru - naučit mDNS
  const char* targetServer = "192.168.0.18"; //macBook - 192.168.0.40=RPi.nanoWifi
  
  const int httpPort = 9090;

//Garage Door positions 
  const int CurrentDoorStateOpen = 0;
  const int CurrentDoorStateClosed = 1;
  const int CurrentDoorStateOpening = 2;
  const int CurrentDoorStateClosing = 3;
  const int CurrentDoorStateStopped = 4;
  const int TargetDoorStateOpen = 0;
  const int TargetDoorStateClosed = 1;
  const int DoorMaxCm = 70;    //real position in cm, when the door are open 
  const int DoorMinCm = 20;      //real position in cm, when the door are closed 
  
  int CurrentDoorState = CurrentDoorStateOpen;
  int TargetDoorState = TargetDoorStateOpen;
  int TargetDoorOpen = 100;   //how many percents you want to open the door? Target positoin.
  int doorPositionLast = 100; //Last door position 
  int doorLenght = DoorMaxCm - DoorMinCm; //Size of the door 

  bool ObstructionDetected = false; //ne nějaká překážka ve vratech? Pokud ano, tak true
  bool doorIsMoving = false;        //jsou li vrata v pohybu , tak true
  bool changeDoorState = false;     //požadavek uživatele na změnu pozice dveří

  int realDoorPosition = -1;     //skutečná vzdálenost naměřená v cm
  int doorPosition = -1 ;         //přepočítaná vzdálenost na procenta

  int NewTargetDoorState = TargetDoorState;    //nová pozice zadaná uživatelem
  int NewTargetDoorOpen = TargetDoorOpen;     //nová pozice zadaná uživatelem
  

// ************* inicializace serveru na portu :80
ESP8266WebServer server(80);


// ************* inicializace měření vzdálenosti
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.


void myGaradeDoor() {     //manažer pro pohyb dveří
  
  //zjistí aktuální stav dveří
  realDoorPosition = sonar.ping_cm();     //skutečná vzdálenost naměřená v cm
  doorPosition = (realDoorPosition - DoorMinCm) *100 / doorLenght ;         //přepočítaná vzdálenost na procenta

//je nový požadavek na změnu dveří? 
  if (changeDoorState == true) { 
    if (NewTargetDoorState == CurrentDoorState) {  //pokud jsem už v požadované pozici - tak nic nedelěj
       TargetDoorState = NewTargetDoorState;
       changeDoorState = false;
     } else {                                      //pokud NEjsem v požadované pozici - start motor
       TargetDoorState = NewTargetDoorState;
       Serial.println("pustit MOTOR - ONNNNN");
       PressRelayButton(5);
       changeDoorState = false;
       doorIsMoving = true;
     }
  } //konec požadavku na změnu dveří
    
    //  if (NewTargetDoorState != TargetDoorState) {
//     Serial.println("pustit MOTOR - ONNNNN");
//     doorIsMoving = true;
//     TargetDoorState = NewTargetDoorState;
//  }
  


if (doorPosition <= 0) {                        //je li zavřena garáž
    CurrentDoorState = CurrentDoorStateClosed;
    ObstructionDetected = false;
    doorPositionLast = doorPosition;
    Serial.println("zavreno 0%");
    if ( TargetDoorState == TargetDoorStateClosed ) {
        doorIsMoving = false;
      }
    } else {
    if (doorPosition >= 100) {                      //je li otevřeno 100 %
      CurrentDoorState = CurrentDoorStateOpen;
      ObstructionDetected = false;
      doorPositionLast = doorPosition;
      Serial.println("otevreno 100%");
      if ( TargetDoorState == TargetDoorStateOpen ) {
          doorIsMoving = false;
        }
      } else
      if (doorPosition > doorPositionLast) {          //otvírá se garáž
        CurrentDoorState = CurrentDoorStateOpening;
        ObstructionDetected = false;
        doorPositionLast = doorPosition;
        Serial.println("....otvira");
        } else
        if (doorPosition < doorPositionLast) {          //zavira se garaz
          CurrentDoorState = CurrentDoorStateClosing;
          ObstructionDetected = false;
          doorPositionLast = doorPosition;
          Serial.println("....zavira");
          } else
          if (doorPosition == TargetDoorOpen) {        //cílova pozice dosažena
            CurrentDoorState = CurrentDoorStateStopped;
            ObstructionDetected = false;
            doorPositionLast = doorPosition;      //když dorazí do požadované úrovně, tak se vypne motor
            if (doorIsMoving == true) {
              doorIsMoving = false;
              Serial.println("zastavit MOTOR");   //když nahoru, tak 1xSTOP. Když dolů, 2xSTOP
              
              }
            Serial.println("zastaveno na pozadovane pozici");
            } else
            if (doorPosition == doorPositionLast) {        //problem - dveře se zastavily na překážce
              CurrentDoorState = CurrentDoorStateStopped;
              ObstructionDetected = true;
              Serial.println("prekazka ve vratech");
              }
    }
  //konec dlouheho IF->ELSE


  
}  


void handleGetDoor() {    //informace o DOOR

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

void handleSetDoor() {     //nastaví digitální PIN na 0/1 nebo pozici (%) otevření door

  String message = "";
  //==JSON cteni obsahu ==
  StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.parseObject(server.arg(0));   //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  if (!root.success())  {           //když je špatně JSON
    server.send(400, "text/plain", "Nepodarilo se precist JSON");
    return;
  }
  // Retrieve the values
  NewTargetDoorState = root["TargetDoorState"];    //nastaví se nejseli se má otevřít nebo zavřít
  NewTargetDoorOpen = root["TargetDoorOpen"];      //případně se nastaví na kolik procent otevřít
  //==
  
  changeDoorState = true;                         //byl požadavek ke změnu pozice dveří
  
  server.send(200, "text/plain", "Konfigurace nastavena");

//  if (NewTargetDoorState != TargetDoorState) {
//     Serial.println("pustit MOTOR - ONNNNN");
//     doorIsMoving = true;
//     TargetDoorState = NewTargetDoorState;
//  }


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
  StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
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



  server.on("/device", HTTP_GET, handleGetDevice);  //informace o zařízení
  server.on("/device", HTTP_POST, handlePostDevice);  //nastavi o zařízení
  server.on("/device", HTTP_OPTIONS, handleOptionsDevice);  //nastavi o zařízení

  server.on("/pins", handleGetPinList);            //vypise seznam pinů

  //cteni PINu pomoci GET
  server.on("/pins/5", HTTP_GET, handleGetDigiPin);

  //nastaveni PINu pomoci POST
  server.on("/pins/5", HTTP_POST, handleSetDigiPin);

  //osetreni stavu CORS, pomoci OPTIONS
  server.on("/pins/5", HTTP_OPTIONS, handleOptionsDigiPin);

 //cteni konfigurace jednotlivých PINu pomoci GET
  server.on("/pins/0/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/1/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/2/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/3/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/4/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/5/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/6/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/7/config", HTTP_GET, handleGetConfigPin);
  server.on("/pins/8/config", HTTP_GET, handleGetConfigPin);


  //konfigurace jednotlivých PINu pomoci POST
  server.on("/pins/0/config", HTTP_POST, handleConfigPin);
  server.on("/pins/1/config", HTTP_POST, handleConfigPin);
  server.on("/pins/2/config", HTTP_POST, handleConfigPin);
  server.on("/pins/3/config", HTTP_POST, handleConfigPin);
  server.on("/pins/4/config", HTTP_POST, handleConfigPin);
  server.on("/pins/5/config", HTTP_POST, handleConfigPin);
  server.on("/pins/6/config", HTTP_POST, handleConfigPin);
  server.on("/pins/7/config", HTTP_POST, handleConfigPin);
  server.on("/pins/8/config", HTTP_POST, handleConfigPin);

  //osetreni stavu CORS, pomoci OPTIONS
  server.on("/pins/0/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/1/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/2/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/3/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/4/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/5/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/6/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/7/config", HTTP_OPTIONS, handleOptionsPin);
  server.on("/pins/8/config", HTTP_OPTIONS, handleOptionsPin);



  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");



}



void loop() {

  /*
//  pocitani počtu kmitů pře senzorem
  if (sonar.ping_cm() < 50 ) {
    pocet++;
    Serial.print(pocet);
    Serial.println(" pocet ks");
  }
  */

 
 // - zobrazuje vzdálenost do konzoly

  
  delay(500);                     // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
  Serial.print("Ping: ");
  Serial.print(sonar.ping_cm()); // Send ping, get distance in cm and print result (0 = outside set distance range)
  Serial.print("cm  ");
  Serial.print("   ");
  Serial.print( (sonar.ping_cm() - DoorMinCm)*100 / doorLenght ); // Send ping, get distance in cm and print result (0 = outside set distance range)
  Serial.print(" %  ");
  Serial.print("   ");
  Serial.print(sonar.ping()); // Send ping, get distance in cm and print result (0 = outside set distance range)
  Serial.print(" TIME  ");
  Serial.print(doorIsMoving);
  Serial.print(" ");



  myGaradeDoor();
  


  //sleduje HTTP požadavky GET a POST
  server.handleClient();

  
}
