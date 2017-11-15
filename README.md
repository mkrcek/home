# home
Home automation used in my Home + Garden. 
Server is in Go, running on Raspberry Pi.
Mobile Client: iOS HomeKit is an implementation of the HomeKit Accessory Protocol (HAP) in Go. 
Arduino for end-devices.


#Garage

Usecase: 
- I want to use iPhone to open and close my Garage Door 
- Configure and control the Garage Door over HTTP GET adn POST

What do I need:
- iPhone with HomeKit
- Arduino + Relay + Ultrasonic Sensor
- Server (RPi + GoLang)
- Garage Door :-)

**Garage Door**
I was thinking how to measure the distance the best way. So, this is my result: measure distance on the top-rail.

1) Arduino with Ultrasonic Sensor on the Garage engine. The direction is toward to the Door - so, it is not move.
2) Put/Attache a little "paper board" on the Door Holder on the rail. When Door is opened, Paper board is close to Arduino (miximun distance), when the door is closed, Paper board is on the other side (maximum distance). 

**Arduino**
1) Arduino: WiFi D1 R2 ESP 8266
2) Relay: to switch (press) the Garage Door engine

1) Configuration 
You get the configuration when you call HTTP GET of your Arduino, e.g. 192.168.100.22/door/config
and you receive JSON: 
{"DoorMaxCm":100,"DoorMinCm":10,"setDoorDelay":1000}

where 
- DoorMaxCm: is the maximum distance of your garage door, when it is close
- DoorMinCm: is the minimum lenght of your garage door, when it is close
- setDoorDelay: how often the distance is measured (in miliseconds)

1a) setup the DoorMaxCm
- Close the garage traditional way (max Length from ultrasonic sensor)
- send to Arduino HTTP POST , e.g. 192.168.100.22/door/config with message {"setDoorMax":true, "setDoorMin":false}

1b) setup the DoorMinCm
- Open the garage traditional way (min Length from ultrasonic sensor)
- send to Arduino HTTP POST , e.g. 192.168.100.22/door/config with message {"setDoorMax":false, "setDoorMin":true}

1c) setup the setDoorDelay
- If you think you want to change the speed od "measurement" - change the parameter.
- send to Arduino HTTP POST , e.g. 192.168.100.22/door/config with message {"setDoorMax":false, "setDoorMin":false, "setDoorDelay":1000}

Check the setup / configuration with HTTP GET 192.168.100.22/door/config and you can see more actual parameters e.g. {"DoorMaxCm":258,"DoorMinCm":4,"setDoorDelay":1000}


2) GET Current Door Position
Super Easy, just to call HTTP GET 192.168.100.22/door
and you get message like: 
{"door":0,"CurrentDoorState":1,"TargetDoorState":1,"TargetDoorOpen":100,"ObstructionDetected":false}
where
 - door : is the doorPosition in percentage. 0 is close, 100 is open 100%
 - CurrentDoorState: Actual activity of the door like Open = 0; Closed = 1; Opening = 2; Closing = 3; Stopped = 4;
 - TargetDoorState: Target position of the door like Open = 0; Closed = 1;
 - TargetDoorOpen: Target position in percentage (not implemented yet) like Open 40 %
 - ObstructionDetected: "true" when Door is stoped by any reason between 0-100, "false" - no problem 
  

3) SET Door Position
Super Easy too :-) Send HTTP POST to 192.168.100.22/door
with message : 
{"TargetDoorState":1,"TargetDoorOpen":0}
where
 - TargetDoorState: Target position of the door like Open = 0; Closed = 1;
 - TargetDoorOpen: Target position in percentage (not implemented yet) like Open 40 %
  

**GO-server**

