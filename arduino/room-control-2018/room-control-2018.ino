// ---------------------------------------------------------------------------
// Room Control by HTTP GET & POST + Apple HOMEKIT
//    DEVICE
//    board: WeMos D1R2
// ---------------------------------------------------------------------------

//celý návod v README.md


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h>    //ne vždy to spolehlivě funguje - tak jsem to zrušl
#include <ArduinoJson.h>    //knihovna na JSON https://bblanchon.github.io/ArduinoJson/
#include "OneButton.h"      //knihovna na HW tlačítko : https://github.com/mathertel/OneButton
#include "DHT.h"            //knihovna na teplotu a vlhkost: http://navody.arduino-shop.cz/navody-k-produktum/teplotni-senzor-dht11.html | https://playground.arduino.cc/Main/DHTLib


//update 10.3.2018
//DALLAS One Wire
//martin přidán DALLAS teplota
//martin: nové označní pinů D1 místo párování na konkretni GPIO

//napevno dané vstupy a výstupy (dříve konfigurovatelné)
//ESP - modrý & mini
//D0 - nelze použít teplotu DALLAS /1wire
//D0: NEJEDE - nefunguje - hlásí teplotu 127
//D1 - musí se vyzkoušet !!!!!
//D8: musí být vypnuto při kompilaci a nahrávání a spouštění - tedy i při výpadku elektřiny a opětovném spouštění programu



// ************* ARDUINO BORAD configuration *****

String deviceSwVersion = "2018-03-23"; //brezen
String deviceBoard = "RobotDyn Wifi D1R2";
//char deviceId[] = "esp8266room";
String deviceName = "RoomMan";
String deviceLocation = "Room";



// ************* LOCAL WIFI configuration *****
/*
   Wifi credential are stored in local config file, the same local folder. See example file called "wifi_config_example"
   Filename: wifi_config.h
   And inside of the file are those lines
   #define ssid_config "your-wifi-ssid-name"
   #define pws_config "your-wifi-ssid-password")
   //end the SERVER IP adress :
   #define target_Server "192.168.0.100"
*/
#include "wifi_config.h"
const char* ssid = ssid_config;
const char* password = pws_config;

//IPAddress arduinoIP (192, 168, 0, 71); //static IP address
//IPAddress gateway (192, 168, 0, 1); // set gateway to match your network
//IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
IPAddress arduinoIP (10, 66, 103, 6); //static IP address
IPAddress gateway (10, 66, 100, 1); // set gateway to match your network
IPAddress subnet(255, 255, 0, 0); // set subnet mask to match your network

//changable via HTTP POST to 192.168.0.44/device {"targetServer":"192.168.110.117","httpPort":9091}
char targetServer[] = target_Server;    //server IP address - where he is listening
int httpPort = target_Server_Port;     //and port to listen to



// ************* ALARM configuration  *****
// Alarm Status:

const int AlarmSensorArmed = 0;       // no Activities detected
const int AlarmSensorOn1st = 1;       // Active for the first time
const int AlarmSensorOn = 2;          // Active - alarm is ON
const int AlarmSensorStoped = 3;      // Alarm is off now - no Activities detected from now
const int AlarmSensorDelay = 4;       // Alarm is off now, but count down the delay (e.g. wait 3 minut when last motion detected
const int AlarmSensorDelayEnd = 5;       // End of Delay after alarm

int alarmStatusValue[] = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // Array for Alarm States of each PIN
long alarmDelay[] = {0, 0, 3000, 0, 0, 0, 0, 0, 0}; //how long will be the delay after alarm is STOPED. (milisekunds) - e,g, The Light turn off after 3s after PIR motion is over.
unsigned long alarmEndTime[] = {0, 0, 0, 0, 0, 0, 0, 0, 0}; //the time when Alarm STOPed. For count-down of alarm delay


int analogPins[] = {0};

