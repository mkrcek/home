package main

// ---------------------------------------------------------------------------
// Garage Door Opener by HTTP GET & POST + Apple HOMEKIT
//			SERVER
// ---------------------------------------------------------------------------

// to RUN with param = IP address of Arduino device
// e.g. MY-PROGRAM-NAME.GO -ip=http://129.12.1.0
//


import (
	"github.com/brutella/hc"
	"github.com/brutella/hc/log"
	"github.com/brutella/hc/accessory"
	"time"
	"fmt"
	"net"
	"net/http"
	"os"
	"io/ioutil"
	"bytes"
	"encoding/json"
	"flag"
)

var arduinoURL string = "http://192.168.100.201"		//default - you can change it with command-line Argument - see main()

const (			//GaradeDoor params

	PositionStateDecreasing int = 0
	PositionStateIncreasing int = 1
	PositionStateStopped    int = 2

	CurrentDoorStateOpen    int = 0
	CurrentDoorStateClosed  int = 1
	CurrentDoorStateOpening int = 2
	CurrentDoorStateClosing int = 3
	CurrentDoorStateStopped int = 4

	TargetDoorStateOpen   int = 0
	TargetDoorStateClosed int = 1
)

type doorType struct {
	DoorPosition int
	CurrentDoorState int
	TargetDoorState int
	TargetDoorOpen int
	ObstructionDetected bool
}
var myGarageDoorDevices doorType			//actual information frem device

//homekit Declaration
var myGarage *accessory.GarageDoormetr
var garageInfo accessory.Info


func handleRootDevice(w http.ResponseWriter, req *http.Request) {

	w.Header().Set("Content-Type", "application/text") //or ? text/json
	w.Header().Set("Connection", "keep-alive")
	//rw.Header().Set("Access-Control-Allow-Origin", "http://localhost:8080/device")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE")
	w.Header().Set("Access-Control-Allow-Headers", "X-Requested-With,content-type");
	w.Header().Set("Access-Control-Max-Age", "86400")

	log.Info.Println("This is my text in ROOT")
	w.Write([]byte("This is my text in ROOT"))
}

func deviceDoorGet()  {		//Get information about Garage Door to global variable

	type doorParam struct {
		DoorPosition int `json:"door"`
		CurrentDoorState int  `json:"CurrentDoorState"`
		TargetDoorState int  `json:"TargetDoorState"`
		TargetDoorOpen int  `json:"TargetDoorOpen"`
		ObstructionDetected bool  `json:"ObstructionDetected"`
	}

// ** get information from HTTP GET
	tr := &http.Transport{
		MaxIdleConns: 10,
		DialContext: (&net.Dialer{
			Timeout:   13 * time.Second, //time for timeout, standdard is 30 ---- 13 ..30
			KeepAlive: 30 * time.Second,
			DualStack: true,
		}).DialContext,
		IdleConnTimeout:    30 * time.Second,
		DisableCompression: true,
	}

	client := &http.Client{Transport: tr}

	tepmUrl := arduinoURL + "/door"
	response, err := client.Get(tepmUrl) //NOTE: it must be full name with http:// ...otherwise willbe an error: unsupported protocol scheme

	//	response, err := http.Get("http://192.168.0.39/") 	//it works with it, but no timeout is set up !! It could be an hour
	if err != nil { //if URL doe's not exist, eg. Arduino is off, error is : Get http://esp8266m.local/: dial tcp: lookup issssdnes.cz: no such host
		fmt.Printf("----error chyba--\n")
		fmt.Printf("%s\n", err)
		//os.Exit(1)
	} else {
		defer response.Body.Close()
		contents, err := ioutil.ReadAll(response.Body)
		if err != nil {
			fmt.Printf("%s", err)
			os.Exit(1)
		}

		var deviceDoor doorParam
		dataFromDevice := []byte(contents)
		json.Unmarshal(dataFromDevice, &deviceDoor)
// END ** get information from HTTP GET

		//share information fro device with global var :myGarageDoorDevices
		myGarageDoorDevices.TargetDoorState = deviceDoor.TargetDoorState
		myGarageDoorDevices.ObstructionDetected = deviceDoor.ObstructionDetected
		myGarageDoorDevices.CurrentDoorState = deviceDoor.CurrentDoorState
		myGarageDoorDevices.DoorPosition = deviceDoor.DoorPosition
		myGarageDoorDevices.TargetDoorOpen = deviceDoor.TargetDoorOpen


	}

	//if error  - do something here. Like? Not responding?
}

