package epoch

import "fmt"

type ActorIdParts struct {
	Region       uint16
	Server       uint16
	ProcessType  uint8
	ProcessIndex uint16
	ActorIndex   uint32
}

type ActorIdCodec interface {
	Encode(parts ActorIdParts) (uint64, error)
	Decode(value uint64) ActorIdParts
	Name() string
}

type DefaultActorIdCodec struct{}

const (
	regionBits       = 10
	serverBits       = 12
	processTypeBits  = 6
	processIndexBits = 10
	actorIndexBits   = 26

	regionMax       = (1 << regionBits) - 1
	serverMax       = (1 << serverBits) - 1
	processTypeMax  = (1 << processTypeBits) - 1
	processIndexMax = (1 << processIndexBits) - 1
	actorIndexMax   = (1 << actorIndexBits) - 1

	actorIndexShift   = 0
	processIndexShift = actorIndexShift + actorIndexBits
	processTypeShift  = processIndexShift + processIndexBits
	serverShift       = processTypeShift + processTypeBits
	regionShift       = serverShift + serverBits
)

func (DefaultActorIdCodec) Encode(parts ActorIdParts) (uint64, error) {
	if int(parts.Region) > regionMax ||
		int(parts.Server) > serverMax ||
		int(parts.ProcessType) > processTypeMax ||
		int(parts.ProcessIndex) > processIndexMax ||
		int(parts.ActorIndex) > actorIndexMax {
		return 0, fmt.Errorf("actor id parts out of range")
	}

	value := (uint64(parts.Region) << regionShift) |
		(uint64(parts.Server) << serverShift) |
		(uint64(parts.ProcessType) << processTypeShift) |
		(uint64(parts.ProcessIndex) << processIndexShift) |
		(uint64(parts.ActorIndex) << actorIndexShift)

	return value, nil
}

func (DefaultActorIdCodec) Decode(value uint64) ActorIdParts {
	return ActorIdParts{
		Region:       uint16((value >> regionShift) & regionMax),
		Server:       uint16((value >> serverShift) & serverMax),
		ProcessType:  uint8((value >> processTypeShift) & processTypeMax),
		ProcessIndex: uint16((value >> processIndexShift) & processIndexMax),
		ActorIndex:   uint32((value >> actorIndexShift) & actorIndexMax),
	}
}

func (DefaultActorIdCodec) Name() string {
	return "default"
}

var defaultActorIdCodec ActorIdCodec = DefaultActorIdCodec{}

func EncodeActorId(parts ActorIdParts) (uint64, error) {
	return defaultActorIdCodec.Encode(parts)
}

func DecodeActorId(value uint64) ActorIdParts {
	return defaultActorIdCodec.Decode(value)
}

func DefaultActorIdCodecInstance() ActorIdCodec {
	return defaultActorIdCodec
}
