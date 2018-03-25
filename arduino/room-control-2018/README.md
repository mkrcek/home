# ROOMcontrol: Arduino 25.3.2018 22:00

Room Control by HTTP GET & POST + Apple HOMEKIT
- Arduino Board: **ESP WeMos D1R2**

----------

## Co nového ve verzi


- verze 3/2018
- podpora pro DOOMASTER - NE PRO HOMEKIT (ten se musí upravit)

--------



# HW: Základní nastavení PINů pro ESP

- PIN A0 = analog - snímač osvětlení
- PIN D0 = x
- PIN D1 = x
- PIN D2 = PIR (LOW = zaalarmováno, přerušením se aktivuje alarm)
- PIN D3 = Tlačítko 1
- PIN D4 = Tlačítko 2
- PIN D5 = relé (světlo) 1
- PIN D6 = relé (světlo) 2
- PIN D7 = tepota (DALLAS teplota - po úpravách DHT - teplotu/vlhkost
- PIN D8 = x

PINy D0, D1, D8 jsou nevyužity z důvodu nefunkčnosti připojení na teploměr



## INFORMACE PRO DOOMASTER

      GET http://xx.xx.xx.xx/

vrátí se JSON se všemi údaji

      {
          "deviceTemperature": 0,   // např. 19,22 oC se odešle jako 1922
          "svetlo": 0,
          "svetlo2": 0,
          “analog”: 0,    //stav analogového čidla - intenzita světla 0 - 1023
          “motion”: 0,    //stav PIR čidla 0-5
          "actmillis": 3247,
          "temptime": 0 }
      }

### CHYBY - ERRORS

pokud je něco špatně, Arduino odpoví hláškou a kódem 400.

    “Nepodarilo se precist JSON” nebo
    “Chyba při čtení z DHT senzoru!” ...


### MOTION: Stav PIR čidla

- 0 // no Activities detected
- 1 // Active for the first time
- 2 // Active - alarm is ON
- 3 // Alarm is off now - no Activities detected from now
- 4 // Alarm is off now, but count down the delay (e.g. wait 3 minut when last motion detected
- 5 // End of delay after alarm


-----
## OVLÁDÁNÍ

pomocí GET

        GET http://xx.xx.xx.xx/0      vypne svetlo 1
        GET http://xx.xx.xx.xx/1      zapne svetlo 1
        GET http://xx.xx.xx.xx/2      vypne svetlo 2
        GET http://xx.xx.xx.xx/3      zapne svetlo 2

nebo pomocí POST :-)

        POST http://xx.xx.xx.xx/pins/5 {“value”:0, “pwm”:0}     vypne svetlo 1
        POST http://xx.xx.xx.xx/pins/5 {“value”:1, “pwm”:0}     zapne svetlo 1
        POST http://xx.xx.xx.xx/pins/6 {“value”:0, “pwm”:0}     vypne svetlo 2
        POST http://xx.xx.xx.xx/pins/6 {“value”:1, “pwm”:0}     vypne svetlo 2


-----

## POST z Arduina na server

Arduino při události stisku tlačítka nebo PIR odesílá POST na server

**Stisknut vypínač číslo 1:**


      POST URL je: /pins/5       (kde 5 je vypínač číslo 1)


JSON:


    {“value”:0, “pwm”:0}   


kde value=0 - vypnuto, 1=zapnuto

**Stisknut vypínač číslo 2:**



      POST URL je: /pins/6       (kde 6 je vypínač číslo 2)


JSON:

    {“value”:0, “pwm”:0}


kde value=0 - vypnuto, 1=zapnuto

**Pohyb na PIR:**


      POST URL je: /pirs/2       (jiná URL-!! )


JSON:

      {
            "interval": 3000,
            "starttime": 3516836,
            "currenttime": 3523545,
            "motion": 0
      }


kde "motion" je v intervalu 0-5 (viz MOTION: Stav PIR čidla)

------

# ** PODROBNE NASTAVENI **
==================

## Konfigurace PIR
Připojené k Arduinu jen (JEDNO) 1x PIR čidlo. SW umožňuje  připojit ještě druhé PIR čidlo na D1. Zatím netestováno.

Default konfigurace
- na PIN D2 je připojen LOW= zaalarmovano. Přerušením, tedy sepnutím PIR čidla, se aktivuje alarm.
- sepne se Relé/Světlo 1
- po ukončení posledního pohybu spustí stopky na 3sec (znovu PIN je na LOW)
- teprve potom ukončí Alarm
- kažý stav reportuje pomocí POST na server

PIR senzor se konfiguruje na 2 funkce
1. které PIN/relé se má spínat
2. kdy se má spínat a jaký je stav čidla

je nastaveno na spínaní LOW (tedy nulou) -MM__________

### 1. co spínat

PIR se chová jako tlačítko. (popis Tlačítka samostatně)

- zaregistruje pohyb, sepne RELE / Světlo 1 (PIN5).
- je možné zjistit vzdáleně pomocí GET

        GET xx.xx.xx.xx/pins/2/config

a dostanu JSON

        {
              "name": "PIR",
              "location": "",
              "digitalWrite": false,    // vstupní PIN
              "relayOnHigh": true,      
              "inUse": true,
              "buttonClick": -1,        //
              "buttonDoubleClick": -1,  //
              "buttonHoldStart": 5,     // když je pohyb, zapne PIN5
              "buttonHold": -1,         //
              "buttonHoldEnd": 5        // když je konec pohybu, vypne PIN5
        }

