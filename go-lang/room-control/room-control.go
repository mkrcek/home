package main


// ---------------------------------------------------------------------------
// Room Control by HTTP GET & POST + Apple HOMEKIT
//			SERVER
// ---------------------------------------------------------------------------
//
// sorry - not translated yet :-(
//
// to RUN with param = IP address of Arduino device
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
	"strconv"
	"encoding/json"
	"bytes"
	"strings"
	"flag"
)

var arduinoURL string = "http://192.168.100.201"		//default - you can change it with command-line Argument - see main()


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

func handleOptionsDevice(w http.ResponseWriter, req *http.Request) {
	//odpověď na volání, pokud by se místo POST klient ptal na OPTIONS
	//tato varianta je pro CORS - https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request

	w.Header().Set("Content-Length", "0")
	w.Header().Set("Connection", "keep-alive")
	//rw.Header().Set("Access-Control-Allow-Origin", "http://localhost:8080/device")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE")
	w.Header().Set("Access-Control-Allow-Headers", "X-Requested-With,content-type");
	w.Header().Set("Access-Control-Max-Age", "86400")
	w.WriteHeader(200)
	return
}

func handlePins(w http.ResponseWriter, req *http.Request) {
	//nastaví dle poždavdu na PINy

	type HomePins struct {
		Value int `json:"value"`
		Pwm   int `json:"pwm"`
	}

	/*//nastavení hlaviček:
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE")
	w.Header().Set("Access-Control-Allow-Headers", "X-Requested-With,content-type");
	w.Header().Set("Access-Control-Max-Age", "86400")
*/
	var err error

	switch req.Method {
	case "GET":
		handleRootDevice(w, req)
	case "OPTIONS":
		handleOptionsDevice(w, req)
	case "POST":
		{
			//nalezení čísla PINS za lomítkem, tedy /pins/1 = 1
			myURL := req.RequestURI        // req.URL vs req.RequestURI		"/pins/1"
			myURL = myURL[1:]              //odstrani první lomítko -> pins/1 = 1
			i := strings.Index(myURL, "/") //vrátí poradi prvniho lomítka
			if i > -1 {
				//apiName := myURL[:i]		//vyraz před lomítkem: "pins"
				apiAfter := myURL[i+1:] //vyraz za lomítkom: "1"
				//pinNumber = int(apiAfter)

				pinNumber, err := strconv.Atoi(apiAfter)
				if err != nil {
					w.WriteHeader(400)
					return
				}
				fmt.Print(" PIN number: ") //číslo PINu, které poslalo request
				fmt.Println(pinNumber)
				len := req.ContentLength
				body := make([]byte, len)
				req.Body.Read(body)
				var post HomePins				//vytěžení z JSON parametrů
				json.Unmarshal(body, &post)
				fmt.Print(" POST: ")
				fmt.Println(post)


				//*********
				//AKCE PRO HOMEKIT (pinNumber, post.Value, post.Pwm

				switch pinNumber {
				case 5:		//RELE 5
					{
						if post.Value == 1 {
							myOutletInhouse.Outlet.On.SetValue(true)
							log.Info.Println("Switch turned ZAP")
							w.Write([]byte("Switch turned ZAP"))
						} else {
							myOutletInhouse.Outlet.On.SetValue(false)
							log.Info.Println("Switch turned VYP")
							w.Write([]byte("Switch turned VYP"))
						}
						//w.WriteHeader(200) -  pokud nenín zakomentované nastane: multiple response.WriteHeader calls
						return
					}
				case 6:		//RELE 6
					{
						if post.Value == 1 {
							myOutletOut.Outlet.On.SetValue(true)
							log.Info.Println("Switch turned ZAP")
							w.Write([]byte("Switch turned ZAP"))
						} else {
							myOutletOut.Outlet.On.SetValue(false)
							log.Info.Println("Switch turned VYP")
							w.Write([]byte("Switch turned VYP"))
						}
						//w.WriteHeader(200) -  pokud nenín zakomentované nastane: multiple response.WriteHeader calls
						return
					}
				case 2:		//PIR MOTION on PIN 2 - PIR morion / Alarm
					{
						log.Info.Print("hodnota val")
						log.Info.Println(post.Value)
						log.Info.Print("hodnota motion")
						log.Info.Println(myMotionPir.MotionPinSensor)
						var pohyb bool
						pohyb = myMotionPir.MotionPinSensor.MotionDetected.GetValue()

						log.Info.Println(pohyb)

						if post.Value == 1 {
//							accessory.NewMotionPirSensor.MotionDetected. = true
							myMotionPir.MotionPinSensor.MotionDetected.SetValue(true)
							log.Info.Println("Pohyb detekován")
							w.Write([]byte("Pohyb detekován"))
						} else {
							myMotionPir.MotionPinSensor.MotionDetected.SetValue(false)
							log.Info.Println("Pohyb ukončen")
							w.Write([]byte("Pohyb ukončen"))
						}
						//w.WriteHeader(200) -  pokud nenín zakomentované nastane: multiple response.WriteHeader calls
						return
					}
				}

			} else {
				fmt.Print("Index not found: ")
				fmt.Println(myURL)
				w.WriteHeader(400) //nebyl nalazen parametr, který pin
				return
			}
		}
		if err != nil {
			w.WriteHeader(400) //
			return
		}
	}

}

