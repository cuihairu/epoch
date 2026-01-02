package epoch

import "testing"

func TestAeronFrameRoundTrip(t *testing.T) {
	input := Message{
		Epoch:     7,
		ChannelID: 8,
		SourceID:  9,
		SourceSeq: 10,
		SchemaID:  11,
		Qos:       2,
		Payload:   -42,
	}
	frame, err := encodeAeronFrame(input)
	if err != nil {
		t.Fatalf("encode failed: %v", err)
	}
	output, err := decodeAeronFrame(frame)
	if err != nil {
		t.Fatalf("decode failed: %v", err)
	}
	if output != input {
		t.Fatalf("round trip mismatch: %+v vs %+v", output, input)
	}
}

func TestAeronFrameRejectsShort(t *testing.T) {
	_, err := decodeAeronFrame([]byte{1, 2})
	if err == nil {
		t.Fatalf("expected error on short frame")
	}
}
