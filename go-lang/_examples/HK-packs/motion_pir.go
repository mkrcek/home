package accessory

// martinem vytvorene

import (
	"github.com/brutella/hc/service"
)

type MotionPir struct {
	*Accessory

	MotionPinSensor *service.MotionSensor
}

//komentář

func NewMotionPirSensor(info Info, motion bool) *MotionPir {
	acc := MotionPir{}
	acc.Accessory = New(info, TypeSecuritySystem)
	acc.MotionPinSensor = service.NewMotionSensor()
	acc.MotionPinSensor.MotionDetected.SetValue(motion)

	acc.AddService(acc.MotionPinSensor.Service)

	return &acc
	}