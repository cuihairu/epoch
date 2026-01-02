package epoch

import "testing"

func TestDefaultActorIdCodecRejectsOutOfRange(t *testing.T) {
	_, err := EncodeActorId(ActorIdParts{
		Region:       uint16(regionMax + 1),
		Server:       1,
		ProcessType:  1,
		ProcessIndex: 1,
		ActorIndex:   1,
	})
	if err == nil {
		t.Fatalf("expected region error")
	}
	_, err = EncodeActorId(ActorIdParts{
		Region:       1,
		Server:       uint16(serverMax + 1),
		ProcessType:  1,
		ProcessIndex: 1,
		ActorIndex:   1,
	})
	if err == nil {
		t.Fatalf("expected server error")
	}
	_, err = EncodeActorId(ActorIdParts{
		Region:       1,
		Server:       1,
		ProcessType:  uint8(processTypeMax + 1),
		ProcessIndex: 1,
		ActorIndex:   1,
	})
	if err == nil {
		t.Fatalf("expected process type error")
	}
	_, err = EncodeActorId(ActorIdParts{
		Region:       1,
		Server:       1,
		ProcessType:  1,
		ProcessIndex: uint16(processIndexMax + 1),
		ActorIndex:   1,
	})
	if err == nil {
		t.Fatalf("expected process index error")
	}
	_, err = EncodeActorId(ActorIdParts{
		Region:       1,
		Server:       1,
		ProcessType:  1,
		ProcessIndex: 1,
		ActorIndex:   uint32(actorIndexMax + 1),
	})
	if err == nil {
		t.Fatalf("expected actor index error")
	}
}

func TestActorIdCodecName(t *testing.T) {
	if DefaultActorIdCodecInstance().Name() != "default" {
		t.Fatalf("unexpected codec name")
	}
}

func TestAeronFrameRejectsVersion(t *testing.T) {
	buffer := make([]byte, aeronFrameLength)
	buffer[0] = 2
	if _, err := decodeAeronFrame(buffer); err == nil {
		t.Fatalf("expected version error")
	}
}

func TestTrimCString(t *testing.T) {
	if got := trimCString([]byte{'a', 'b', 'c', 0, 'd'}); got != "abc" {
		t.Fatalf("unexpected trim")
	}
	if got := trimCString([]byte{'x', 'y', 'z'}); got != "xyz" {
		t.Fatalf("unexpected trim without null")
	}
}
