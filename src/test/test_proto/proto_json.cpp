#include <google/protobuf/util/json_util.h>

#include <fstream>
#include <iostream>
#include <sstream>

using google::protobuf::util::JsonStringToMessage;

namespace kim {
class ProtoJson {
    ProtoJson() {}
    virtual ~ProtoJson() {}

    bool proto_to_json(const google::protobuf::Message& message, std::string& json) {
        google::protobuf::util::JsonPrintOptions options;
        options.add_whitespace = true;
        options.always_print_primitive_fields = true;
        options.preserve_proto_field_names = true;
        return MessageToJsonString(message, &json, options).ok();
    }

    bool json_file_to_proto(const std::string& file, google::protobuf::Message& message) {
        std::ifstream is(file);
        if (!is.good()) {
            return false;
        }
        std::stringstream json;
        json << is.rdbuf();
        is.close();
        return JsonStringToMessage(json.str(), &message).ok();
    }

    bool json_to_proto(const std::string& json, google::protobuf::Message& message) {
        return JsonStringToMessage(json, &message).ok();
    }
};

}  // namespace kim