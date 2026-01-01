package io.epoch;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

public final class InMemoryTransport implements Transport {
    private final Deque<Engine.Message> queue = new ArrayDeque<>();

    @Override
    public void send(Engine.Message message) {
        queue.addLast(message);
    }

    @Override
    public List<Engine.Message> poll(int max) {
        List<Engine.Message> out = new ArrayList<>();
        while (!queue.isEmpty() && out.size() < max) {
            out.add(queue.removeFirst());
        }
        return out;
    }

    @Override
    public void close() {
        queue.clear();
    }
}
