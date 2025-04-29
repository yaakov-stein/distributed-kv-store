#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <thread>

enum class MessageType { NoopMessage };

class Message {
    public:
        virtual ~Message() = default;
        virtual const MessageType getMessageType() const = 0;
        virtual const std::thread::id getSender() const = 0;
};

#endif
