package main

import (
	"fmt"
	"time"

	"github.com/elastic/gosigar"
	cpuid "github.com/klauspost/cpuid/v2"
)

func main() {
	cpu := gosigar.Cpu{}
	mem := gosigar.Mem{}

	for {
		cpu.Get()
		mem.Get()

		total := cpu.User + cpu.Nice + cpu.Sys + cpu.Idle
		idle := cpu.Idle
		usage := 100.0 * (1.0 - float64(idle)/float64(total))

		// fmt.Printf("\033[H\033[2J") // Clear screen
		fmt.Printf("CPU Usage: %.2f%%\n", usage)
		fmt.Printf("Memory Usage: %.2f%%\n", 100.0*float64(mem.Used)/float64(mem.Total))
		fmt.Printf("Total Memory: %.2f GB\n", float64(mem.Total)/1024/1024/1024)
		fmt.Printf("Used Memory: %.2f GB\n", float64(mem.Used)/1024/1024/1024)
		fmt.Printf("Free Memory: %.2f GB\n", float64(mem.Free)/1024/1024/1024)

		fmt.Printf("%+v\n", cpu)
		fmt.Printf("%+v\n", cpuid.CPU)

		tsc1 := cpuid.CPU.RTCounter()
		start := time.Now()
		time.Sleep(time.Millisecond * 100)
		tsc2 := cpuid.CPU.RTCounter()
		duration := time.Since(start)

		cycles := tsc2 - tsc1
		seconds := duration.Seconds()
		freq := float64(cycles) / seconds / 1_000_000 // Convert to MHz
		fmt.Printf("CPU Frequency: %.2f MHz\n", freq)

		time.Sleep(time.Second)
	}
}
