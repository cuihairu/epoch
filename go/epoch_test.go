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
