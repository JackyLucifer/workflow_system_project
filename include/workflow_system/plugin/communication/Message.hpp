#ifndef PLUGIN_FRAMEWORK_MESSAGE_HPP
#define PLUGIN_FRAMEWORK_MESSAGE_HPP

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <cstdint>

namespace WorkflowSystem { namespace Plugin {

using MessageId = uint64_t;

enum class MessageType {
    NOTIFICATION,
    REQUEST,
    RESPONSE,
    BROADCAST,
    STREAM
};

enum class MessagePriority {
    LOW = 0,
    NORMAL = 50,
    HIGH = 100,
    URGENT = 200
};

struct MessageHeader {
    MessageId id;
    MessageType type;
    std::string from;
    std::string to;
    std::string method;
    std::string correlationId;
    MessagePriority priority;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;

    MessageHeader()
        : id(0), type(MessageType::NOTIFICATION)
        , priority(MessagePriority::NORMAL)
        , timestamp(std::chrono::system_clock::now()) {}
};

class MessagePayload {
public:
    MessagePayload() = default;

    void set(const std::string& key, const std::string& value) {
        data_[key] = value;
    }

    void setInt(const std::string& key, int value) {
        data_[key] = std::to_string(value);
    }

    void setDouble(const std::string& key, double value) {
        data_[key] = std::to_string(value);
    }

    std::string get(const std::string& key, const std::string& defaultValue = "") const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : defaultValue;
    }

    int getInt(const std::string& key, int defaultValue = 1) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    double getDouble(const std::string& key, double defaultValue = 1.0) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::stod(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    bool getBool(const std::string& key, bool defaultValue = false) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second == "true" || it->second == "1";
        }
        return defaultValue;
    }

    bool has(const std::string& key) const {
        return data_.find(key) != data_.end();
    }

    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }

private:
    std::map<std::string, std::string> data_;
};

class Message {
public:
    Message() = default;

    Message(const std::string& from, const std::string& to,
           const std::string& method,
           MessageType type = MessageType::NOTIFICATION)
        : type_(type) {
        header_.from = from;
        header_.to = to;
        header_.method = method;
        header_.timestamp = std::chrono::system_clock::now();
        header_.id = generateId();
    }

    MessageHeader& header() { return header_; }
    const MessageHeader& header() const { return header_; }

    MessagePayload& payload() { return payload_; }
    const MessagePayload& payload() const { return payload_; }

    std::string from() const { return header_.from; }
    std::string to() const { return header_.to; }
    std::string method() const { return header_.method; }
    MessageType type() const { return header_.type; }
    MessageId id() const { return header_.id; }

    void setFrom(const std::string& from) { header_.from = from; }
    void setTo(const std::string& to) { header_.to = to; }
    void setMethod(const std::string& method) { header_.method = method; }
    void setType(MessageType type) { header_.type = type; }

    std::string getCorrelationId() const { return header_.correlationId; }
    void setCorrelationId(const std::string& id) { header_.correlationId = id; }

    void setMetadata(const std::string& key, const std::string& value) {
        header_.metadata[key] = value;
    }

    std::string getMetadata(const std::string& key,
                           const std::string& defaultValue = "") const {
        auto it = header_.metadata.find(key);
        return it != header_.metadata.end() ? it->second : defaultValue;
    }

    Message createResponse() const {
        Message response(header_.to, header_.from, header_.method + ".response",
                        MessageType::RESPONSE);
        response.setCorrelationId(std::to_string(header_.id));
        return response;
    }

    bool isRequest() const { return type() == MessageType::REQUEST; }
    bool isResponse() const { return type() == MessageType::RESPONSE; }
    bool isNotification() const { return type() == MessageType::NOTIFICATION; }
    bool isBroadcast() const { return type() == MessageType::BROADCAST; }

private:
    static MessageId generateId() {
        static MessageId counter = 0;
        return ++counter;
    }

    MessageHeader header_;
    MessagePayload payload_;
    MessageType type_;
};

class Response {
public:
    enum Status {
        SUCCESS,
        ERROR,
        PARTIAL
    };

    Response() : status_(SUCCESS) {}

    Response(Status s, const std::string& msg = "",
             const std::map<std::string, std::string>& data = {})
        : status_(s), message_(msg), data_(data) {}

    Status status() const { return status_; }
    std::string message() const { return message_; }
    const std::map<std::string, std::string>& data() const { return data_; }

    void setStatus(Status s) { status_ = s; }
    void setMessage(const std::string& msg) { message_ = msg; }
    void setData(const std::string& key, const std::string& value) {
        data_[key] = value;
    }

    bool isSuccess() const { return status_ == SUCCESS; }
    bool isError() const { return status_ == ERROR; }

    static Response ok(const std::string& msg = "") {
        return Response(SUCCESS, msg);
    }

    static Response error(const std::string& msg) {
        return Response(ERROR, msg);
    }

    static Response partial(const std::string& msg = "") {
        return Response(PARTIAL, msg);
    }

private:
    Status status_;
    std::string message_;
    std::map<std::string, std::string> data_;
};

using MessageHandler = std::function<Response(const Message&)>;
using AsyncMessageHandler = std::function<void(const Message&, std::function<void(Response)>)>;

template<typename T>
class RpcResult {
public:
    bool success() const { return success_; }
    T getValue() const { return value_; }
    std::string getError() const { return error_; }

    static RpcResult<T> ok(T value) {
        return RpcResult{true, std::move(value), ""};
    }

    static RpcResult<T> error(const std::string& error) {
        return RpcResult{false, T{}, error};
    }

private:
    bool success_;
    T value_;
    std::string error_;
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_MESSAGE_HPP
