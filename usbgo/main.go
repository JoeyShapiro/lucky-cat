package main

import (
	"fmt"
	"log"

	"github.com/google/gousb"
)

func main() {
	// Create a new USB context
	ctx := gousb.NewContext()
	defer ctx.Close()

	// Open device by vendor and product IDs
	// Replace these with your device's IDs
	vid, pid := gousb.ID(0x05ac), gousb.ID(0x1261)

	dev, err := ctx.OpenDeviceWithVIDPID(vid, pid)
	if err != nil {
		log.Fatalf("Could not open device: %v", err)
	}
	if dev == nil {
		log.Fatalf("Device not found")
	}
	defer dev.Close()

	// Get the active configuration
	config, err := dev.Config(1)
	if err != nil {
		log.Fatalf("Could not get config: %v", err)
	}
	defer config.Close()

	// Get interface #0 with alternate setting 0
	intf, err := config.Interface(0, 0)
	if err != nil {
		log.Fatalf("Could not get interface: %v", err)
	}
	defer intf.Close()

	// Get IN and OUT endpoints
	inEndpoint, err := intf.InEndpoint(1) // endpoint address 0x81
	if err != nil {
		log.Fatalf("Could not get IN endpoint: %v", err)
	}

	outEndpoint, err := intf.OutEndpoint(1) // endpoint address 0x02
	if err != nil {
		log.Fatalf("Could not get OUT endpoint: %v", err)
	}

	// Write data to device
	data := []byte{0x01, 0x02}
	numBytes, err := outEndpoint.Write(data)
	if err != nil {
		log.Fatalf("Write error: %v", err)
	}
	fmt.Printf("Wrote %d bytes\n", numBytes)

	// Read data from device
	buffer := make([]byte, 64) // adjust buffer size based on your needs
	numBytes, err = inEndpoint.Read(buffer)
	if err != nil {
		log.Fatalf("Read error: %v", err)
	}
	fmt.Printf("Read %d bytes: %+v\n", numBytes, buffer[:numBytes])
}
