package collector

import (
	"fmt"
	"net"
	"os"
	"strings"
	"time"

	"github.com/shirou/gopsutil/v3/cpu"
	"github.com/shirou/gopsutil/v3/disk"
	"github.com/shirou/gopsutil/v3/host"
	"github.com/shirou/gopsutil/v3/mem"
)

func collectSystem(pkt *StatusPacket, cpuThresh, tempThresh float64) {
	if h, err := os.Hostname(); err == nil {
		pkt.Hostname = h
	}

	pkt.IPs, pkt.PrimaryIf, pkt.Link = collectInterfaces()

	if percents, err := cpu.Percent(0, false); err == nil && len(percents) > 0 {
		v := percents[0]
		pkt.CPU = ptr(v)
		if v > cpuThresh {
			pkt.Alert = true
		}
	}

	if vm, err := mem.VirtualMemory(); err == nil {
		pkt.Mem = ptr(vm.UsedPercent)
	}

	if usage, err := disk.Usage("/"); err == nil {
		pkt.Disk = ptr(usage.UsedPercent)
	}

	if temps, err := host.SensorsTemperatures(); err == nil {
		for _, t := range temps {
			if strings.Contains(t.SensorKey, "cpu") || strings.Contains(t.SensorKey, "thermal") {
				pkt.Temp = ptr(t.Temperature)
				if t.Temperature > tempThresh {
					pkt.Alert = true
				}
				break
			}
		}
	}

	if uptime, err := host.Uptime(); err == nil {
		pkt.Uptime = formatUptime(uptime)
	}
}

func collectInterfaces() (ips []string, primaryIf, link string) {
	ifaces, err := net.Interfaces()
	if err != nil {
		return
	}
	for _, iface := range ifaces {
		if iface.Flags&net.FlagLoopback != 0 || iface.Flags&net.FlagUp == 0 {
			continue
		}
		addrs, _ := iface.Addrs()
		for _, addr := range addrs {
			if ipnet, ok := addr.(*net.IPNet); ok && ipnet.IP.To4() != nil {
				ips = append(ips, ipnet.IP.String())
				if primaryIf == "" {
					primaryIf = iface.Name
				}
			}
		}
	}
	return ips, primaryIf, ""  // link speed requires ethtool/sysfs; filled separately
}

func formatUptime(secs uint64) string {
	d := time.Duration(secs) * time.Second
	days := int(d.Hours()) / 24
	hours := int(d.Hours()) % 24
	mins := int(d.Minutes()) % 60
	switch {
	case days > 0:
		return fmt.Sprintf("%dd %dh", days, hours)
	case hours > 0:
		return fmt.Sprintf("%dh %dm", hours, mins)
	default:
		return fmt.Sprintf("%dm", mins)
	}
}