func deviceSetDigiPin(pin, pinValue, pinPwm int) error { //nastaví digitální PIN na 0/1 nebo PWM
	tepmUrl := arduinoURL + "/pins/" + strconv.Itoa(pin)

	//nastaveni JSON
	type pinSetup struct {
		DeviceValue int `json:"value"`
		DevicePwm   int `json:"pwm"`
	}

	//nastaveni pinů
	devicePinSetup := pinSetup{
		DeviceValue: pinValue,
		DevicePwm:   pinPwm,
	}

	b, err := json.Marshal(devicePinSetup)
	if err != nil {
		fmt.Println("error:", err)
	}

	fmt.Println("URL: ")
	fmt.Println(tepmUrl)

	fmt.Print("hodnota v b: ")      //testovaci vypis
	fmt.Println(bytes.NewBuffer(b)) //vypis co se nastavuje

	// *******   hodně  pomalý post

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

func deviceReadTempHum(pin int) (float64) {

	//teplota a vlhkost
	type tempHumid struct {
		DeviceTemperature int `json:"deviceTemperature"`
		DeviceHumidity    int `json:"deviceHumidity"`
	}

	//nastavení parametrů na nejjižší urovni TCP komunikace
	//popis: https://golang.org/src/net/http/transport.go
	tr := &http.Transport{
		MaxIdleConns: 10,
		DialContext: (&net.Dialer{
			Timeout:   13 * time.Second, //delka trvani timeout, standardně je 30
			KeepAlive: 30 * time.Second,
			DualStack: true,
		}).DialContext,
		IdleConnTimeout:    30 * time.Second,
		DisableCompression: true,
	}

	client := &http.Client{Transport: tr}

	tepmUrl := arduinoURL + "/pins/" + strconv.Itoa(pin)
	response, err := client.Get(tepmUrl) //musí být použito plné jméno s http:// jinak bude chyba: unsupported protocol scheme

	//	response, err := http.Get("http://192.168.0.39/") 	//nebo to také finguje s tímto - ale není nastaven TIMEOUT"!! můž ebýt kliendě hodinu
	if err != nil { //pokud URL neexistuje, např. Arduino je vypnuté, tak je chyba: Get http://esp8266m.local/: dial tcp: lookup issssdnes.cz: no such host
		fmt.Printf("----nastala tato chyba--\n")
		fmt.Printf("%s\n", err)
		//os.Exit(1)
	} else {
		defer response.Body.Close()
		fmt.Printf("%s\n", response.Status)
		fmt.Printf("%v\n", response.StatusCode)
		contents, err := ioutil.ReadAll(response.Body)
		if err != nil {
			fmt.Printf("%s", err)
			os.Exit(1)
		}
		fmt.Printf("%s\n", string(contents))

		var deviceTepmHumid tempHumid
		dataFromDevice := []byte(contents)
		json.Unmarshal(dataFromDevice, &deviceTepmHumid)

		fmt.Printf("teplota je " + strconv.Itoa(deviceTepmHumid.DeviceTemperature) + "\n")
		fmt.Printf("vlhkost je " + strconv.Itoa(deviceTepmHumid.DeviceHumidity) + "\n")
//		return float64(deviceTepmHumid.DeviceTemperature), float64(deviceTepmHumid.DeviceHumidity)
		return float64(deviceTepmHumid.DeviceTemperature)
	}
	//return 0,0
	return 0
}

func deviceSetPir(pin, interval int) error { //nastaví délku PIR intervalu
	tepmUrl := arduinoURL + "/pirs/" + strconv.Itoa(pin)

	//nastaveni JSON
	type pinSetup struct {
		DeviceInterval int `json:"interval"`
	}

	//nastaveni pinů pro PIR
	devicePinSetup := pinSetup{
		DeviceInterval: interval,
	}

	fmt.Println("odeslanini na PIR: ")

	b, err := json.Marshal(devicePinSetup)
	if err != nil {
		fmt.Println("error:", err)
	}

	fmt.Println("URL: ")
	fmt.Println(tepmUrl)

	fmt.Print("hodnota v b: ")      //testovaci vypis
	fmt.Println(bytes.NewBuffer(b)) //vypis co se nastavuje

	// *******   hodně  pomalý post vyřešen ne mDNS ale IP adresou

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

var teplota float64
//var vlhkost  float64

var myOutletInhouse *accessory.Outlet
var OutletInhouse accessory.Info

var myOutletOut *accessory.Outlet
var OutletOut accessory.Info

var myLed *accessory.Lightbulb
var ledInfo accessory.Info

//PIR
var myMotionPir *accessory.MotionPir
var motionInfo accessory.Info

var myMotionSetup *accessory.Lightbulb		//nastavení délky sepnutní pro PIR
var motionSetupInfo accessory.Info

var pairingCode string = "22344322"
var pairingPort string = "12342"
var pairingStoragePath string = "./dbroom"
var serverPort = ":9091"


//The main CODE


func main() {
	fmt.Println("Room Control by HTTP GET & POST + Apple HOMEKIT")
	fmt.Println("  server  ")
	fmt.Println("you can use arguments like -ip=http://129.12.1.0 -pin=22344322 -port=12342 -path=./dbroom -serverport=.9091")
	fmt.Println("  ...  ")
	//set Arduino IP adresss from command-line Argument in this format
	//e.g. MY-PROGRAM-NAME.GO -ip=http://129.12.1.0
	//if it is not setup in command-line, default adress is used: defined in var : arduinoURL

	wordPtr := flag.String("ip", arduinoURL, "a string")
	wordPtrCode := flag.String("pin", pairingCode, "a string")
	wordPtrPort := flag.String("port", pairingPort, "a string")
	wordPtrPath := flag.String("path", pairingStoragePath, "a string")
	wordPtrServerPort := flag.String("serverport", serverPort, "a string")

	flag.Parse()
	fmt.Println("Arduino IP adress is :", *wordPtr)
	arduinoURL = *wordPtr		//new IP address for ARDUINO

	fmt.Println("HomeKit pairing Code (pin) is :", *wordPtrCode)
	pairingCode = *wordPtrCode		//new pairing HomeKit Code

	fmt.Println("HomeKit Port is :", *wordPtrPort)
	pairingPort = *wordPtrPort		//new HomeKit Port

	fmt.Println("HomeKit Path is :", *wordPtrPath)
	pairingStoragePath = *wordPtrPath		//new HomeKit Path

	fmt.Println("HomeKit Server Port is :", *wordPtrServerPort)
	serverPort = *wordPtrServerPort		//new HomeKitServer Port

	thermometerInfo := accessory.Info{
		Name: "Temperature",
		SerialNumber: "50",
		Manufacturer: "Martin",
		Model: "11-2017",
	}


	myThermometr := accessory.NewTemperatureSensor(thermometerInfo, deviceReadTempHum(0), -50, 100, 1)

	OutletInhouse = accessory.Info{
		Name: "room", //název NESMÍ mít DIAKRITIKU - jinak se nerozjede
		SerialNumber: "51",
		Manufacturer: "Martin",
		Model: "11-2017",
	}
	myOutletInhouse = accessory.NewOutlet(OutletInhouse)

	OutletOut = accessory.Info{
		Name: "garden",
		SerialNumber: "52",
		Manufacturer: "Martin",
		Model: "11-2017",
	}
	myOutletOut = accessory.NewOutlet(OutletOut)

	ledInfo = accessory.Info{
		Name: "LED",
		SerialNumber: "53",
		Manufacturer: "Martin",
		Model: "11-2017",
	}
	myLed = accessory.NewLightbulb(ledInfo)
	myLed.Lightbulb.Brightness.Value = 30


	motionInfo = accessory.Info{
		Name: "PIR-cidlo",
		SerialNumber: "54",
		Manufacturer: "Martin",
		Model: "11-2017",
	}
	myMotionPir = accessory.NewMotionPirSensor(motionInfo, false)

	motionSetupInfo = accessory.Info{
		Name: "PIR_setup",
		SerialNumber: "55",
		Manufacturer: "Martin",
		Model: "11-2017",
	}
	myMotionSetup = accessory.NewLightbulb(motionSetupInfo)
	myMotionSetup.Lightbulb.Brightness.Value = 5
	myMotionSetup.Lightbulb.On.Value = true



	//zapnutí a konfigurace

	//parovací kod je 12344321
	//config := hc.Config{Pin: "12344321", Port: "12345", StoragePath: "./db"}
	config := hc.Config{Pin: pairingCode, Port: pairingPort, StoragePath: pairingStoragePath}
	//***** PIN must be unique for every HomeKitBridge, Storapath is forder for config&temp files


	//nově Motion
	transportHK, err := hc.NewIPTransport(config, myThermometr.Accessory, myOutletInhouse.Accessory, myOutletOut.Accessory, myLed.Accessory, myMotionPir.Accessory, myMotionSetup.Accessory)
	if err != nil {
		log.Info.Panic(err)
	}

	// VNITŘNÍ světlo: změna na mobilu
	myOutletInhouse.Outlet.On.OnValueRemoteUpdate(func(on bool) {
		if on == true {
			deviceSetDigiPin(5, 1, 0)
			fmt.Println("Vnitřní světlo je zapnuto")
		} else {
			deviceSetDigiPin(5, 0, 0)
			fmt.Println("Vnitřní světlo je vypnuto")
		}
	})

	// VENKOVNI světlo: změna na mobilu
	myOutletOut.Outlet.On.OnValueRemoteUpdate(func(on bool) {
		if on == true {
			deviceSetDigiPin(6, 1, 0)
			fmt.Println("Venkovní světlo je zapnuto")
		} else {
			deviceSetDigiPin(6, 0, 0)
			fmt.Println("venkovní světlo je vypnuto")
		}
	})

	//LED: změna jasu na telefonu
	myLed.Lightbulb.Brightness.OnValueRemoteUpdate(func(pinDeviceBrightness int) {
		deviceBrightness := (1023 * pinDeviceBrightness / 100) //převod z procent na cislo do 1023
		switch deviceBrightness {
		case 0:
			deviceSetDigiPin(7, 0, 0)
		case 100:
			deviceSetDigiPin(7, 1, 0)
		default:
			deviceSetDigiPin(7, 0, deviceBrightness)
		}
	})

	//LED: zap/vyp na telefonu a zachová jas
	myLed.Lightbulb.On.OnValueRemoteUpdate(func(on bool) {
		var jas int
		jas = (1023 * myLed.Lightbulb.Brightness.Value.(int) / 100) //převod z procent na cislo do 1023
		if on == true {
			fmt.Println("Lampa nastavena na ON")
			if jas == 0 {
				deviceSetDigiPin(7, 1, 0)
			} else {
				deviceSetDigiPin(7, 1, jas)
			}

		} else {
			fmt.Println("Lampa nastavena na OFF")
			deviceSetDigiPin(7, 0, 0)
		}
	})

	//TEPLOTA: změří teplotu každých každých 60s (nový ticker - běží na pozadí, jako cron)
	ticker := time.NewTicker(time.Second * 60)
	go func() {
		for t := range ticker.C {
			teplota = deviceReadTempHum(0)
			myThermometr.TempSensor.CurrentTemperature.Value = teplota
			fmt.Print("v čase ", t)
			fmt.Print(" Teplota: ")
			fmt.Println(teplota)
		}
	}()



	//PIR-setup: změna času a vypnutí
	myMotionSetup.Lightbulb.Brightness.OnValueRemoteUpdate(func(pinDeviceInterval int) {
		devicePirInterval := (600000 * pinDeviceInterval / 100) //převod z procent na cislo 600.000 tedy 10 minut na delku sepnutí PIR
		switch devicePirInterval {
		case 0:
			deviceSetPir(2, 0)
		case 100:
			deviceSetPir(2, 600000)
		default:
			deviceSetPir(2, devicePirInterval)
		}
	})

	//PIR-setup: zap/vyp na telefonu a zachová jas
	myMotionSetup.Lightbulb.On.OnValueRemoteUpdate(func(on bool) {
		var interval int
		interval = (600000 * myMotionSetup.Lightbulb.Brightness.Value.(int) / 100) //převod z procent na cislo 600.000 tedy 10 minut na delku sepnutí PIR
		if on == true {
			fmt.Println("PIR nastaveno na ON")
			if interval == 0 {
				deviceSetPir(2, 0)
			} else {
				deviceSetPir(2, interval)
			}

		} else {
			fmt.Println("PIR nastavena na OFF")
			deviceSetPir(2, 0)
		}
	})







	// ********************

	mux := http.NewServeMux()
	mux.HandleFunc("/pins/", handlePins) //poslaný přes POST
	mux.HandleFunc("/", handleRootDevice)

	s := &http.Server{
		Addr:    serverPort, //TCP address to listen on: v tomto případě localhost:4321
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

//Cross compile with Go 1.5 for

/*
Raspberry Pi: $ GOOS=linux GOARCH=arm GOARM=6 go build -v room-control.go
//5 & 6 works on Raspberian : Raspberry Pi Zero W
Windows:      $ GOOS=windows GOARCH=386 go build -v room-control.exe room-control.go   //??
Windows10: 	$ GOOS=windows GOARCH=amd64 go build -v room-control-64.exe room-control.go

-v:
  print the names of packages as they are compiled.

On RPi - maybe you need to change the right of the file
	$ chmod +x room-control

and run program
	$ sudo ./room-control
*/