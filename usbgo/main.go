package main

import (
	"fmt"
	"log"
	"math/rand"
	"time"

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

	for {
		amp := byte(rand.Intn(255))
		vel := byte(rand.Intn(255))
		fmt.Printf("Sending amplitude: %d, velocity: %d\n", amp, vel)

		buf, err := sendWithAck(outEndpoint, inEndpoint, []byte{amp, vel})
		if err != nil {
			log.Fatalf("Could not send data: %v", err)
		}

		fmt.Printf("Read %d bytes: %+v\n", len(buf), buf)

		time.Sleep(1 * time.Second)
	}
}

func sendWithAck(outEndpoint *gousb.OutEndpoint, inEndpoint *gousb.InEndpoint, data []byte) ([]byte, error) {
	// Write data to device
	_, err := outEndpoint.Write(data)
	if err != nil {
		return nil, err
	}

	// Read data from device
	buffer := make([]byte, 1) // adjust buffer size based on your needs
	n, err := inEndpoint.Read(buffer)
	if err != nil {
		return nil, err
	}

	return buffer[:n], nil
}
