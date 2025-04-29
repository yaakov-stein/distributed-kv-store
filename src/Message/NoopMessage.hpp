#ifndef NOOP_MESSAGE_HPP
#define NOOP_MESSAGE_HPP

#include "Message.hpp"
#include <thread>

class NoopMessage : public Message {
    public:
        NoopMessage(const std::thread::id sender);
        ~NoopMessage();
        const MessageType getMessageType() const override;
        const std::thread::id getSender() const override;
    private:
        const std::thread::id sender;
        const MessageType mtype;
};

#endif
