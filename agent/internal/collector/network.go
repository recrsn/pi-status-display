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

// collectNetworkThroughput measures TX/RX bytes per second on primaryIf
// between consecutive calls. First call returns nil values (no delta yet).
func collectNetworkThroughput(pkt *StatusPacket) {
	if pkt.PrimaryIf == "" {
		return
	}

	counters, err := psnet.IOCounters(true)
	if err != nil {
		return
	}

	now := time.Now()
	var current *psnet.IOCountersStat
	for i := range counters {
		if counters[i].Name == pkt.PrimaryIf {
			current = &counters[i]
			break
		}
	}
	if current == nil {
		return
	}

	if prevSample == nil {
		prevSample = make(map[string]netSample)
	}

	prev, ok := prevSample[pkt.PrimaryIf]
	prevSample[pkt.PrimaryIf] = netSample{
		bytesSent: current.BytesSent,
		bytesRecv: current.BytesRecv,
		ts:        now,
	}

	if !ok {
		return
	}

	elapsed := now.Sub(prev.ts).Seconds()
	if elapsed <= 0 {
		return
	}

	tx := float64(current.BytesSent-prev.bytesSent) / elapsed
	rx := float64(current.BytesRecv-prev.bytesRecv) / elapsed
	pkt.NetTX = ptr(tx)
	pkt.NetRX = ptr(rx)
}
