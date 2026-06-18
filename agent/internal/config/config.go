package config

import (
	"os"
	"time"

	"gopkg.in/yaml.v3"
)

type Config struct {
	Serial   SerialConfig   `yaml:"serial"`
	Socket   SocketConfig   `yaml:"socket"`
	Interval time.Duration  `yaml:"interval"`
	Alerts   AlertsConfig   `yaml:"alerts"`
	Services []string       `yaml:"services"`
}

type SerialConfig struct {
	Port string `yaml:"port"`
	Baud int    `yaml:"baud"`
}

type SocketConfig struct {
	Path string `yaml:"path"`
}

type AlertsConfig struct {
	CPUThreshold  float64 `yaml:"cpu_threshold"`
	TempThreshold float64 `yaml:"temp_threshold"`
}

func Defaults() *Config {
	return &Config{
		Serial:   SerialConfig{Port: "/dev/pi-display", Baud: 115200},
		Socket:   SocketConfig{Path: "/tmp/pi-status.sock"},
		Interval: time.Second,
		Alerts:   AlertsConfig{CPUThreshold: 80, TempThreshold: 70},
	}
}

func Load(path string) (*Config, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	cfg := &Config{
		Serial:   SerialConfig{Port: "/dev/pi-display", Baud: 115200},
		Socket:   SocketConfig{Path: "/tmp/pi-status.sock"},
		Interval: time.Second,
		Alerts:   AlertsConfig{CPUThreshold: 80, TempThreshold: 70},
	}
	if err := yaml.NewDecoder(f).Decode(cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}
