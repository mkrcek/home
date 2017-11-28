package main



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
	//"strconv"
	"encoding/json"
)

var arduinoURL string = "http://192.168.0.44"

const (			//Door & GaradeDoor

	PositionStateDecreasing int = 0	//sleduje stav
	PositionStateIncreasing int = 1
	PositionStateStopped    int = 2

	CurrentDoorStateOpen    int = 0	//sleduje stav
	CurrentDoorStateClosed  int = 1
	CurrentDoorStateOpening int = 2
	CurrentDoorStateClosing int = 3
	CurrentDoorStateStopped int = 4

	TargetDoorStateOpen   int = 0	//co se má stát - požadovaný stav garáze = je to ovladač
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

func handleRootDevice(w http.ResponseWriter, req *http.Request) {

	w.Header().Set("Content-Type", "application/text") //nebo text/json
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
			Timeout:   13 * time.Second, //delka trvani timeout, standardně je 30 ---- 13 ..30
			KeepAlive: 30 * time.Second,
			DualStack: true,
		}).DialContext,
		IdleConnTimeout:    30 * time.Second,
		DisableCompression: true,
	}

	client := &http.Client{Transport: tr}

	tepmUrl := arduinoURL + "/door"
	response, err := client.Get(tepmUrl) //musí být použito plné jméno s http:// jinak bude chyba: unsupported protocol scheme

	//	response, err := http.Get("http://192.168.0.39/") 	//nebo to také finguje s tímto - ale není nastaven TIMEOUT"!! můž ebýt kliendě hodinu
	if err != nil { //pokud URL neexistuje, např. Arduino je vypnuté, tak je chyba: Get http://esp8266m.local/: dial tcp: lookup issssdnes.cz: no such host
		fmt.Printf("----nastala tato chyba--\n")
		fmt.Printf("%s\n", err)
		//os.Exit(1)
	} else {
		defer response.Body.Close()
		//fmt.Printf("%s\n", response.Status)
		//fmt.Printf("%v\n", response.StatusCode)
		contents, err := ioutil.ReadAll(response.Body)
		if err != nil {
			fmt.Printf("%s", err)
			os.Exit(1)
		}
		//fmt.Printf("%s\n", string(contents))

		var deviceDoor doorParam
		dataFromDevice := []byte(contents)
		json.Unmarshal(dataFromDevice, &deviceDoor)

		// END ** get information from HTTP GET


		//fmt.Printf("pozice je " + strconv.Itoa(deviceDoor.DoorPosition) + "\n")


		//share information fro device with global var :myGarageDoorDevices
		myGarageDoorDevices.TargetDoorState = deviceDoor.TargetDoorState
		myGarageDoorDevices.ObstructionDetected = deviceDoor.ObstructionDetected
		myGarageDoorDevices.CurrentDoorState = deviceDoor.CurrentDoorState
		myGarageDoorDevices.DoorPosition = deviceDoor.DoorPosition
		myGarageDoorDevices.TargetDoorOpen = deviceDoor.TargetDoorOpen


	}

	//if error  - do something here. Like? Not responding?
}


func deviceDoorSet (targetDoorState, targetDoorOpenPercentage int) error { //send HTTP POST To Arduino with command of target position OPEN/CLOSE
	tepmUrl := arduinoURL + "/door"

	//nastaveni JSON
	type garageDoorSetup struct {
		TargetDoorState int `json:"TargetDoorState"`		// otevřeno - zavřeno
		TargetDoorOpen   int `json:"TargetDoorOpen"`		//není implementováno  (% otevření)
	}

	//nastaveni dveří
	myGarageDoor := garageDoorSetup {
		TargetDoorState: targetDoorState,
		TargetDoorOpen:   targetDoorOpenPercentage,
	}

	b, err := json.Marshal(myGarageDoor)
	if err != nil {
		fmt.Println("error:", err)
	}

	fmt.Print("URL: ")
	fmt.Println(tepmUrl)

	fmt.Print("hodnota v b: ")      //testovaci vypis
	fmt.Println(bytes.NewBuffer(b)) //vypis co se nastavuje

	// *******   hodně  pomalý post ??

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

	data, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("ioutil.ReadAll() error: %v\n", err)
		return err
	}

	fmt.Printf("read resp.Body successfully:\n%v\n", string(data))
	//fmt.Printf("%s\n", resp.Status)
	//fmt.Printf("%v\n", resp.StatusCode)
	fmt.Printf("===odeslano na POST==\n")

	return nil //opravdu nenastala chyba? nevím
}

//program

var pozice, minulaPozice int = 0, 0		//nastavení výchozí pozice 0, tedy zavřeno

