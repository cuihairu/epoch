package epoch

import (
	"bufio"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

type expectedResult struct {
	epoch int64
	state int64
	hash  string
}

func TestVectorMatchesExpected(t *testing.T) {
	vectorPath := locateVectorFile(t)
	file, err := os.Open(vectorPath)
	if err != nil {
		t.Fatalf("open vector: %v", err)
	}
	defer file.Close()

	messages := make([]Message, 0)
	expected := make([]expectedResult, 0)

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		parts := strings.Split(line, ",")
		if len(parts) == 0 {
			continue
		}
		if parts[0] == "M" && len(parts) == 7 {
			messages = append(messages, Message{
				Epoch:     parseInt64(t, parts[1]),
				ChannelID: parseInt64(t, parts[2]),
				SourceID:  parseInt64(t, parts[3]),
				SourceSeq: parseInt64(t, parts[4]),
				SchemaID:  parseInt64(t, parts[5]),
				Payload:   parseInt64(t, parts[6]),
			})
		} else if parts[0] == "E" && len(parts) == 4 {
			expected = append(expected, expectedResult{
				epoch: parseInt64(t, parts[1]),
				state: parseInt64(t, parts[2]),
				hash:  parts[3],
			})
		}
	}
	if err := scanner.Err(); err != nil {
		t.Fatalf("scan vector: %v", err)
	}

	results := ProcessMessages(messages)
	if len(results) != len(expected) {
		t.Fatalf("expected %d results, got %d", len(expected), len(results))
	}
	for i := range expected {
		if results[i].Epoch != expected[i].epoch {
			t.Fatalf("epoch mismatch at %d", i)
		}
		if results[i].State != expected[i].state {
			t.Fatalf("state mismatch at %d", i)
		}
		if results[i].Hash != expected[i].hash {
			t.Fatalf("hash mismatch at %d", i)
		}
	}
}

func TestVersion(t *testing.T) {
	if Version() != "0.1.0" {
		t.Fatalf("unexpected version")
	}
}

func TestFnv1a64Hex(t *testing.T) {
	if got := Fnv1a64Hex("state:0"); got != "c3c43df01be7b59c" {
		t.Fatalf("hash mismatch: %s", got)
	}
	if got := Fnv1a64Hex("hello"); got != "a430d84680aabd0b" {
		t.Fatalf("hash mismatch: %s", got)
	}
}

func TestProcessMessagesEmpty(t *testing.T) {
	results := ProcessMessages(nil)
	if len(results) != 0 {
		t.Fatalf("expected empty results")
	}
}

func TestProcessMessagesOrdering(t *testing.T) {
	messages := []Message{
		{Epoch: 2, ChannelID: 2, SourceID: 1, SourceSeq: 2, SchemaID: 100, Payload: 5},
		{Epoch: 1, ChannelID: 1, SourceID: 2, SourceSeq: 1, SchemaID: 100, Payload: 2},
		{Epoch: 1, ChannelID: 1, SourceID: 1, SourceSeq: 2, SchemaID: 100, Payload: -1},
		{Epoch: 3, ChannelID: 1, SourceID: 1, SourceSeq: 1, SchemaID: 100, Payload: 4},
	}
	results := ProcessMessages(messages)
	if len(results) != 3 {
		t.Fatalf("expected 3 results, got %d", len(results))
	}
	if results[0].Epoch != 1 || results[0].State != 1 || results[0].Hash != "c3c43ef01be7b74f" {
		t.Fatalf("epoch 1 mismatch")
	}
	if results[1].Epoch != 2 || results[1].State != 6 || results[1].Hash != "c3c43bf01be7b236" {
		t.Fatalf("epoch 2 mismatch")
	}
	if results[2].Epoch != 3 || results[2].State != 10 || results[2].Hash != "8e2e70ff6abccccd" {
		t.Fatalf("epoch 3 mismatch")
	}
}

func TestCompareMessage(t *testing.T) {
	base := Message{Epoch: 1, ChannelID: 1, SourceID: 1, SourceSeq: 1}
	if compareMessage(base, Message{Epoch: 2}) >= 0 {
		t.Fatalf("expected epoch compare")
	}
	if compareMessage(Message{Epoch: 2}, base) <= 0 {
		t.Fatalf("expected epoch compare reverse")
	}
	if compareMessage(base, Message{Epoch: 1, ChannelID: 2}) >= 0 {
		t.Fatalf("expected channel compare")
	}
	if compareMessage(base, Message{Epoch: 1, ChannelID: 1, SourceID: 2}) >= 0 {
		t.Fatalf("expected source compare")
	}
	if compareMessage(base, Message{Epoch: 1, ChannelID: 1, SourceID: 1, SourceSeq: 2}) >= 0 {
		t.Fatalf("expected seq compare")
	}
	if compareMessage(base, base) != 0 {
		t.Fatalf("expected equality compare")
	}
}

func TestInMemoryTransport(t *testing.T) {
	transport := &InMemoryTransport{}
	transport.Send(Message{Payload: 1})
	transport.Send(Message{Payload: 2})
	transport.Send(Message{Payload: 3})
	if got := transport.Poll(0); len(got) != 0 {
		t.Fatalf("expected empty poll")
	}
	first := transport.Poll(2)
	if len(first) != 2 || first[0].Payload != 1 || first[1].Payload != 2 {
		t.Fatalf("unexpected first poll")
	}
	second := transport.Poll(5)
	if len(second) != 1 || second[0].Payload != 3 {
		t.Fatalf("unexpected second poll")
	}
	transport.Close()
	if got := transport.Poll(1); len(got) != 0 {
		t.Fatalf("expected empty after close")
	}
}

func TestActorIdCodecDefault(t *testing.T) {
	parts := ActorIdParts{
		Region:       1,
		Server:       2,
		ProcessType:  3,
		ProcessIndex: 4,
		ActorIndex:   5,
	}
	value, err := EncodeActorId(parts)
	if err != nil {
		t.Fatalf("encode actor id: %v", err)
	}
	decoded := DecodeActorId(value)
	if decoded != parts {
		t.Fatalf("actor id mismatch")
	}
}

func locateVectorFile(t *testing.T) string {
	current, err := os.Getwd()
	if err != nil {
		t.Fatalf("getwd: %v", err)
	}
	for i := 0; i < 6; i++ {
		candidate := filepath.Join(current, "test-vectors", "epoch_vector_v1.txt")
		if _, err := os.Stat(candidate); err == nil {
			return candidate
		}
		parent := filepath.Dir(current)
		if parent == current {
			break
		}
		current = parent
	}
	t.Fatalf("vector file not found")
	return ""
}

func parseInt64(t *testing.T, value string) int64 {
	v, err := strconv.ParseInt(value, 10, 64)
	if err != nil {
		t.Fatalf("parse int: %v", err)
	}
	return v
}
