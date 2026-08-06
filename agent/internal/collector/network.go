package collector

import (
	"time"

	psnet "github.com/shirou/gopsutil/v3/net"
)

type netSample struct {
	bytesSent uint64
	bytesRecv uint64
	ts        time.Time
}

var prevSample map[string]netSample

// collectNetworkThroughput measures TX/RX bytes per second on every
// interface in pkt.Interfaces, between consecutive calls. First call
// returns nil rates for all interfaces (no delta yet). The primary
// interface's rates are mirrored onto the top-level net_tx/net_rx fields
// that the Overview screen's single net rate line uses.
func collectNetworkThroughput(pkt *StatusPacket) {
	if len(pkt.Interfaces) == 0 {
		return
	}

	counters, err := psnet.IOCounters(true)
	if err != nil {
		return
	}
	byName := make(map[string]*psnet.IOCountersStat, len(counters))
	for i := range counters {
		byName[counters[i].Name] = &counters[i]
	}

	if prevSample == nil {
		prevSample = make(map[string]netSample)
	}
	now := time.Now()

	for i := range pkt.Interfaces {
		iface := &pkt.Interfaces[i]
		current, ok := byName[iface.Name]
		if !ok {
			continue
		}

		prev, hadPrev := prevSample[iface.Name]
		prevSample[iface.Name] = netSample{
			bytesSent: current.BytesSent,
			bytesRecv: current.BytesRecv,
			ts:        now,
		}
		if !hadPrev {
			continue
		}

		elapsed := now.Sub(prev.ts).Seconds()
		if elapsed <= 0 {
			continue
		}

		tx := float64(current.BytesSent-prev.bytesSent) / elapsed
		rx := float64(current.BytesRecv-prev.bytesRecv) / elapsed
		iface.TXRate = ptr(tx)
		iface.RXRate = ptr(rx)

		if iface.Name == pkt.PrimaryIf {
			pkt.NetTX = ptr(tx)
			pkt.NetRX = ptr(rx)
		}
	}
}