var myDoor *accessory.Doormetr//nastavení pro dveře garážové
var doorInfo accessory.Info

var myGarage *accessory.GarageDoormetr
var garageInfo accessory.Info



func main() {

	//get Arduino IP adresss from command-line Argument
	//defaul IP is var arduinoURL string = "http://192.168.0.44"

	arg := os.Args		//take the firt argument
	fmt.Print("arg ")
	fmt.Println(arg)

	doorInfo = accessory.Info{
		Name: "door",
		SerialNumber: "12",
		Manufacturer: "MK",
		Model: "AB",
	}

	garageInfo = accessory.Info{
		Name: "garage",
		SerialNumber: "12",
		Manufacturer: "MK",
		Model: "AB",
	}




									//	-position / state / target /
	//pudodní ---myDoor = accessory.NewDoorSensor(doorInfo, 0, PositionStateStopped, 0)
	myDoor = accessory.NewDoorSensor(doorInfo, PositionStateStopped, CurrentDoorStateClosed, TargetDoorStateClosed)

		//startovací pozice: je 0(zavřeno), sveře stojí, a cíl/target je 0(zavřeno)
	//	myDoor = accessory.NewDoorSensor(doorInfo, deviceDoor(1), 2, 37)


	/*
		myDoor = accessory.NewDoorSensor(doorInfo, deviceDoor(1), 0, 25, 10, 25, 1) - psalo 25 otvřeno
		myDoor = accessory.NewDoorSensor(doorInfo, deviceDoor(1), 0, 25, 10, 100, 1) - psalo Otevřeno, <100 otvirani
		myDoor = accessory.NewDoorSensor(doorInfo, deviceDoor(1), 0, 25, 10, 50, 1) - nenastalo Otevřeno - jen otvirani

	- pozice, která je nastavena v šoupátku na mobilu: myDoor.DoorSensor.TargetPosition.GetValue()
	- otvírání:  (konfig: 0,25,10,100,1
	   když je target např. na 37 (třebas soupátkem na mobilu)
	   tak píše - Otvírá až do vzdálenosti 37
	   ve vzdálenosti 37 se zobrazí "37% otevřeno" a stav je Otevřeno
	   když je nad 37, tak stále píše Otvírání

	*/

	myGarage = accessory.NewGarageDoorSensor(garageInfo, CurrentDoorStateClosed,TargetDoorStateClosed, false)
		//startovací pozice: zavřené dveře, cíl/target je zavřít dveře, bez překážek = false



	//***** PIN musí být unikátní - jiný od ostatních HomeKit Bridgů spuštěných v domácnosti
	config := hc.Config{Pin: "32344322", Port: "12345", StoragePath: "./db3"}

//	transportHK, err := hc.NewIPTransport(config, myDoor.Accessory, myGarage.Accessory) - i s pozici vrat

	transportHK, err := hc.NewIPTransport(config, myGarage.Accessory)
	if err != nil {
		log.Info.Panic(err)
	}


	//GARAGE: zap/vyp z mobilu : Tlačítko Garage Opener
	myGarage.GarageDoorSensor.TargetDoorState.OnValueRemoteUpdate(func(onValue int) {
		fmt.Print("Stav tlacitka ")
		fmt.Println(onValue)
		if (onValue == 0) {fmt.Println("=NAHORE")} else {fmt.Println("=DOLE")}

		fmt.Print("TargetDoorState ")
		fmt.Println(myGarage.GarageDoorSensor.TargetDoorState.GetValue())

		deviceDoorGet()		//zjistí současný stav
		fmt.Print("po aktualizaci dat ")
		myGarage.GarageDoorSensor.CurrentDoorState.SetValue(myGarageDoorDevices.CurrentDoorState)
		myGarage.GarageDoorSensor.ObstructionDetected.SetValue(myGarageDoorDevices.ObstructionDetected)


		if onValue == 1 {		//
			fmt.Println("Zavrit garaz z mobilu")

			if myGarageDoorDevices.CurrentDoorState == TargetDoorStateClosed {
				fmt.Println("už zavřeno")
			} else {
				fmt.Println("zaviram")
				myGarage.GarageDoorSensor.TargetDoorState.SetValue(TargetDoorStateClosed)
				myDoor.DoorSensor.TargetPosition.SetValue(0) 	 //procenta otevření/zavření
				deviceDoorSet(TargetDoorStateClosed,0)
			}
		} else {
			fmt.Println("Otevrit garaz z mobilu")
			if myGarageDoorDevices.CurrentDoorState == TargetDoorStateOpen {
				fmt.Println("už otevreno")
			} else {
				myGarage.GarageDoorSensor.TargetDoorState.SetValue(TargetDoorStateOpen)
				myDoor.DoorSensor.TargetPosition.SetValue(100)
				deviceDoorSet(TargetDoorStateOpen, 100)
			}
		}
	})


	//GARAGE kontrola stavu čisla každou xx sekundu

	ticker := time.NewTicker(time.Second)		//každou sekundu se ptá na stav
	go func() {
		for t := range ticker.C {

			deviceDoorGet()		//zjistí současný stav
			t = t
			//aktualizace tlačítka Garage
			myGarage.GarageDoorSensor.CurrentDoorState.SetValue(myGarageDoorDevices.CurrentDoorState)
			myGarage.GarageDoorSensor.ObstructionDetected.SetValue(myGarageDoorDevices.ObstructionDetected)

			//v budoucnu aktualizace tačítka pozice garáže
			//myDoor.DoorSensor.PositionState.SetValue (myGarageDoorDevices.CurrentDoorState)
			//myDoor.DoorSensor.CurrentPosition.SetValue(myGarageDoorDevices.DoorPosition)

			/*
						pozice = deviceDoor(1)								//zjistí pozici z Arduina

						myDoor.DoorSensor.CurrentPosition.SetValue(pozice)	//předá pozici do HomeKitu-mobilu


						t = t
						fmt.Print(" pozice: ")
						fmt.Print(pozice)
						fmt.Print(" target: ")
						fmt.Print(myDoor.DoorSensor.TargetPosition.GetValue())
						fmt.Print(" >>> ")

						//fmt.Print(" PositionState.GetValue: ")
						//fmt.Println(myDoor.DoorSensor.PositionState.GetValue())


						switch  {
						case pozice == 0:		//je li zavřena garáž
							myDoor.DoorSensor.PositionState.SetValue(PositionStateStopped)
							myGarage.GarageDoorSensor.CurrentDoorState.SetValue(CurrentDoorStateClosed)
							minulaPozice = pozice
							fmt.Println("zavreno 0%")
						case pozice >= 100:		//je li otevřeno 100 %
							myDoor.DoorSensor.PositionState.SetValue(PositionStateStopped)
							myGarage.GarageDoorSensor.CurrentDoorState.SetValue(CurrentDoorStateOpen)
							minulaPozice = pozice
							fmt.Println("Otevreno 100 %")
						case pozice>minulaPozice: //otvírá se garáž (časem přidat chybu cca 5 cm)
							myDoor.DoorSensor.PositionState.SetValue(PositionStateIncreasing)
							myGarage.GarageDoorSensor.CurrentDoorState.SetValue(CurrentDoorStateOpening)
							myGarage.GarageDoorSensor.ObstructionDetected.SetValue(false)	//alarm zrusen, protoze se hybou vrata
							minulaPozice = pozice
							fmt.Println("otvira")
						case pozice<minulaPozice: //zavira se garaz // aráž (časem přidat chybu cca 5 cm)
							myDoor.DoorSensor.PositionState.SetValue(PositionStateDecreasing)
							myGarage.GarageDoorSensor.CurrentDoorState.SetValue(CurrentDoorStateClosing)
							myGarage.GarageDoorSensor.ObstructionDetected.SetValue(false)	//alarm zrusen, protoze se hybou vrata
							minulaPozice = pozice
							fmt.Println("zavira ")
						case pozice == myDoor.DoorSensor.TargetPosition.GetValue():		//cílova pozice
							myDoor.DoorSensor.PositionState.SetValue(PositionStateStopped)
							myGarage.GarageDoorSensor.CurrentDoorState.SetValue(CurrentDoorStateStopped)
							minulaPozice = pozice
							fmt.Println("zastaveno na cilove pozici ")
						case pozice == minulaPozice:		//dveře se zastavily
							myDoor.DoorSensor.PositionState.SetValue(PositionStateStopped)
							myGarage.GarageDoorSensor.CurrentDoorState.SetValue(CurrentDoorStateStopped)
							myGarage.GarageDoorSensor.ObstructionDetected.SetValue(true)	//alarm se zastavilo
							fmt.Println("prekazka ve vratech")
							//nic se ale na mobilu neděje
						}
			*/
					}
				}()






	// ********************

	mux := http.NewServeMux()
	mux.HandleFunc("/", handleRootDevice)

	s := &http.Server{
		Addr:    ":9090", //TCP address to listen on: v tomto případě localhost:4321
		Handler: mux,
	}
	// Run server in new goroutine
	go s.ListenAndServe()

	// ********************

	hc.OnTermination(func() {
		//s.Close()						//** peto - proc to nejede??? //
		transportHK.Stop()
	})
	transportHK.Start()
}
