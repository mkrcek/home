package accessory

// martinem vytvorene - stav otevření

import (
	"github.com/brutella/hc/service"
)

type Doormetr struct {
	*Accessory

	DoorSensor *service.Door
}

// NewTemperatureSensor returns a Thermometer which implements model.Thermometer.
//  napsat jiny komentar
// třeba takto, ale moc nerozumím....NewDoorSensor vrátí DoorSesror implementuje model.Door

//func NewDoorSensor(info Info, position, state, target, min, max, step  int)  *Doormetr {
func NewDoorSensor(info Info, position, state, target  int)  *Doormetr {
	acc := Doormetr{}

	acc.Accessory = New(info, TypeDoor)
	//acc.Accessory = New(info, TypeGarageDoorOpener)

	acc.DoorSensor= service.NewDoor()
	acc.DoorSensor.CurrentPosition.SetValue(position)
	acc.DoorSensor.PositionState.SetValue(state)
	acc.DoorSensor.TargetPosition.SetValue(target)

	//acc.DoorSensor.CurrentPosition.SetMinValue(min)
	//acc.DoorSensor.CurrentPosition.SetMaxValue(max)
	//acc.DoorSensor.CurrentPosition.SetStepValue(step)

	acc.AddService(acc.DoorSensor.Service)



	return &acc
}
