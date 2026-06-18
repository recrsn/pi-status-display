package collector

// StatusPacket mirrors status.schema.json v1.
// Fields are pointers so omitempty works correctly for null values.
type StatusPacket struct {
	V         int       `json:"v"`
	Hostname  string    `json:"hostname"`
	IPs       []string  `json:"ips"`
	PrimaryIf string    `json:"primary_if,omitempty"`
	Link      string    `json:"link,omitempty"`
	Temp      *float64  `json:"temp,omitempty"`
	CPU       *float64  `json:"cpu,omitempty"`
	Mem       *float64  `json:"mem,omitempty"`
	Disk      *float64  `json:"disk,omitempty"`
	Uptime    string    `json:"uptime,omitempty"`
	NetTX     *float64  `json:"net_tx,omitempty"`
	NetRX     *float64  `json:"net_rx,omitempty"`
	Alert     bool      `json:"alert"`
	Services  []Service `json:"services,omitempty"`
}

type Service struct {
	Name   string `json:"name"`
	Active bool   `json:"active"`
}

func ptr(v float64) *float64 { return &v }
