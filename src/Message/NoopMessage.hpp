#ifndef NOOP_MESSAGE_HPP
#define NOOP_MESSAGE_HPP

#include "Message.hpp"

class NoopMessage : public Message {
    public:
        NoopMessage(const std::string& sender) : sender(sender), mtype(MessageType::NoopMessage) {}
        ~NoopMessage();
        const MessageType getMessageType() const override;
        const std::string& getSender() const override;
    private:
        const std::string& sender;
        const MessageType mtype;
};

#endif
