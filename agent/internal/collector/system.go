package collector

import (
	"fmt"
	"net"
	"os"
	"sort"
	"strings"
	"time"

	"github.com/shirou/gopsutil/v3/cpu"
	"github.com/shirou/gopsutil/v3/disk"
	"github.com/shirou/gopsutil/v3/host"
	"github.com/shirou/gopsutil/v3/mem"
)

func collectSystem(pkt *StatusPacket, cpuThresh, tempThresh float64) {
	pkt.Ts = time.Now().Unix()

	if h, err := os.Hostname(); err == nil {
		pkt.Hostname = h
	}

	pkt.Interfaces, pkt.IPs, pkt.PrimaryIf, pkt.IfaceType = collectInterfaces()

	if percents, err := cpu.Percent(0, false); err == nil && len(percents) > 0 {
		v := percents[0]
		pkt.CPU = ptr(v)
		if v > cpuThresh {
			pkt.Alert = true
		}
	}

	if vm, err := mem.VirtualMemory(); err == nil {
		pkt.Mem = ptr(vm.UsedPercent)
		pkt.MemUsedMB = ptr(float64(vm.Used) / (1024 * 1024))
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

// isPhysicalInterface reports whether name looks like a physical NIC
// (eth*/en*/wlan*/wl*), filtering out loopback, docker/veth/bridge and
// other virtual interfaces that would otherwise clutter the Network screen.
func isPhysicalInterface(name string) bool {
	for _, prefix := range [...]string{"eth", "en", "wlan", "wl"} {
		if strings.HasPrefix(name, prefix) {
			return true
		}
	}
	return false
}

// collectInterfaces enumerates physical network interfaces, returning both
// the full per-interface breakdown (for the Network screen) and the
// summarized ips/primaryIf/ifaceType used elsewhere (e.g. the Overview
// screen's single net rate line). Interfaces are sorted up-before-down so
// that active links always occupy the Network screen's limited slots ahead
// of down ones (relevant mainly on hosts with many virtual en* devices,
// like macOS dev machines). primaryIf is the first up interface after
// sorting.
func collectInterfaces() (interfaces []NetworkInterfaceInfo, ips []string, primaryIf, ifaceType string) {
	ifaces, err := net.Interfaces()
	if err != nil {
		return
	}
	for _, iface := range ifaces {
		if !isPhysicalInterface(iface.Name) {
			continue
		}

		var ip string
		addrs, _ := iface.Addrs()
		for _, addr := range addrs {
			if ipnet, ok := addr.(*net.IPNet); ok && ipnet.IP.To4() != nil {
				ip = ipnet.IP.String()
				break
			}
		}

		up := iface.Flags&net.FlagUp != 0 && ip != ""
		typ := classifyInterface(iface.Name)

		info := NetworkInterfaceInfo{
			Name:   iface.Name,
			Type:   typ,
			IP:     ip,
			Status: statusStr(up),
		}
		if typ == "wifi" && up {
			info.SSID, info.Signal = collectWifiInfo(iface.Name)
		}
		interfaces = append(interfaces, info)

		if ip != "" {
			ips = append(ips, ip)
		}
	}

	sort.SliceStable(interfaces, func(i, j int) bool {
		return interfaces[i].Status == "up" && interfaces[j].Status != "up"
	})

	for _, info := range interfaces {
		if info.IP != "" {
			primaryIf = info.Name
			ifaceType = info.Type
			break
		}
	}

	return interfaces, ips, primaryIf, ifaceType
}

func statusStr(up bool) string {
	if up {
		return "up"
	}
	return "down"
}

// classifyInterface reports "wifi" or "ethernet" for a Linux network interface
// by checking for the kernel's wireless sysfs node. Always "ethernet" on
// non-Linux hosts (e.g. macOS dev), since that node doesn't exist there.
func classifyInterface(name string) string {
	if _, err := os.Stat("/sys/class/net/" + name + "/wireless"); err == nil {
		return "wifi"
	}
	return "ethernet"
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
