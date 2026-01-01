package io.epoch;

import java.util.List;

public interface Transport {
    void send(Engine.Message message);

    List<Engine.Message> poll(int max);

    void close();
}
