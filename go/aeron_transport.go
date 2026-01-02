package epoch

/*
#cgo CFLAGS: -I${SRCDIR}/../native/include
#cgo !windows LDFLAGS: -L${SRCDIR}/../native/build -lepoch_aeron
#cgo windows LDFLAGS: -L${SRCDIR}/../native/build/Release -lepoch_aeron
#include "epoch_aeron.h"
#include <stdlib.h>
*/
import "C"

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"unsafe"
)

type AeronConfig struct {
	Channel          string
	StreamID         int
	AeronDirectory   string
	FragmentLimit    int
	OfferMaxAttempts int
}

type AeronStats struct {
	SentCount         int64
	ReceivedCount     int64
	OfferBackPressure int64
	OfferNotConnected int64
	OfferAdminAction  int64
	OfferClosed       int64
	OfferMaxPosition  int64
	OfferFailed       int64
}

type AeronTransport struct {
	config AeronConfig
	handle aeronHandle
	closed bool
}

const aeronFrameLength = 56
const aeronFrameVersion = 1

type aeronHandle = unsafe.Pointer

var (
	aeronOpen = func(config AeronConfig, errBuf []byte) aeronHandle {
		cChannel := C.CString(config.Channel)
		defer C.free(unsafe.Pointer(cChannel))
		var cDir *C.char
		if config.AeronDirectory != "" {
			cDir = C.CString(config.AeronDirectory)
			defer C.free(unsafe.Pointer(cDir))
		}
		cConfig := C.epoch_aeron_config_t{
			channel:            cChannel,
			stream_id:          C.int32_t(config.StreamID),
			aeron_directory:    cDir,
			fragment_limit:     C.int32_t(config.FragmentLimit),
			offer_max_attempts: C.int32_t(config.OfferMaxAttempts),
		}
		handle := C.epoch_aeron_open(
			&cConfig,
			(*C.char)(unsafe.Pointer(&errBuf[0])),
			C.size_t(len(errBuf)),
		)
		return unsafe.Pointer(handle)
	}
	aeronSend = func(handle aeronHandle, frame []byte, errBuf []byte) int {
		return int(C.epoch_aeron_send(
			(*C.epoch_aeron_transport_t)(handle),
			(*C.uint8_t)(unsafe.Pointer(&frame[0])),
			C.size_t(len(frame)),
			(*C.char)(unsafe.Pointer(&errBuf[0])),
			C.size_t(len(errBuf)),
		))
	}
	aeronPoll = func(handle aeronHandle, frameBuf []byte, max int, errBuf []byte) (int, int) {
		var count C.size_t
		result := C.epoch_aeron_poll(
			(*C.epoch_aeron_transport_t)(handle),
			(*C.uint8_t)(unsafe.Pointer(&frameBuf[0])),
			C.size_t(max),
			&count,
			(*C.char)(unsafe.Pointer(&errBuf[0])),
			C.size_t(len(errBuf)),
		)
		return int(result), int(count)
	}
	aeronStats = func(handle aeronHandle, out *AeronStats) int {
		var stats C.epoch_aeron_stats_t
		if C.epoch_aeron_stats((*C.epoch_aeron_transport_t)(handle), &stats) < 0 {
			return -1
		}
		out.SentCount = int64(stats.sent_count)
		out.ReceivedCount = int64(stats.received_count)
		out.OfferBackPressure = int64(stats.offer_back_pressure)
		out.OfferNotConnected = int64(stats.offer_not_connected)
		out.OfferAdminAction = int64(stats.offer_admin_action)
		out.OfferClosed = int64(stats.offer_closed)
		out.OfferMaxPosition = int64(stats.offer_max_position)
		out.OfferFailed = int64(stats.offer_failed)
		return 0
	}
	aeronClose = func(handle aeronHandle) {
		C.epoch_aeron_close((*C.epoch_aeron_transport_t)(handle))
	}
)

func encodeAeronFrame(message Message) ([]byte, error) {
	buffer := make([]byte, aeronFrameLength)
	buffer[0] = aeronFrameVersion
	buffer[1] = message.Qos
	binary.LittleEndian.PutUint64(buffer[8:16], uint64(message.Epoch))
	binary.LittleEndian.PutUint64(buffer[16:24], uint64(message.ChannelID))
	binary.LittleEndian.PutUint64(buffer[24:32], uint64(message.SourceID))
	binary.LittleEndian.PutUint64(buffer[32:40], uint64(message.SourceSeq))
	binary.LittleEndian.PutUint64(buffer[40:48], uint64(message.SchemaID))
	binary.LittleEndian.PutUint64(buffer[48:56], uint64(message.Payload))
	return buffer, nil
}

func decodeAeronFrame(buffer []byte) (Message, error) {
	if len(buffer) < aeronFrameLength {
		return Message{}, fmt.Errorf("aeron frame too short")
	}
	if buffer[0] != aeronFrameVersion {
		return Message{}, fmt.Errorf("unsupported aeron frame version")
	}
	return Message{
		Epoch:     int64(binary.LittleEndian.Uint64(buffer[8:16])),
		ChannelID: int64(binary.LittleEndian.Uint64(buffer[16:24])),
		SourceID:  int64(binary.LittleEndian.Uint64(buffer[24:32])),
		SourceSeq: int64(binary.LittleEndian.Uint64(buffer[32:40])),
		SchemaID:  int64(binary.LittleEndian.Uint64(buffer[40:48])),
		Qos:       buffer[1],
		Payload:   int64(binary.LittleEndian.Uint64(buffer[48:56])),
	}, nil
}

func NewAeronTransport(config AeronConfig) *AeronTransport {
	errBuf := make([]byte, 256)
	handle := aeronOpen(config, errBuf)
	if handle == nil {
		panic(fmt.Errorf("aeron open failed: %s", trimCString(errBuf)))
	}
	return &AeronTransport{config: config, handle: handle}
}

func (t *AeronTransport) Send(message Message) {
	if t.closed {
		panic(fmt.Errorf("aeron transport closed"))
	}
	frame, err := encodeAeronFrame(message)
	if err != nil {
		panic(err)
	}
	errBuf := make([]byte, 256)
	result := aeronSend(t.handle, frame, errBuf)
	if result < 0 {
		panic(fmt.Errorf("aeron send failed: %s", trimCString(errBuf)))
	}
}

func (t *AeronTransport) Poll(max int) []Message {
	if t.closed || max <= 0 {
		return nil
	}
	frameBuf := make([]byte, max*aeronFrameLength)
	errBuf := make([]byte, 256)
	result, count := aeronPoll(t.handle, frameBuf, max, errBuf)
	if result < 0 {
		panic(fmt.Errorf("aeron poll failed: %s", trimCString(errBuf)))
	}
	out := make([]Message, 0, count)
	for i := 0; i < count; i++ {
		offset := i * aeronFrameLength
		message, err := decodeAeronFrame(frameBuf[offset : offset+aeronFrameLength])
		if err != nil {
			panic(err)
		}
		out = append(out, message)
	}
	return out
}

func (t *AeronTransport) Close() {
	if t.closed {
		return
	}
	t.closed = true
	if t.handle != nil {
		aeronClose(t.handle)
		t.handle = nil
	}
}

func (t *AeronTransport) Stats() AeronStats {
	if t.handle == nil {
		return AeronStats{}
	}
	var stats AeronStats
	if aeronStats(t.handle, &stats) < 0 {
		return AeronStats{}
	}
	return stats
}

func trimCString(buffer []byte) string {
	if idx := bytes.IndexByte(buffer, 0); idx >= 0 {
		return string(buffer[:idx])
	}
	return string(buffer)
}
