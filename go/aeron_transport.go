package epoch

import "fmt"

type AeronConfig struct {
	Channel        string
	StreamID       int
	AeronDirectory string
}

type AeronTransport struct {
	config AeronConfig
}

func NewAeronTransport(config AeronConfig) *AeronTransport {
	return &AeronTransport{config: config}
}

func (t *AeronTransport) Send(message Message) {
	panic(fmt.Errorf("Aeron transport not linked"))
}

func (t *AeronTransport) Poll(max int) []Message {
	panic(fmt.Errorf("Aeron transport not linked"))
}

func (t *AeronTransport) Close() {
}
