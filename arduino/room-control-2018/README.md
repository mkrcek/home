# ROOMcontrol: Arduino 25.3.2018 22:00


----------

## Co nového ve verzi


- verze 3/2018
- pouze pro DOOMASTER - NE PRO HOMEKIT (ten se musí upravit)

--------

Room Control by HTTP GET & POST + Apple HOMEKIT
DEVICE:
board: WeMos D1R2


## Základní nastavení

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

INFORMACE

      GET http://xx.xx.xx.xx/

vrátí se JSON se všemi údaji

      {
          "deviceTemperature": 0,   //je to 19,22 oC
          "svetlo": 0,
          "svetlo2": 0,
          “analog”: 0,    //stav analogového čidla - intenzita světla 0 - 1023
          “motion”: 0,    //stav PIR čidla 0-5
          "actmillis": 3247,
          "temptime": 0 }
      }


---staré :
-
Super Easy, just to call HTTP GET

     GET http://192.168.100.22/door

and you get message like:

     {"door":0,"CurrentDoorState":1,"TargetDoorState":1,"TargetDoorOpen":100,"ObstructionDetected":false}

sdf
