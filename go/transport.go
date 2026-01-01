package epoch

// Transport defines the minimal send/poll/close contract.
type Transport interface {
	Send(message Message)
	Poll(max int) []Message
	Close()
}

type InMemoryTransport struct {
	queue []Message
}

func (t *InMemoryTransport) Send(message Message) {
	t.queue = append(t.queue, message)
}

func (t *InMemoryTransport) Poll(max int) []Message {
	if max <= 0 || len(t.queue) == 0 {
		return nil
	}
	if max > len(t.queue) {
		max = len(t.queue)
	}
	out := append([]Message(nil), t.queue[:max]...)
	t.queue = t.queue[max:]
	return out
}

func (t *InMemoryTransport) Close() {
	t.queue = nil
}