func deviceDoorSet (targetDoorState int) error { //send HTTP POST To Arduino with command of target position OPEN/CLOSE
	tepmUrl := arduinoURL + "/door"

	targetDoorOpenPercentage := 0;		//in the future you can pass target positino like 40 %
	//set the JSON
	type garageDoorSetup struct {
		TargetDoorState int `json:"TargetDoorState"`		// otevřeno - zavřeno
		TargetDoorOpen   int `json:"TargetDoorOpen"`		//není implementováno  (% otevření)
	}

	//set the door
	myGarageDoor := garageDoorSetup {
		TargetDoorState: targetDoorState,
		TargetDoorOpen:   targetDoorOpenPercentage,
	}

	b, err := json.Marshal(myGarageDoor)
	if err != nil {
		fmt.Println("error:", err)
	}

	req, err := http.NewRequest("POST", tepmUrl, bytes.NewBuffer(b))

	if err != nil {
		fmt.Printf("http.NewRequest() error: %v\n", err)
		return err
	}
	req.Header.Add("Content-Type", "application/json")

	c := &http.Client{}
	resp, err := c.Do(req)
	if err != nil {
		fmt.Printf("http.Do() error: %v\n", err)
		return err
	}
	defer resp.Body.Close()

	_, err = ioutil.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("ioutil.ReadAll() error: %v\n", err)
		return err
	}



	return nil //no error? Real? I am not sure...
}

//The main CODE

func main() {

	//set Arduino IP adresss from command-line Argument in this format
	//e.g. MY-PROGRAM-NAME.GO -ip=http://129.12.1.0
	//if it is not setup in command-line, default adress is used: defined in var : arduinoURL
	wordPtr := flag.String("ip", arduinoURL, "a string")
	flag.Parse()
	fmt.Println("Arduino IP adress is :", *wordPtr)
	arduinoURL = *wordPtr		//new IP address for ARDUINO


	//setup the HomeKit button
	garageInfo = accessory.Info{		//new GARAGE controller
		Name: "Garage",					//you can change all params - don't use any diacritic -
		SerialNumber: "44",
		Manufacturer: "Martin",
		Model: "11-2017",
	}

	myGarage = accessory.NewGarageDoorSensor(garageInfo, CurrentDoorStateClosed,TargetDoorStateClosed, false)
		//starting params

	config := hc.Config{Pin: "32344322", Port: "12345", StoragePath: "./db3"}
		//***** PIN must be unique for every HomeKitBridge, Storapath is forder for config&temp files

	transportHK, err := hc.NewIPTransport(config, myGarage.Accessory)
		//	transportHK, err := hc.NewIPTransport(config, myDoor.Accessory, next.acc, next.acc2, next.accy3) more accessories you can add
	if err != nil {
		log.Info.Panic(err)
	}



	//GARAGE: when you change the posion on your iPhone
	myGarage.GarageDoorSensor.TargetDoorState.OnValueRemoteUpdate(func(onValue int) {

		//fmt.Print("Your button ")
		//fmt.Println(onValue)
		//if (onValue == 0) {fmt.Println("=UP")} else {fmt.Println("=DOWN")}

		deviceDoorGet()		//get the position from Arduino device - update/change in iPhone
		myGarage.GarageDoorSensor.CurrentDoorState.SetValue(myGarageDoorDevices.CurrentDoorState)
		myGarage.GarageDoorSensor.ObstructionDetected.SetValue(myGarageDoorDevices.ObstructionDetected)

		if onValue == 1 {		//
			fmt.Print("Close from iPhone: ")
			if myGarageDoorDevices.CurrentDoorState == TargetDoorStateClosed {
				fmt.Println("already closed")
			} else {
				fmt.Println("closing...")
				myGarage.GarageDoorSensor.TargetDoorState.SetValue(TargetDoorStateClosed)
				deviceDoorSet(TargetDoorStateClosed)		//sent new position to device.
			}
		} else {
			fmt.Print("Open from iPhone: ")
			if myGarageDoorDevices.CurrentDoorState == TargetDoorStateOpen {
				fmt.Println("already opened")
			} else {
				fmt.Println("opening...")
				myGarage.GarageDoorSensor.TargetDoorState.SetValue(TargetDoorStateOpen)
				deviceDoorSet(TargetDoorStateOpen)			//sent new position to device.
			}
		}
	})

	//GARAGE: UPDATE / check the device every 1 second
	ticker := time.NewTicker(time.Second)
	go func() {
		for t := range ticker.C {
			deviceDoorGet()		//Get the current information from device
			t = t 				//not used so much :-)

			//- update/change in iPhone
			myGarage.GarageDoorSensor.CurrentDoorState.SetValue(myGarageDoorDevices.CurrentDoorState)
			myGarage.GarageDoorSensor.ObstructionDetected.SetValue(myGarageDoorDevices.ObstructionDetected)
		}
	}()


	// ******************** Run INTERNAL WEB SERVER
	mux := http.NewServeMux()
	mux.HandleFunc("/", handleRootDevice)

	s := &http.Server{
		Addr:    ":9090", //TCP address to listen on: maybe localhost:4321
		Handler: mux,
	}
	// Run server in new goroutine
	go s.ListenAndServe()
	// ******************** Run INTERNAL WEB SERVER	*** END


	// ******************** Run HomeKit
	hc.OnTermination(func() {
		//s.Close()						//** peto - proc to nejede??? //
		transportHK.Stop()
	})
	transportHK.Start()
	// ******************** Run HomeKit	*** END
}