//              PIN:{ 0, 1, 2, 3, 4,  5,  6,  7,  8 }    | PIN cislo [5] = LED modrá na boardu
int digitalPins[] = {16, 5, 4, 0, 2, 14, 12, 13, 15};   //mapování PINů pro RobotDyn Wifi D1R2: https://www.wemos.cc/product/d1.html
int pwmPins[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //uložení hodnoty pwm, když je (-1), tak není pwm nastaveno
String namePins[] = { "NaN", "NaN", "PIR", "Tlacitko-1", "Tlacitko-1", "svetlo-1", "svetlo-2", "Teplota DS", "NaN"};  //popis
String locationPins[] = { "", "", "", "", "", "", "", "", ""};        //popis umístění
bool digitalWritePins[] = { false, false, false, false, false, true, true, true, false};   //piny 5, 6, 7, (8) jsou vystupní
bool relayOnHighPins[] = { true, true, true, true, true, false, true, true, true}; //čím se sepne relé true = HIGH
bool inusePin[] = { true, false, true, true, true, true, true, false, true};       //jeli pin používán
int clickPin[] = { -1, -1, -1, 5, 6, -1, -1, -1, -1};  //tlacitko-click se přepne uvedený PIN. -1 je nenastaveno
int click2Pin[] = { -1, -1, -1, 6, 6, -1, -1, -1, -1};  //tlacitko-double click se přepne uvedený PIN. -1 je nenastaveno
int clickHoldStartPin[] = { -1, -1, 5, 5, 6, -1, -1, -1, -1};  //tlacitko-Začne držet ... se přepne uvedený PIN. -1 je nenastaveno
//nastaveno, že vypne PIN 5+6 ... viz funkce longPressStart1()
int clickHoldDoPin[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1};  //tlacitko-Drží se ....se přepne uvedený PIN. -1 je nenastaveno
int clickHoldEndPin[] = { -1, -1, 5, -1, -1, -1, -1, -1, -1};  //tlacitko-Pustí držení .... se přepne uvedený PIN. -1 je nenastaveno


// ************* TEPLOTA OneWire DS configuration  *****
//TEPLOTA

#include <OneWire.h>
#include <DallasTemperature.h>
long temp1 = -9899;   //globální teplota pro OneWire DS teploměr
unsigned long lastTempRead = 0;
// nastaveni vstupniho cisla Teplota

const int pinCidlaDS = D7;

// instance oneWireDS z knihovny OneWire
OneWire oneWireDS(pinCidlaDS);
// instance senzoryDS z knihovny DallasTemperature
DallasTemperature senzoryDS(&oneWireDS);

//TEPLOTA - END






// *************inicializace serveru na portu :80
ESP8266WebServer server(80);


// *************inicializace čidla teploty a vlhkosti
// inicializace DHT senzoru s nastaveným pinem a typem senzoru
//DHT myDHT(digitalPins[0], DHT11);
DHT myDHT(digitalPins[0], DHT11);


//*************inicializace HW tlačítek
OneButton button1(digitalPins[3], true);    // Nastavení nového "talčítka" OneButton na PIN P3, true = tlačítko se spina na LOW
OneButton button2(digitalPins[4], true);    // Nastavení nového "talčítka" OneButton na PIN P4



//end of definitions



void handleGetDStemperature()  { //informace o teplotě z Dallas OneWire

  //nastavení hlavicky
  server.sendHeader("Connection", "keep-alive"); //BODY bude delší, tak se nenastavuje na NULU
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");

  String message = "";
  unsigned long actmill = millis();

  // čte z OneWire v globální promene temp1
  // aktualizuje každcýh 5 s

  if ((millis() - lastTempRead) > 5000) {
    lastTempRead = millis();
    senzoryDS.requestTemperatures();
    temp1 = round(senzoryDS.getTempCByIndex(0) * 100);

    // vĂ˝pis teploty na sĂ©riovou linku, pĹ™i pĹ™ipojenĂ­ vĂ­ce ÄŤidel
    // na jeden pin mĹŻĹľeme postupnÄ› naÄŤĂ­st vĹˇechny teploty
    // pomocĂ­ zmÄ›ny ÄŤĂ­sla v zĂˇvorce (0) - poĹ™adĂ­ dle unikĂˇtnĂ­ adresy ÄŤidel
    Serial.print("Teplota cidla DS18B20: ");
    Serial.print(temp1);
    Serial.println(" stupnu Celsia");
  }

  int temperature = temp1;
  //int humidity = myDHT.readHumidity();


    //==JSON vytvoreni obsahu ==
    StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
    JsonObject& root = jsonBuffer.createObject();
    root["deviceTemperature"] = temperature;
    root["actmillis"] = actmill;     //od 0mena, pro 0mena
    root["TempTime"] = lastTempRead;     //od 0mena, pro 0mena

    //==JSON generuje vystup ==
    int len = root.measureLength() + 1; //delka cele zpravy
    char rootstr[len];
    root.printTo(rootstr, len);         //vygeneruje JSON
    message += rootstr;

    server.send(200, "application/json", message);

}

void handleGetTemperature() {    //informace o teplotě a vlhkosti

  //nastavení hlavicky
  server.sendHeader("Connection", "keep-alive"); //BODY bude delší, tak se nenastavuje na NULU
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");

  String message = "";

  // pomocí funkcí readTemperature a readHumidity načteme
  // do proměnných tep a vlh informace o teplotě a vlhkosti,
  // čtení trvá cca 250 ms
  int temperature = myDHT.readTemperature();
  int humidity = myDHT.readHumidity();

  // kontrola, je li v pořádku teplota //https://playground.arduino.cc/Main/DHTLib    //musímít napájení +5v ne 3.3
  int chk = myDHT.read();
  if (chk != 1) {
    // při chybném čtení vypiš hlášku
    message = ("Chyba při čtení z DHT senzoru!");
    server.send(404, "application/javascript", message);
    return;

  } else {

    //==JSON vytvoreni obsahu ==
    StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
    JsonObject& root = jsonBuffer.createObject();
    root["deviceTemperature"] = temperature;
    root["deviceHumidity"] = humidity;

    //==JSON generuje vystup ==
    int len = root.measureLength() + 1; //delka cele zpravy
    char rootstr[len];
    root.printTo(rootstr, len);         //vygeneruje JSON
    message += rootstr;

    //==ktedy formát?
    //server.send(200, "text/plain", message);
    server.send(200, "application/json", message);
    //server.send(200, "text/json", message);
    //server.send(200, "application/javascript", message);
  }
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
  //  String mDNS = deviceId;

  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<300> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.createObject();

  root["deviceName"] = deviceName;
  root["deviceLocation"] = deviceLocation;

  //jen pro cteni:
  root["deviceSwVersion"] = deviceSwVersion;
  root["deviceBoard"] = deviceBoard;
  root["deviceIP"] = WiFi.localIP().toString();
  //  root["devicemDNS"] = mDNS + ".local" ;
  //  root["deviceId"] = deviceId;
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
  server.send(200, "application/json", message);
  //server.send(200, "text/json", message);
  //server.send(200, "application/javascript", message);

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

  String servername = root["targetServer"] ;
  servername.toCharArray (targetServer, servername.length() + 1);
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

void handleGetAnalog() {       //zjistí stav analogového vstupupu /analog  ESP má jen jeden analogovy vstup A0

  String message = "";

  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<200> jsonBuffer;                           //Reserve memory space
  int value  = 0;
  value = analogRead(analogPins[0]);
  JsonObject& root = jsonBuffer.createObject();
  root["value"] = value;
  //==

  //==JSON generuje vystup ==
  int len = root.measureLength() + 1; //delka cele zpravy
  char rootstr[len];
  root.printTo(rootstr, len);         //vygeneruje JSON
  message += rootstr;
  //==

  server.send(200, "text/plain", message);
} //konec fce


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


void handleGetPir() {     //get information about Alarm Sensors ... from URL

  String message = "";

  //==cislo PINu z URL
  String urlUri = server.uri();
  char uriArray[] = "/pirs/x";       //přečte pořadí pirs - PINU: x
  int pinNumber;                    //prevede na cislo
  urlUri.toCharArray(uriArray, 10);       //nakupíruje text do pole: Copies the string's characters to the supplied buffer. (9 je malo)
  pinNumber = atoi(&uriArray[6]);        //z pole zjistí cislo - převede znak na číslo (x) - jen jednociferne
  //==

  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<200> jsonBuffer;                           //Reserve memory space

  JsonObject& root = jsonBuffer.createObject();

  root["interval"] = alarmDelay[pinNumber];          //jako dlouho (milisekundy) po aktivaci PIR bude trvat alarm. 0 = no Delay
  root["starttime"] = alarmEndTime[pinNumber];   //kdy byl naposledy zmeren cas, aby se dopocitala delka sepnuti PIR
  root["currenttime"] = millis();                     //vrátí současný stav počítadla času
  root["motion"] = alarmStatusValue[pinNumber];      // Return the Alarm Status Codes ...see comments

  //==

  //==JSON generuje vystup ==
  int len = root.measureLength() + 1; //delka cele zpravy
  char rootstr[len];
  root.printTo(rootstr, len);         //vygeneruje JSON
  message += rootstr;
  //==

  server.send(200, "text/plain", message);

  /*    Alarm Status Codes
    const int AlarmSensorArmed = 0;       // no Activities detected
    const int AlarmSensorOn1st = 1;       // Active for the first time
    const int AlarmSensorOn = 2;          // Active - alarm is ON
    const int AlarmSensorStoped = 3;      // Alarm is off now - no Activities detected from now
    const int AlarmSensorDelay = 4;       // Alarm is off now, but count down the delay (e.g. wait 3 minut when last motion detected
    const int AlarmSensorDelayEnd = 5;       // End of Delay after alarm

  */


} //konec fce

void handleSetPir() {     //set parameters for Alarm

  String message = "";

  //==cislo PINu z URL
  String urlUri = server.uri();
  char uriArray[] = "/pirs/x";       //přečte hodnotu PINU: x
  int pinNumber;                    //prevede na cislo
  urlUri.toCharArray(uriArray, 10);       //nakupíruje text do pole: Copies the string's characters to the supplied buffer. (8 je malo)
  pinNumber = atoi(&uriArray[6]);        //z pole zjistí cislo - převede znak na číslo (x) - jen jednociferne
  //==


  //==JSON cteni obsahu ==
  StaticJsonBuffer<200> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.parseObject(server.arg(0));   //Deserialize the JSON string, count here: https://bblanchon.github.io/ArduinoJson/assistant/
  if (!root.success())  {           //když je špatně JSON
    server.send(400, "text/plain", "Nepodarilo se precist JSON PIR");

    return;
  }
  // Retrieve the values
  String valueS  = root["interval"];                  //how long will be the delay after alarm is STOPED. (milisekunds) - e,g, The Light turn off after 3s after PIR motion is over. | 3000 = 3sec
  int value = valueS.toInt();
  alarmDelay[pinNumber] = value;
  //==


  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce

void handleOptionsPir() {   //korekce pro CORS
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



void handleRoot() {         //help v rootu IP adresy
  String message = "";

  message += "<h1>homeDevice </h1>";
  message += "<p>Verze: " + String(deviceSwVersion) + "</p>";
  message += "<p>IP adresa: http://" +  WiFi.localIP().toString() + "</p>";
  //  message += "<p>mDNS adresa: http://" +  String(deviceId) + ".local</p>";

  message += "<h2>Současná zapojení</h2>";
  message += "<p>PIN 0 = teplota a vlhkost</p>";
  message += "<p>PIN 1 = PIR venkovni //neni nastaceno</p>";
  message += "<p>PIN 2 = PIR vnitřní: zapne / vypne svetlo</p>";
  message += "<p>PIN 3 = CLICK: vypínač bouda / CLICK2: vypínač okolí / HOLD: vypne svetlo, v boudoucnosti vše</p>";
  message += "<p>PIN 4 = CLICK: vypínač okolí   //integrovana ale pridat PIN po spusteni</p>";
  message += "<p>PIN 5 = relé světlo bouda</p>";
  message += "<p>PIN 6 = relé světlo venku okolí</p>";
  message += "<p>PIN 7 = PWM - přes udělátko na boardu ledky ...... //někdy bude : vrata //neni</p>";
  message += "<p>PIN 8 = branka // neni</p>";
  message += "<p>PIN A0 - světlo solar</p>";


  message += "<h2>Popis REST API</h2>";
  message += "<p>Zobrazeni stavu konkretniho pinu, napr cislo 4: ";
  message += "<a href=\"http://\\" + WiFi.localIP().toString() + "/pins/4 \">http://" +  WiFi.localIP().toString() + "/pins/4</a></p>";
  message += "<p>Nastaveni stavu konkretniho pinu pomoci stejne cesty jen s POST + JSON (value, pwm)</p>";

  message += "<br><p>Konfigurace konkretniho pinu, napr cislo 4: ";
  message += "<a href=\"http://\\" + WiFi.localIP().toString() + "/pins/4/config \">http://" +  WiFi.localIP().toString() + "/pins/4/config</a></p>";
  message += "<p>Nastaveni stavu konkretniho pinu pomoci stejne cesty jen s POST + JSON (name, location, DigitalWrite, RelayOnHigh)</p>";

  message += "<br><p>Nastavení celeho zarizeni:";
  message += "<a href=\"http://\\" + WiFi.localIP().toString() + "/device \"> http://" +  WiFi.localIP().toString() + "/device</a></p>";
  message += "<p>Nastaveni stavu konkretniho pinu pomoci stejne cesty jen s POST + JSON (deviceName, deviceLocation)</p>";

  message += "<br><p>Zobrazeni stavu analogoveho pinu: (je jen jedno A0)";
  message += "<a href=\"http://\\" + WiFi.localIP().toString() + "/analog \"> http://" +  WiFi.localIP().toString() + "/analog</a></p>";

  message += "<br><p>PIR: (je jen pinu 2)";
  message += "<a href=\"http://\\" + WiFi.localIP().toString() + "/pir/2 \"> http://" +  WiFi.localIP().toString() + "/analog</a></p>";
  message += "<p>Je možné menit pomoci POST jen /interval/";

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



void sendPostToServer(int pinNumber, int pinValue, int pinPwm) { //poslani POST na server

  //nastavení serveru, na který budu poslouchat a posílat POST
  String postMessage;
  String url = "/pins/" + String(pinNumber);


  //pripojeni na server
  Serial.print("connecting to ");
  Serial.println(targetServer);
  // Use WiFiClient class to create TCP connections
  WiFiClient client;



  if (!client.connect(targetServer, httpPort)) {
    Serial.println("connection failed");
    return;
  }


  //==JSON vytvoreni obsahu ==
  StaticJsonBuffer<200> jsonBuffer;                           //Reserve memory space
  JsonObject& root = jsonBuffer.createObject();
  root["value"] = pinValue;
  root["pwm"] = pinPwm;
  //==


  //==JSON generuje vystup ==
  int len = root.measureLength() + 1; //delka cele zpravy
  char rootstr[len];
  root.printTo(rootstr, len);         //vygeneruje JSON
  postMessage = rootstr;
  //==


  // We now create a URI for the request
  Serial.print("Requesting URL: ");
  Serial.println(url);

  /*
    // This will send the GET request to the server
    //client.print(String("GET ") + url + " HTTP/1.1\r\n" +
    //              "targetServer: " + targetServer + "\r\n" +
    //              "Connection: close\r\n\r\n");

  */
  // a toto pošle POST
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + targetServer + "\r\n" +
               "Content-Type: application/json\r\n" +    //like application/x-www-form-urlencoded, multipart/form-data or application/json)
               "Connection: keep-alive\r\n" +               //Connection: keep-alive: https://blog.insightdatascience.com/learning-about-the-http-connection-keep-alive-header-7ebe0efa209d
               "Content-Length: " + postMessage.length() + "\r\n\r\n" +
               postMessage + "\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println();
  Serial.println("closing connection");

} //konec fce




// ****************  funkce pri stisknuti tlacitka 1

void click1() {             // P3
  // This function will be called when the button1 was pressed 1 time (and no 2. button press followed).
  Serial.println("Button 1 click.");
  switchChange (clickPin[3]);         //změní aktuální stav PINU 1->0 nebo 0->1
} // click1

void doubleclick1() {       // P4
  // This function will be called when the button1 was pressed 2 times in a short timeframe.
  Serial.println("Button 1 doubleclick.");
  switchChange (click2Pin[4]);
} // doubleclick1

void longPressStart1() {    //off: 3+4
  // This function will be called once, when the button1 is pressed for a long time.
  Serial.println("Button 1 longPress start");
  switchOff(clickHoldStartPin[3]);
  switchOff(clickHoldStartPin[4]);
} // longPressStart1

void longPress1() {       //nic
  // This function will be called often, while the button1 is pressed for a long time.
  Serial.println("Button 1 longPress...");
} // longPress1

void longPressStop1() {  // nic
  // This function will be called once, when the button1 is released after beeing pressed for a long time.

  Serial.println("Button 1 longPress stop");
} // longPressStop1

// funkce pri stisknuti tlacitka 2
void click2() {              // P4
  Serial.println("Button 2 click.");
  switchChange (clickPin[4]);
} // click2

void doubleclick2() {        // nic
  Serial.println("Button 2 doubleclick.");
} // doubleclick2

void longPressStart2() {     // off: 3+4
  Serial.println("Button 2 longPress start");
  switchOff(clickHoldStartPin[3]);
  switchOff(clickHoldStartPin[4]);

} // longPressStart2

void longPress2() {         // nic
  Serial.println("Button 2 longPress...");
} // longPress2

void longPressStop2() {     // nic
  Serial.println("Button 2 longPress stop");
} // longPressStop2


// Konec tlačítek



// ********** ALARMs

void alarmStatus (int pinNumber) { //Alarm: update global array of statute /Alarm Sensor

  /*    Alarm Sensor Status:
        update value for alarmStatusValue[pinNumber]

     const int AlarmSensorArmed = 0;       // no Activities detected
     const int AlarmSensorOn1st = 1;       // Active for the first time
     const int AlarmSensorOn = 2;          // Active - alarm is ON
     const int AlarmSensorStoped = 3;      // Alarm is off now - no Activities detected from now
     const int AlarmSensorDelay = 4;       // Alarm is off now, but count down the delay (e.g. wait 3 minut when last motion detected
     const int AlarmSensorDelayEnd = 5;       // End of Delay after alarm

  */



  //přidat sendPostToServer(při začátku a ukončení alarmu
  //
  //přidat na SERVER - teĎ špatně na port 8




  int alarmValue = digitalRead(digitalPins[pinNumber]);    //info from sensor

  if (alarmValue == 1) {  //Signal from Alalrm Sensor is On
    if (alarmStatusValue[pinNumber] == AlarmSensorArmed || alarmStatusValue[pinNumber] > AlarmSensorOn) {   //active alarm
      alarmStatusValue[pinNumber] = AlarmSensorOn1st; //Active - alarm is ON 1st time (just starting)
    } else {
      if (alarmStatusValue[pinNumber] == AlarmSensorOn1st) {
        alarmStatusValue[pinNumber] = AlarmSensorOn;   //Active - alarm is ON (is running)
      }
    }
  } else {              //Signal from Alalrm Sensor is Off
    if (alarmStatusValue[pinNumber] == AlarmSensorOn1st || alarmStatusValue[pinNumber] == AlarmSensorOn) {
      alarmStatusValue[pinNumber] = AlarmSensorStoped;   //Alarm is turning Off (just Stoped)
    } else {
      if (alarmStatusValue[pinNumber] == AlarmSensorStoped) {
        alarmStatusValue[pinNumber] = AlarmSensorDelay;
        alarmEndTime[pinNumber] = millis(); //update the last time when ALARM is ON
      } else {
        if (alarmStatusValue[pinNumber] == AlarmSensorDelay) {
          unsigned long currentMillis = millis();
          if (currentMillis - alarmEndTime[pinNumber] > alarmDelay[pinNumber] ) {    //Do I need to wait ?
            alarmStatusValue[pinNumber] = AlarmSensorDelayEnd;
          }
        } else {
          if (alarmStatusValue[pinNumber] == AlarmSensorDelayEnd) {
            alarmStatusValue[pinNumber] = AlarmSensorArmed;  //Alarm is Off and is Armed (quiet)
          }
        }
      }
    }
  }
} //end alarmStatus


void alarmInit (int pinNumber) {   //ALARM - inicialisation of PIN used for alarm sensor like PIR motion, Magnetic door...

  digitalWritePins[pinNumber] = false;
  pinMode(digitalPins[pinNumber], INPUT_PULLUP);
  digitalWrite(digitalPins[pinNumber], LOW );

}


// ************* ALARMs END




// ********** přepínače výstupů

void switchChange(int pinNumber) {                  //změní aktuální stav PINU 1->0 nebo 0->1
  int value  = 0;
  value = digitalRead(digitalPins[pinNumber]);
  if (value == 1) {
    digitalWrite(digitalPins[pinNumber], 0);
    sendPostToServer(pinNumber, 0, 0); //poslani na POST server
  } else {
    digitalWrite(digitalPins[pinNumber], 1);
    sendPostToServer(pinNumber, 1, 0); //poslani na POST server
  }

}

void switchOff(int pinNumber) {                  //PIN nastaví na vypnuto (zohlední i relayOnHighPins )
  if (relayOnHighPins[pinNumber]) {
    digitalWrite(digitalPins[pinNumber], 0);
  } else {
    digitalWrite(digitalPins[pinNumber], 1);
  }
  sendPostToServer(pinNumber, 0, 0); //poslani na POST server
}

void switchOn(int pinNumber) {                  //PIN nastaví na zapnuto (zohlední i relayOnHighPins )
  if (relayOnHighPins[pinNumber]) {
    digitalWrite(digitalPins[pinNumber], 1);
  } else {
    digitalWrite(digitalPins[pinNumber], 0);
  }
  sendPostToServer(pinNumber, 1, 0); //poslani na POST server
}

//konec přepínače výstupů




// ****************  SETUP - nastaveni vseho potřebného
void setup(void) {


  //nastavení pevni IPadresy: https://www.arduino.cc/en/Reference/WiFiConfig
  //Serial.begin(921600);
  Serial.begin(9600);

  //ještě před teplotou
  resetPins();      //nastaví digitální vystupní piny na vystupni a vypne je

  // zapnutí komunikace s teploměrem DHT
  myDHT.begin();





  // ******* nastavení PINů a Tlačítek



  // nalinkovani funkcni pro tlacitko 1
  button1.attachClick(click1);
  button1.attachDoubleClick(doubleclick1);
  button1.attachLongPressStart(longPressStart1);
  button1.attachLongPressStop(longPressStop1);
  button1.attachDuringLongPress(longPress1);

  // nalinkovani funkcni pro tlacitko 2
  button2.attachClick(click2);
  button2.attachDoubleClick(doubleclick2);
  button2.attachLongPressStart(longPressStart2);
  button2.attachLongPressStop(longPressStop2);
  button2.attachDuringLongPress(longPress2);


  //init of Alarm's PIN
  alarmInit(2);

  // ******* nastavení a připojení WIFI
  //Unlike WiFi.begin() which automatically configures the WiFi shield to use DHCP, WiFi.config() allows you to manually set the network address of the shield.
  //https://www.arduino.cc/en/Reference/WiFiConfig
  WiFi.config(arduinoIP, gateway, subnet);

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

  //  if (MDNS.begin(deviceId)) {       //mDNS je podporovano! Na deviceId.local je k nalezeni tento DEVICE
  //    Serial.println("MDNS responder started");
  //  }


  // ******* nastavení poslouchání na GET/POST

  //server.on("/", handleRoot);         //zde je i zakladni help a popis
  //toto bylo spravne
  // pro demo ucely vraci obdobne jako SDS - nez bude mit omen update
  //handleRoot je zde
  server.on("/help", handleRoot);         //zde je i zakladni help a popis

//  server.on("/", handleGetDStemperature);         //teplota DS //dočasne nez omen navrhne jinad
  server.on("/", handleGetDoomaster);         //teplota DS //dočasne nez omen navrhne jinad


  server.on("/device", HTTP_GET, handleGetDevice);  //informace o zařízení
  server.on("/device", HTTP_POST, handlePostDevice);  //nastavi o zařízení
  server.on("/device", HTTP_OPTIONS, handleOptionsDevice);  //nastavi o zařízení

  server.on("/analog", handleGetAnalog);            //precte analogovy vstup

  server.on("/pins", handleGetPinList);            //vypise seznam pinů

  server.on("/1", handleZapni1);   //testování s Omenem Doomaster - zapne svetlo P5
  server.on("/0", handleVypni1);   //testování s Omenem Doomaster - vypne svetlo P5
  server.on("/3", handleZapni2);   //testování s Omenem Doomaster - zapne svetlo P5
  server.on("/2", handleVypni2);   //testování s Omenem Doomaster - vypne svetlo P5

  //cteni PINu pomoci GET
  server.on("/pins/0", HTTP_GET, handleGetTemperature);
  server.on("/pins/1", HTTP_GET, handleGetDigiPin);
  server.on("/pins/2", HTTP_GET, handleGetDigiPin);
  server.on("/pins/3", HTTP_GET, handleGetDigiPin);
  server.on("/pins/4", HTTP_GET, handleGetDigiPin);
  server.on("/pins/5", HTTP_GET, handleGetDigiPin);
  server.on("/pins/6", HTTP_GET, handleGetDigiPin);
  server.on("/pins/7", HTTP_GET, handleGetDStemperature);
  server.on("/pins/8", HTTP_GET, handleGetDigiPin);

  //nastaveni PINu pomoci POST
  //server.on("/pins/0", HTTP_POST, handleSetDigiPin); - není možné, je to teplotni číslo
  server.on("/pins/1", HTTP_POST, handleSetDigiPin);
  server.on("/pins/2", HTTP_POST, handleSetDigiPin);
  server.on("/pins/3", HTTP_POST, handleSetDigiPin);
  server.on("/pins/4", HTTP_POST, handleSetDigiPin);
  server.on("/pins/5", HTTP_POST, handleSetDigiPin);
  server.on("/pins/6", HTTP_POST, handleSetDigiPin);
  //server.on("/pins/7", HTTP_POST, handleSetDigiPin);- není možné, je to teplotni číslo DS OneWire
  server.on("/pins/8", HTTP_POST, handleSetDigiPin);

  //osetreni stavu CORS, pomoci OPTIONS
  //server.on("/pins/0", HTTP_OPTIONS, handleOptionsDigiPin);  - není možné, je to teplotni číslo
  server.on("/pins/1", HTTP_OPTIONS, handleOptionsDigiPin);
  server.on("/pins/2", HTTP_OPTIONS, handleOptionsDigiPin);
  server.on("/pins/3", HTTP_OPTIONS, handleOptionsDigiPin);
  server.on("/pins/4", HTTP_OPTIONS, handleOptionsDigiPin);
  server.on("/pins/5", HTTP_OPTIONS, handleOptionsDigiPin);
  server.on("/pins/6", HTTP_OPTIONS, handleOptionsDigiPin);
  //server.on("/pins/7", HTTP_OPTIONS, handleOptionsDigiPin);- není možné, je to teplotni číslo DS OneWire
  server.on("/pins/8", HTTP_OPTIONS, handleOptionsDigiPin);

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


  //pro PIR cidlo

  //cteni PIR PINu pomoci GET - jiné url
  server.on("/pirs/1", HTTP_GET, handleGetPir);
  server.on("/pirs/2", HTTP_GET, handleGetPir);

  //nastaveni PINu pomoci POST
  server.on("/pirs/1", HTTP_POST, handleSetPir);
  server.on("/pirs/2", HTTP_POST, handleSetPir);

  //osetreni stavu CORS, pomoci OPTIONS
  server.on("/pirs/1", HTTP_OPTIONS, handleOptionsPir);
  server.on("/pirs/2", HTTP_OPTIONS, handleOptionsPir);



  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");





//TEPLOTA DS
    pinMode(pinCidlaDS, OUTPUT);  //nutné pro ESP - MEGA asi má
   // zapnuti­ komunikace knihovny s teplotnim cidlem
    senzoryDS.begin();







}


// ****************  HLAVNÍ ČÁST - vykonávání hlavní části

void loop(void) {


  //sleduje HTTP požadavky GET a POST
  server.handleClient();

  // sleduje jestli bylo tlacitko stisknuto
  button1.tick();
  button2.tick();

  //checking for alarm activation - in this case PIR motino
  alarmStatus (2);
  if (alarmStatusValue[2] == AlarmSensorOn1st) {
    Serial.println ("ALARM ON");
    switchOn(5);                 //Switch On the light
    sendPostToServer(2, 1, 0);   //send info to server - Alarm is ON
  }

  //   if (alarmStatusValue[2] == AlarmSensorStoped) {
  //      Serial.println ("ALARM ---KONEC--");
  //      switchOff(5);
  //    }

  if (alarmStatusValue[2] == AlarmSensorDelayEnd) {
    Serial.println ("DELAY KONEC--");
    switchOff(5);                 //Switch OFF the light
    sendPostToServer(2, 0, 0);   //send info to server - Alarm is OFF
  }
}

/*
  const int AlarmSensorArmed = 0;       // no Activities detected
  const int AlarmSensorOn1st = 1;       // Active for the first time
  const int AlarmSensorOn = 2;          // Active - alarm is ON
  const int AlarmSensorStoped = 3;      // Alarm is off now - no Activities detected from now
  const int AlarmSensorDelay = 4;       // Alarm is off now, but count down the delay (e.g. wait 3 minut when last motion detected
  const int AlarmSensorDelayEnd = 5;       // End of Delay after alarm

*/


//======= pro demo s Doomastera ==== 21.3.2018


void handleZapni1() {     //zapne svetlo na PIN 5

  String message = "";

  int pinNumber;
  pinNumber = 5;


  //==vlastní funkcnost zapinani

  //nově

    int value = 1;
    if (relayOnHighPins[pinNumber]) {
      digitalWrite(digitalPins[pinNumber], value);
    } else {
      if (value == 1) {
        digitalWrite(digitalPins[pinNumber], 0);
      } else {
        digitalWrite(digitalPins[pinNumber], 1);
      }
    }
  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce

void handleVypni1() {     //vypne svetlo na PIN 5

  String message = "";

  int pinNumber;
  pinNumber = 5;


  //==vlastní funkcnost zapinani

  //nově



    int value = 0;
    if (relayOnHighPins[pinNumber]) {
      digitalWrite(digitalPins[pinNumber], value);
    } else {
      if (value == 1) {
        digitalWrite(digitalPins[pinNumber], 0);
      } else {
        digitalWrite(digitalPins[pinNumber], 1);
      }
    }
  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce

void handleZapni2() {     //zapne svetlo na PIN 6

  String message = "";

  int pinNumber;
  pinNumber = 6;


  //==vlastní funkcnost zapinani

  //nově

    int value = 1;
    if (relayOnHighPins[pinNumber]) {
      digitalWrite(digitalPins[pinNumber], value);
    } else {
      if (value == 1) {
        digitalWrite(digitalPins[pinNumber], 0);
      } else {
        digitalWrite(digitalPins[pinNumber], 1);
      }
    }
  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce

void handleVypni2() {     //vypne svetlo na PIN 6

  String message = "";

  int pinNumber;
  pinNumber = 6;


  //==vlastní funkcnost zapinani

  //nově



    int value = 0;
    if (relayOnHighPins[pinNumber]) {
      digitalWrite(digitalPins[pinNumber], value);
    } else {
      if (value == 1) {
        digitalWrite(digitalPins[pinNumber], 0);
      } else {
        digitalWrite(digitalPins[pinNumber], 1);
      }
    }
  server.send(200, "text/plain", "Konfigurace nastavena");

} //konec fce



void handleGetDoomaster()  { //informace vypinaci
  //{"deviceTemperature":65409,"svetlo":0,"actmillis":18076,"TempTime":15003}

  //nastavení hlavicky
  server.sendHeader("Connection", "keep-alive"); //BODY bude delší, tak se nenastavuje na NULU
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE");
  server.sendHeader("Access-Control-Allow-Headers", "X-Requested-With,content-type");
  server.sendHeader("Access-Control-Max-Age", "86400");

  String message = "";
  unsigned long actmill = millis();



  // čte z OneWire v globální promene temp1
  // aktualizuje každcýh 5 s

  if ((millis() - lastTempRead) > 5000) {
    lastTempRead = millis();
    senzoryDS.requestTemperatures();
    temp1 = round(senzoryDS.getTempCByIndex(0) * 100);

    // vĂ˝pis teploty na sĂ©riovou linku, pĹ™i pĹ™ipojenĂ­ vĂ­ce ÄŤidel
    // na jeden pin mĹŻĹľeme postupnÄ› naÄŤĂ­st vĹˇechny teploty
    // pomocĂ­ zmÄ›ny ÄŤĂ­sla v zĂˇvorce (0) - poĹ™adĂ­ dle unikĂˇtnĂ­ adresy ÄŤidel
   // Serial.print("Teplota cidla DS18B20: ");
   // Serial.print(temp1);
   // Serial.println(" stupnu Celsia");
  }

  int temperature = temp1;



  //cte hodnotu svetla 1
  int value = 0;
  int pinNumber = 5;

  value = digitalRead(digitalPins[pinNumber]);

  if (relayOnHighPins[pinNumber]) {
    value = value;
  } else {
    if (value == 1) {
      value = 0;
    } else {
      value = 1;
    }
  }

    Serial.print("rele 1 je: ");
    Serial.println(value);

 //cte hodnotu svetla 2
  int value2 = 0;
  int pinNumber2 = 6;

  value2 = digitalRead(digitalPins[pinNumber2]);

  if (relayOnHighPins[pinNumber2]) {
    value2 = value2;
  } else {
    if (value2 == 1) {
      value2 = 0;
    } else {
      value2 = 1;
    }
  }


    Serial.print("rele 2 je: ");
    Serial.println(value2);


  //cte hodnotu Analogového vstupu - intenzita osvetleni

  int value_analog = 0;
  value_analog = analogRead(analogPins[0]);



 //==JSON vytvoreni obsahu ==
    StaticJsonBuffer<250> jsonBuffer;                           //Reserve memory space
    JsonObject& root = jsonBuffer.createObject();
    root["deviceTemperature"] = temperature;

    root["svetlo"] = value;                             // stav svetla / rele 2
    root["svetlo2"] = value2;                           // stav svetla / rele 2
    root["analog"] = value_analog;                      // stav analogového čidla - intenzita světla
    root["motion"] = alarmStatusValue[2];       // stav PIR čidla na PIN číslo 2
    root["actmillis"] = actmill;     //od 0mena, pro 0mena
    root["temptime"] = lastTempRead;     //od 0mena, pro 0mena

    //==JSON generuje vystup ==
    int len = root.measureLength() + 1; //delka cele zpravy
    char rootstr[len];
    root.printTo(rootstr, len);         //vygeneruje JSON
    message += rootstr;

    server.send(200, "application/json", message);

}
