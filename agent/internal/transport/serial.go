package transport

import (
	"go.bug.st/serial"
)

type SerialTransport struct {
	port   serial.Port
	reader *LineReader
	cmds   chan []byte
}

func NewSerial(portName string, baud int) (*SerialTransport, error) {
	mode := &serial.Mode{BaudRate: baud}
	port, err := serial.Open(portName, mode)
	if err != nil {
		return nil, err
	}

	t := &SerialTransport{
		port:   port,
		reader: NewLineReader(),
		cmds:   make(chan []byte, 16),
	}
	go t.readLoop()
	return t, nil
}

func (t *SerialTransport) readLoop() {
	buf := make([]byte, 256)
	for {
		n, err := t.port.Read(buf)
		if err != nil {
			return
		}
		t.reader.Feed(buf[:n])
	}
}

func (t *SerialTransport) Write(line []byte) error {
	return WriteLineTo(t.port, line)
}

func (t *SerialTransport) Commands() <-chan []byte {
	return t.reader.C()
}

func (t *SerialTransport) Close() error {
	return t.port.Close()
}
