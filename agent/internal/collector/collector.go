package collector

// StatusPacket mirrors status.schema.json v1.
// Fields are pointers so omitempty works correctly for null values.
type StatusPacket struct {
	V          int                    `json:"v"`
	Ts         int64                  `json:"ts"`
	Hostname   string                 `json:"hostname"`
	IPs        []string               `json:"ips"`
	PrimaryIf  string                 `json:"primary_if,omitempty"`
	IfaceType  string                 `json:"iface_type,omitempty"`
	Link       string                 `json:"link,omitempty"`
	Interfaces []NetworkInterfaceInfo `json:"interfaces,omitempty"`
	Temp       *float64               `json:"temp,omitempty"`
	CPU        *float64               `json:"cpu,omitempty"`
	Mem        *float64               `json:"mem,omitempty"`
	MemUsedMB  *float64               `json:"mem_used_mb,omitempty"`
	Disk       *float64               `json:"disk,omitempty"`
	Uptime     string                 `json:"uptime,omitempty"`
	NetTX      *float64               `json:"net_tx,omitempty"`
	NetRX      *float64               `json:"net_rx,omitempty"`
	Alert      bool                   `json:"alert"`
	Services   []Service              `json:"services,omitempty"`
}

// NetworkInterfaceInfo describes one physical network interface, mirroring
// the design's per-interface breakdown (as opposed to the top-level
// primary_if/net_tx/net_rx fields, which summarize the interface Overview
// uses for its single-line net rate).
type NetworkInterfaceInfo struct {
	Name   string   `json:"name"`
	Type   string   `json:"type"` // "wifi" or "ethernet"
	IP     string   `json:"ip,omitempty"`
	Status string   `json:"status"` // "up" or "down"
	SSID   string   `json:"ssid,omitempty"`
	Signal *float64 `json:"signal,omitempty"` // dBm, wifi only
	TXRate *float64 `json:"tx_rate,omitempty"`
	RXRate *float64 `json:"rx_rate,omitempty"`
}

type Service struct {
	Name   string `json:"name"`
	Active bool   `json:"active"`
}

func ptr(v float64) *float64 { return &v }
