package epoch

import (
	"fmt"
	"strings"
	"testing"
	"unsafe"
)

type aeronStubSet struct {
	open  func(AeronConfig, []byte) aeronHandle
	send  func(aeronHandle, []byte, []byte) int
	poll  func(aeronHandle, []byte, int, []byte) (int, int)
	stats func(aeronHandle, *AeronStats) int
	close func(aeronHandle)
}

func withAeronStubs(stubs aeronStubSet, fn func()) {
	origOpen := aeronOpen
	origSend := aeronSend
	origPoll := aeronPoll
	origStats := aeronStats
	origClose := aeronClose
	if stubs.open != nil {
		aeronOpen = stubs.open
	}
	if stubs.send != nil {
		aeronSend = stubs.send
	}
	if stubs.poll != nil {
		aeronPoll = stubs.poll
	}
	if stubs.stats != nil {
		aeronStats = stubs.stats
	}
	if stubs.close != nil {
		aeronClose = stubs.close
	}
	defer func() {
		aeronOpen = origOpen
		aeronSend = origSend
		aeronPoll = origPoll
		aeronStats = origStats
		aeronClose = origClose
	}()
	fn()
}

func writeErr(buf []byte, msg string) {
	if len(buf) == 0 {
		return
	}
	n := copy(buf, msg)
	if n < len(buf) {
		buf[n] = 0
	}
}

func assertPanic(t *testing.T, contains string, fn func()) {
	t.Helper()
	defer func() {
		if r := recover(); r == nil {
			t.Fatalf("expected panic")
		} else if contains != "" && !strings.Contains(fmt.Sprint(r), contains) {
			t.Fatalf("panic mismatch: %v", r)
		}
	}()
	fn()
}

func TestAeronTransportLifecycle(t *testing.T) {
	dummy := byte(0)
	handle := aeronHandle(unsafe.Pointer(&dummy))
	sample := Message{
		Epoch:     10,
		ChannelID: 11,
		SourceID:  12,
		SourceSeq: 13,
		SchemaID:  14,
		Qos:       1,
		Payload:   99,
	}
	opened := false
	sent := false
	polled := false
	closed := false

	withAeronStubs(aeronStubSet{
		open: func(config AeronConfig, errBuf []byte) aeronHandle {
			opened = true
			return handle
		},
		send: func(h aeronHandle, frame []byte, errBuf []byte) int {
			sent = true
			if h != handle {
				t.Fatalf("unexpected handle in send")
			}
			if len(frame) != aeronFrameLength {
				t.Fatalf("unexpected frame length")
			}
			return 1
		},
		poll: func(h aeronHandle, out []byte, max int, errBuf []byte) (int, int) {
			polled = true
			frame, _ := encodeAeronFrame(sample)
			copy(out[:aeronFrameLength], frame)
			return 1, 1
		},
		stats: func(h aeronHandle, out *AeronStats) int {
			out.SentCount = 7
			out.ReceivedCount = 8
			out.OfferBackPressure = 1
			out.OfferNotConnected = 2
			out.OfferAdminAction = 3
			out.OfferClosed = 4
			out.OfferMaxPosition = 5
			out.OfferFailed = 6
			return 0
		},
		close: func(h aeronHandle) {
			closed = true
			if h != handle {
				t.Fatalf("unexpected handle in close")
			}
		},
	}, func() {
		transport := NewAeronTransport(AeronConfig{
			Channel:  "aeron:udp?endpoint=localhost:40123",
			StreamID: 10,
		})
		if !opened || transport.handle != handle {
			t.Fatalf("expected open to run")
		}
		transport.Send(sample)
		if !sent {
			t.Fatalf("expected send to run")
		}
		out := transport.Poll(1)
		if !polled || len(out) != 1 || out[0] != sample {
			t.Fatalf("unexpected poll result")
		}
		stats := transport.Stats()
		if stats.SentCount != 7 || stats.ReceivedCount != 8 || stats.OfferFailed != 6 {
			t.Fatalf("unexpected stats")
		}
		transport.Close()
		if !closed {
			t.Fatalf("expected close to run")
		}
		transport.Close()
		if transport.Stats() != (AeronStats{}) {
			t.Fatalf("expected zero stats after close")
		}
		if transport.Poll(1) != nil {
			t.Fatalf("expected nil poll after close")
		}
	})
}

func TestAeronTransportErrors(t *testing.T) {
	dummy := byte(0)
	handle := aeronHandle(unsafe.Pointer(&dummy))

	withAeronStubs(aeronStubSet{
		open: func(config AeronConfig, errBuf []byte) aeronHandle {
			writeErr(errBuf, "open failed")
			return nil
		},
	}, func() {
		assertPanic(t, "open failed", func() {
			NewAeronTransport(AeronConfig{Channel: "aeron:udp?endpoint=localhost:40123", StreamID: 1})
		})
	})

	withAeronStubs(aeronStubSet{
		open: func(config AeronConfig, errBuf []byte) aeronHandle {
			return handle
		},
		send: func(h aeronHandle, frame []byte, errBuf []byte) int {
			writeErr(errBuf, "send failed")
			return -1
		},
	}, func() {
		transport := NewAeronTransport(AeronConfig{Channel: "aeron:udp?endpoint=localhost:40123", StreamID: 1})
		assertPanic(t, "send failed", func() {
			transport.Send(Message{Payload: 1})
		})
	})

	withAeronStubs(aeronStubSet{
		open: func(config AeronConfig, errBuf []byte) aeronHandle {
			return handle
		},
		poll: func(h aeronHandle, out []byte, max int, errBuf []byte) (int, int) {
			writeErr(errBuf, "poll failed")
			return -1, 0
		},
	}, func() {
		transport := NewAeronTransport(AeronConfig{Channel: "aeron:udp?endpoint=localhost:40123", StreamID: 1})
		assertPanic(t, "poll failed", func() {
			transport.Poll(1)
		})
	})

	withAeronStubs(aeronStubSet{
		open: func(config AeronConfig, errBuf []byte) aeronHandle {
			return handle
		},
		stats: func(h aeronHandle, out *AeronStats) int {
			return -1
		},
	}, func() {
		transport := NewAeronTransport(AeronConfig{Channel: "aeron:udp?endpoint=localhost:40123", StreamID: 1})
		if transport.Stats() != (AeronStats{}) {
			t.Fatalf("expected zero stats on error")
		}
	})
}

func TestAeronTransportClosedSend(t *testing.T) {
	dummy := byte(0)
	handle := aeronHandle(unsafe.Pointer(&dummy))
	withAeronStubs(aeronStubSet{
		open: func(config AeronConfig, errBuf []byte) aeronHandle {
			return handle
		},
	}, func() {
		transport := NewAeronTransport(AeronConfig{Channel: "aeron:udp?endpoint=localhost:40123", StreamID: 1})
		transport.closed = true
		assertPanic(t, "aeron transport closed", func() {
			transport.Send(Message{Payload: 1})
		})
		if transport.Poll(0) != nil {
			t.Fatalf("expected nil poll with max 0")
		}
	})
}
