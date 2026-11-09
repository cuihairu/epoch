package epoch

import (
	"encoding/binary"
	"testing"
	"unsafe"
)

func TestAeronPollRejectsCorruptFrame(t *testing.T) {
	dummy := byte(0)
	frame := make([]byte, aeronFrameLength)
	binary.LittleEndian.PutUint64(frame[8:16], 1)
	frame[0] = 99
	withAeronStubs(aeronStubSet{
		open: func(AeronConfig, []byte) aeronHandle { return aeronHandle(unsafe.Pointer(&dummy)) },
		poll: func(_ aeronHandle, frameBuf []byte, max int, errBuf []byte) (int, int) {
			copy(frameBuf, frame)
			return 1, 1
		},
		close: func(aeronHandle) {},
	}, func() {
		transport := NewAeronTransport(AeronConfig{})
		defer transport.Close()
		assertPanic(t, "unsupported aeron frame version", func() {
			transport.Poll(1)
		})
	})
}

func TestProcessMessagesSortsSourceSeqWithinEpoch(t *testing.T) {
	messages := []Message{
		{Epoch: 7, ChannelID: 1, Qos: 3, SourceID: 5, SourceSeq: 2, Payload: 20},
		{Epoch: 7, ChannelID: 1, Qos: 3, SourceID: 5, SourceSeq: 1, Payload: 10},
	}
	results := ProcessMessages(messages)
	if len(results) != 1 || results[0].State != 30 {
		t.Fatalf("unexpected results: %+v", results)
	}
}

func TestProcessMessagesSortsBySourceID(t *testing.T) {
	messages := []Message{
		{Epoch: 5, ChannelID: 1, Qos: 1, SourceID: 3, SourceSeq: 1, SchemaID: 100, Payload: 30},
		{Epoch: 5, ChannelID: 1, Qos: 1, SourceID: 1, SourceSeq: 1, SchemaID: 100, Payload: 10},
		{Epoch: 5, ChannelID: 1, Qos: 1, SourceID: 2, SourceSeq: 1, SchemaID: 100, Payload: 20},
	}
	results := ProcessMessages(messages)
	if len(results) != 1 || results[0].State != 60 {
		t.Fatalf("unexpected results: %+v", results)
	}
}

func TestRealNativeOpenRejectsInvalidDirectory(t *testing.T) {
	config := AeronConfig{
		Channel:        "aeron:udp?endpoint=127.0.0.1:20000",
		StreamID:       1,
		AeronDirectory: "/dev/null/epoch-denied-directory",
	}
	assertPanic(t, "aeron open failed", func() {
		NewAeronTransport(config)
	})
}
