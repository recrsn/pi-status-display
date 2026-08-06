package collector

import (
	"os/exec"
	"regexp"
	"strconv"
)

var (
	ssidRe   = regexp.MustCompile(`SSID:\s*(.+)`)
	signalRe = regexp.MustCompile(`signal:\s*(-?[0-9.]+)\s*dBm`)
)

// collectWifiInfo returns the SSID and signal strength (dBm) of an
// associated wifi interface via `iw`. Best-effort: returns zero values
// if `iw` is unavailable (e.g. macOS dev) or the interface isn't associated.
func collectWifiInfo(name string) (ssid string, signal *float64) {
	out, err := exec.Command("iw", "dev", name, "link").Output()
	if err != nil {
		return "", nil
	}

	if m := ssidRe.FindSubmatch(out); m != nil {
		ssid = string(m[1])
	}
	if m := signalRe.FindSubmatch(out); m != nil {
		if v, err := strconv.ParseFloat(string(m[1]), 64); err == nil {
			signal = ptr(v)
		}
	}
	return ssid, signal
}
