# ROOMcontrol: Arduino 25.3.2018 22:00


----------

## Co nového ve verzi


- verze 3/2018
- pouze pro DOOMASTER - NE PRO HOMEKIT (ten se musí upravit)

--------

Room Control by HTTP GET & POST + Apple HOMEKIT
- Arduino Board: WeMos D1R2


# Základní nastavení

- PIN A0 = analog - snímač osvětlení
- PIN D0 = x
- PIN D1 = x
- PIN D2 = PIR
- PIN D3 = Tlačítko 1
- PIN D4 = Tlačítko 2
- PIN D5 = relé (světlo) 1
- PIN D6 = relé (světlo) 2
- PIN D7 = tepota (DALLAS teplota - po úpravách DHT - teplotu/vlhkost
- PIN D8 = x

PINy D0, D1, D8 jsou nevyužity z důvodu nefunkčnosti připojení na teploměr


## REST API

## INFORMACE

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

- stisknut vypínač číslo 1


    POST URL je: /pins/5       (kde 5 je vypínač číslo 1)


JSON:


    {“value”:0, “pwm”:0}   


kde value=0 - vypnuto, 1=zapnuto

- stisknut vypínač číslo 2


    POST URL je: /pins/6       (kde 6 je vypínač číslo 2)


JSON:

    {“value”:0, “pwm”:0}


kde value=0 - vypnuto, 1=zapnuto

- pohyb na PIR

    POST URL je: /pins/2       (kde 2 je PIR čidlo)

JSON:
    {“value”:0, “pwm”:0}   

kde value=0-5 (viz MOTION: Stav PIR čidla)
