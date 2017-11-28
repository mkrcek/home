package main

import (
	"net/http"
	"io/ioutil"
	"strconv"
	"encoding/json"
	"bytes"
	"fmt"
)

func deviceSetDigiPin(pin, pinValue, pinPwm int) error {    //nastaví digitální PIN na 0/1 nebo PWM
	tepmUrl := "http://esp8266m.local/pins/" + strconv.Itoa(pin)
	//tepmUrl := "http://localhost:9090/pins/" + strconv.Itoa(pin)
	//tepmUrl := "http://192.168.0.41/pins/" + strconv.Itoa(pin)

	//nastaveni JSON
	type pinSetup struct {
		DeviceValue  int `json:"value"`
		DevicePwm    int `json:"pwm"`
	}

	//nastaveni pinů
	devicePinSetup := pinSetup{
		DeviceValue: pinValue,
		DevicePwm: pinPwm,
	}

	b, err := json.Marshal(devicePinSetup)
	if err != nil {
		fmt.Println("error:", err)
	}

	fmt.Println("URL: ")
	fmt.Println(tepmUrl)

	fmt.Print("hodnota v bufferu: ")				//testovaci vypis
	fmt.Println (bytes.NewBuffer(b))		//vypis co se nastavuje




	// *******   hodně  pomalý post

	req, err := http.NewRequest("POST", tepmUrl, bytes.NewBuffer(b))

	fmt.Print("poslan post a chyby je ")
	fmt.Println(err)

	if err != nil {
		fmt.Printf("http.NewRequest() error: %v\n", err)
		return err
	}
	//req.Header.Add("Content-Type", "application/json")
	//req.Header.Add("Connection", "keep-alive")


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


	return nil			//opravdu nenastala chyba? nevím
}

func main()  {

	deviceSetDigiPin(6,1, 0)
}