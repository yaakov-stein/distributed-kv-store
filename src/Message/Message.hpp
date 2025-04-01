#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include<string>

enum class MessageType { NoopMessage };

class Message {
    public:
        virtual ~Message() = default;
        virtual const MessageType getMessageType() const = 0;
        virtual const std::string& getSender() const = 0;
};

#endif
