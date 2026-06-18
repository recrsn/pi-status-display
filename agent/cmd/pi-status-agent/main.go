package main

import (
	"encoding/json"
	"flag"
	"log"
	"os"
	"os/exec"
	"time"

	"github.com/amitosh/pi-status-display/agent/internal/collector"
	"github.com/amitosh/pi-status-display/agent/internal/config"
	"github.com/amitosh/pi-status-display/agent/internal/transport"
)

func main() {
	cfgPath := flag.String("config", "/etc/pi-status/config.yaml", "path to config file")
	local := flag.Bool("local", false, "socket-only mode: skip serial, write to Unix socket only")
	flag.Parse()

	cfg, err := config.Load(*cfgPath)
	if err != nil {
		if *local {
			cfg = config.Defaults()
			log.Printf("no config file found, using defaults (socket: %s)", cfg.Socket.Path)
		} else {
			log.Fatalf("config: %v", err)
		}
	}

	if *local {
		cfg.Serial.Port = ""
	}

	transports := openTransports(cfg)
	if len(transports) == 0 {
		log.Fatal("no transports available")
	}

	for _, t := range transports {
		go handleCommands(t.Commands())
	}

	ticker := time.NewTicker(cfg.Interval)
	defer ticker.Stop()

	for range ticker.C {
		pkt := collector.Collect(cfg)
		line, err := json.Marshal(pkt)
		if err != nil {
			log.Printf("marshal: %v", err)
			continue
		}
		for _, t := range transports {
			if err := t.Write(line); err != nil {
				log.Printf("write: %v", err)
			}
		}
	}
}

func openTransports(cfg *config.Config) []transport.Transport {
	var ts []transport.Transport

	if cfg.Serial.Port != "" {
		s, err := transport.NewSerial(cfg.Serial.Port, cfg.Serial.Baud)
		if err != nil {
			log.Printf("serial %s: %v (continuing without)", cfg.Serial.Port, err)
		} else {
			ts = append(ts, s)
		}
	}

	if cfg.Socket.Path != "" {
		sock, err := transport.NewSocket(cfg.Socket.Path)
		if err != nil {
			log.Printf("socket %s: %v (continuing without)", cfg.Socket.Path, err)
		} else {
			ts = append(ts, sock)
		}
	}

	return ts
}

func handleCommands(cmds <-chan []byte) {
	type cmd struct {
		Cmd string `json:"cmd"`
	}
	for raw := range cmds {
		var c cmd
		if err := json.Unmarshal(raw, &c); err != nil {
			log.Printf("bad command %q: %v", raw, err)
			continue
		}
		switch c.Cmd {
		case "refresh":
			// next tick will send fresh data; nothing to do
		case "reboot":
			log.Println("command: reboot")
			exec.Command("systemctl", "reboot").Run() //nolint:errcheck
		case "shutdown":
			log.Println("command: shutdown")
			exec.Command("systemctl", "poweroff").Run() //nolint:errcheck
		default:
			log.Printf("unknown command: %s", c.Cmd)
		}
	}
	os.Exit(1) // transport closed unexpectedly
}
