package transport

import "io"

// Transport writes newline-delimited JSON to a connected destination and
// reads newline-delimited JSON commands from it.
type Transport interface {
	// Write sends a JSON line (caller must not include the trailing newline).
	Write(line []byte) error
	// Commands returns a channel that emits raw command JSON lines as received.
	Commands() <-chan []byte
	// Close tears down the connection.
	Close() error
}

// LineReader buffers input and emits complete newline-terminated lines.
type LineReader struct {
	buf []byte
	out chan []byte
}

func NewLineReader() *LineReader {
	return &LineReader{out: make(chan []byte, 16)}
}

func (r *LineReader) Feed(data []byte) {
	r.buf = append(r.buf, data...)
	for {
		nl := -1
		for i, b := range r.buf {
			if b == '\n' {
				nl = i
				break
			}
		}
		if nl < 0 {
			break
		}
		line := make([]byte, nl)
		copy(line, r.buf[:nl])
		r.buf = r.buf[nl+1:]
		if len(line) > 0 {
			select {
			case r.out <- line:
			default:
				// Slow consumer; drop oldest
				<-r.out
				r.out <- line
			}
		}
	}
}

func (r *LineReader) C() <-chan []byte { return r.out }

// WriteLineTo writes data as a newline-terminated line to w.
func WriteLineTo(w io.Writer, line []byte) error {
	if _, err := w.Write(line); err != nil {
		return err
	}
	_, err := w.Write([]byte{'\n'})
	return err
}
