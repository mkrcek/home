package accessory

// martinem vytvorene - otvírač garáže

import (
	"github.com/brutella/hc/service"
)

type GarageDoormetr struct {
	*Accessory

	GarageDoorSensor *service.GarageDoorOpener
}

// NewTemperatureSensor returns a Thermometer which implements model.Thermometer.
//  napsat jiny komentar
// třeba takto, ale moc nerozumím....NewDoorSensor vrátí DoorSesror implementuje model.Door

func NewGarageDoorSensor(info Info, current, target int, obstruction bool)  *GarageDoormetr {
	acc := GarageDoormetr{}

	acc.Accessory = New(info, TypeGarageDoorOpener)

	acc.GarageDoorSensor = service.NewGarageDoorOpener()
	acc.GarageDoorSensor.CurrentDoorState.SetValue(current)
	acc.GarageDoorSensor.TargetDoorState.SetValue(target)
	acc.GarageDoorSensor.ObstructionDetected.SetValue(obstruction)


	acc.AddService(acc.GarageDoorSensor.Service)



	return &acc
}