- konfigurace /nastavení/ Tlačítka je možná pomocí POST, na funkci handleConfigPin()

      POST xx.xx.xx.xx/pins/2/config

a odeslaním JSON s požadovanou změnou.

        {
              "name": "PIR",
              "location": "",
              "digitalWrite": false,    // vstupní PIN
              "relayOnHigh": true,      
              "inUse": true,
              "buttonClick": -1,        //
              "buttonDoubleClick": -1,  //
              "buttonHoldStart": 5,     // když je pohyb, zapne PIN5
              "buttonHold": -1,         //
              "buttonHoldEnd": 5        // když je konec pohybu, vypne PIN5
        }

pozn. Před odeslání JSON smazat vše s //


### 2. stav PIR
- konfigurace délky sepnutí v array: alarmDelay
- informace poskytuje funkce handleGetPir()
- je možné zjistit vzdáleně pomocí GET (jiné URL s /pirs/2  !!)

        GET xx.xx.xx.xx/pirs/2

a dostanu JSON

        {
            "interval": 3000,           //doba sepnutí v milisec
            "starttime": 0,             //začátek alarmu
            "currenttime": 2158656,     //současný cas arduina
            "motion": 2                 //stav alarmu - viz MOTION: Stav PIR čidla
        }

- konfigurace /nastavení/ RELÉ je možná pomocí POST, na funkci handleSetPir()    (jiné URL s /pirs/2  !!)


      POST xx.xx.xx.xx/pirs/2


a odeslaním JSON s požadovanou změnou na délku sepnutí

            {
                "interval": 3000           //doba sepnutí v milisec
            }


pozn. Před odeslání JSON smazat vše s //

--------

## Konfigurace Tlačítko

Připojené jsou 2 tlačítka. Pro více tlačítek je nutná změna kodu. Tlačítka jsou nastavena takto:

Tlačítko 1:
- 1 x klik: změní stav na PINu 5 (RELÉ1 / SVĚTLO 1) - Array clickPin
- 2 x klik: změní stav na PINu 6 (RELÉ2 / SVĚTLO 2) - Array click2Pin
- Dlouze se podrží: vypne se PINu 5+6 (RELÉ1+2 / SVĚTLO 1+2) - Array clickHoldStartPin
- je možné zjistit vzdáleně pomocí GET

        GET xx.xx.xx.xx/pins/3/config
        GET xx.xx.xx.xx/pins/4/config

a dostanu JSON

        {
              "name": "Tlacitko-1",
              "location": "",
              "digitalWrite": false,    // vstupní PIN
              "relayOnHigh": true,      
              "inUse": true,
              "buttonClick": 5,         //1x klik přepne na PIN5
              "buttonDoubleClick": 6,   //2x klik přepne na PIN6
              "buttonHoldStart": 5,     //podrží vypne oba 5+6, neměnit - !
              "buttonHold": -1,         //
              "buttonHoldEnd": -1       //
        }

- konfigurace /nastavení/ Tlačítka je možná pomocí POST, na funkci handleConfigPin()

      POST xx.xx.xx.xx/pins/3/config
      POST xx.xx.xx.xx/pins/4/config

a odeslaním JSON s požadovanou změnou.

        {
              "name": "Tlacitko-1",
              "location": "",
              "digitalWrite": false,    // vstupní PIN
              "relayOnHigh": true,      
              "inUse": true,
              "buttonClick": 5,         //1x klik přepne na PIN5
              "buttonDoubleClick": 6,   //2x klik přepne na PIN6
              "buttonHoldStart": 5,     //podrží vypne oba 5+6, neměnit - !
              "buttonHold": -1,         //
              "buttonHoldEnd": -1       //
        }

pozn. Před odeslání JSON smazat vše s //
------

## Konfigurace Relé
Připojené relé je nastaveno na spínaní LOW (tedy nulou)
- konfigurace v array: relayOnHighPins
- informace poskytuje funkce handleGetConfigPin()
- je možné zjistit vzdáleně pomocí GET

        GET xx.xx.xx.xx/pins/5/config
        GET xx.xx.xx.xx/pins/6/config

a dostanu JSON

        {
              "name": "svetlo-1",
              "location": "",
              "digitalWrite": true,     //výstupní PIN
              "relayOnHigh": false,     //spíná na LOW
              "inUse": true,
              "buttonClick": -1,
              "buttonDoubleClick": -1,
              "buttonHoldStart": -1,
              "buttonHold": -1,
              "buttonHoldEnd": -1
        }

- konfigurace /nastavení/ RELÉ je možná pomocí POST, na funkci handleConfigPin()

      POST xx.xx.xx.xx/pins/5/config
      POST xx.xx.xx.xx/pins/6/config

a odeslaním JSON s požadovanou změnou.

        {
              "name": "svetlo-1",
              "location": "",
              "digitalWrite": true,     //výstupní PIN
              "relayOnHigh": false,     //spíná na LOW
              "inUse": true,
              "buttonClick": -1,
              "buttonDoubleClick": -1,
              "buttonHoldStart": -1,
              "buttonHold": -1,
              "buttonHoldEnd": -1
        }


pozn. Před odeslání JSON smazat vše s //

--------
