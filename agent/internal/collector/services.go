package collector

import (
	"context"

	"github.com/coreos/go-systemd/v22/dbus"
)

func collectServices(pkt *StatusPacket, units []string) {
	if len(units) == 0 {
		return
	}

	conn, err := dbus.NewSystemConnectionContext(context.Background())
	if err != nil {
		// Not running on a systemd system (e.g. emulator mode); emit unknown state
		for _, name := range units {
			pkt.Services = append(pkt.Services, Service{Name: name, Active: false})
		}
		return
	}
	defer conn.Close()

	for _, name := range units {
		unitName := name
		if len(unitName) < 8 || unitName[len(unitName)-8:] != ".service" {
			unitName += ".service"
		}
		props, err := conn.GetUnitPropertiesContext(context.Background(), unitName)
		active := err == nil && props["ActiveState"] == "active"
		pkt.Services = append(pkt.Services, Service{Name: name, Active: active})
	}
}
