package transport

import (
	"net"
	"os"
	"sync"
)

// SocketTransport listens on a Unix domain socket and broadcasts each written
// line to all connected clients. Commands from any client are forwarded to the
// Commands channel.
type SocketTransport struct {
	listener net.Listener
	mu       sync.Mutex
	conns    []net.Conn
	reader   *LineReader
}

func NewSocket(path string) (*SocketTransport, error) {
	_ = os.Remove(path)
	l, err := net.Listen("unix", path)
	if err != nil {
		return nil, err
	}
	t := &SocketTransport{
		listener: l,
		reader:   NewLineReader(),
	}
	go t.acceptLoop()
	return t, nil
}

func (t *SocketTransport) acceptLoop() {
	for {
		conn, err := t.listener.Accept()
		if err != nil {
			return
		}
		t.mu.Lock()
		t.conns = append(t.conns, conn)
		t.mu.Unlock()
		go t.readConn(conn)
	}
}

func (t *SocketTransport) readConn(conn net.Conn) {
	buf := make([]byte, 256)
	for {
		n, err := conn.Read(buf)
		if err != nil {
			t.removeConn(conn)
			return
		}
		t.reader.Feed(buf[:n])
	}
}

func (t *SocketTransport) removeConn(conn net.Conn) {
	t.mu.Lock()
	defer t.mu.Unlock()
	for i, c := range t.conns {
		if c == conn {
			t.conns = append(t.conns[:i], t.conns[i+1:]...)
			break
		}
	}
	conn.Close()
}

func (t *SocketTransport) Write(line []byte) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	for _, conn := range t.conns {
		WriteLineTo(conn, line) //nolint:errcheck — stale conns cleaned up on next read error
	}
	return nil
}

func (t *SocketTransport) Commands() <-chan []byte {
	return t.reader.C()
}

func (t *SocketTransport) Close() error {
	return t.listener.Close()
}
