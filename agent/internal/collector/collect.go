package collector

import "github.com/amitosh/pi-status-display/agent/internal/config"

// Collect builds a StatusPacket from live system state.
func Collect(cfg *config.Config) *StatusPacket {
	pkt := &StatusPacket{V: 1}
	collectSystem(pkt, cfg.Alerts.CPUThreshold, cfg.Alerts.TempThreshold)
	collectNetworkThroughput(pkt)
	collectServices(pkt, cfg.Services)
	return pkt
}
