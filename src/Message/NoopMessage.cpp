#include "NoopMessage.hpp"
#include <thread>

NoopMessage::NoopMessage(const std::thread::id sender) : sender(sender), mtype(MessageType::NoopMessage) {}

NoopMessage::~NoopMessage() {}

const MessageType NoopMessage::getMessageType() const
{
    return this->mtype;
}

const std::thread::id NoopMessage::getSender() const
{
    return this->sender;
}
