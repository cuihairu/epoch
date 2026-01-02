package epoch

import "fmt"

func Version() string {
	return "0.1.0"
}

type Message struct {
	Epoch     int64
	ChannelID int64
	SourceID  int64
	SourceSeq int64
	SchemaID  int64
	Qos       uint8
	Payload   int64
}

type EpochResult struct {
	Epoch int64
	State int64
	Hash  string
}

func Fnv1a64Hex(input string) string {
	const offsetBasis uint64 = 0xcbf29ce484222325
	const prime uint64 = 0x100000001b3
	var hash uint64 = offsetBasis
	for i := 0; i < len(input); i++ {
		hash ^= uint64(input[i])
		hash *= prime
	}
	return fmt.Sprintf("%016x", hash)
}

func ProcessMessages(messages []Message) []EpochResult {
	sortMessages(messages)
	results := make([]EpochResult, 0)
	var state int64
	var currentEpoch int64
	hasEpoch := false

	for _, message := range messages {
		if !hasEpoch {
			currentEpoch = message.Epoch
			hasEpoch = true
		}
		if message.Epoch != currentEpoch {
			results = append(results, EpochResult{
				Epoch: currentEpoch,
				State: state,
				Hash:  Fnv1a64Hex(fmt.Sprintf("state:%d", state)),
			})
			currentEpoch = message.Epoch
		}
		state += message.Payload
	}
	if hasEpoch {
		results = append(results, EpochResult{
			Epoch: currentEpoch,
			State: state,
			Hash:  Fnv1a64Hex(fmt.Sprintf("state:%d", state)),
		})
	}
	return results
}

func sortMessages(messages []Message) {
	for i := 1; i < len(messages); i++ {
		j := i
		for j > 0 && compareMessage(messages[j-1], messages[j]) > 0 {
			messages[j-1], messages[j] = messages[j], messages[j-1]
			j--
		}
	}
}

func compareMessage(a, b Message) int {
	if a.Epoch != b.Epoch {
		if a.Epoch < b.Epoch {
			return -1
		}
		return 1
	}
	if a.ChannelID != b.ChannelID {
		if a.ChannelID < b.ChannelID {
			return -1
		}
		return 1
	}
	if a.Qos != b.Qos {
		if a.Qos > b.Qos {
			return -1
		}
		return 1
	}
	if a.SourceID != b.SourceID {
		if a.SourceID < b.SourceID {
			return -1
		}
		return 1
	}
	if a.SourceSeq != b.SourceSeq {
		if a.SourceSeq < b.SourceSeq {
			return -1
		}
		return 1
	}
	return 0
}
