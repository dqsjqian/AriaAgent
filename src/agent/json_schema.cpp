// AriaAgent — JSON-Schema validator implementation.
#include "agent/json_schema.hpp"

namespace agent {

using json = nlohmann::json;

namespace {

void validate_impl(const json& value, const json& schema,
                   const std::string& path, std::vector<SchemaViolation>& out) {
    // type check
    if (schema.contains("type")) {
        const std::string want = schema["type"].get<std::string>();
        bool ok = false;
        if (want == "string")      ok = value.is_string();
        else if (want == "number") ok = value.is_number();
        else if (want == "integer") ok = value.is_number_integer();
        else if (want == "boolean") ok = value.is_boolean();
        else if (want == "array")   ok = value.is_array();
        else if (want == "object")  ok = value.is_object();
        else if (want == "null")    ok = value.is_null();
        if (!ok) {
            out.push_back({path, "expected " + want + ", got " + value.type_name()});
            return;   // no point checking constraints on wrong type
        }
    }

    // string constraints
    if (value.is_string()) {
        const std::string s = value.get<std::string>();
        if (schema.contains("minLength") && static_cast<int>(s.size()) < schema["minLength"].get<int>())
            out.push_back({path, "shorter than minLength " + std::to_string(schema["minLength"].get<int>())});
        if (schema.contains("maxLength") && static_cast<int>(s.size()) > schema["maxLength"].get<int>())
            out.push_back({path, "longer than maxLength " + std::to_string(schema["maxLength"].get<int>())});
        if (schema.contains("enum")) {
            bool found = false;
            for (const auto& e : schema["enum"]) if (e.is_string() && e.get<std::string>() == s) { found = true; break; }
            if (!found) out.push_back({path, "not one of the allowed values"});
        }
    }

    // number constraints
    if (value.is_number()) {
        const double n = value.get<double>();
        if (schema.contains("minimum") && n < schema["minimum"].get<double>())
            out.push_back({path, "less than minimum " + std::to_string(schema["minimum"].get<double>())});
        if (schema.contains("maximum") && n > schema["maximum"].get<double>())
            out.push_back({path, "greater than maximum " + std::to_string(schema["maximum"].get<double>())});
    }

    // object: required + properties
    if (value.is_object()) {
        if (schema.contains("required")) {
            for (const auto& req : schema["required"]) {
                const std::string key = req.get<std::string>();
                if (!value.contains(key))
                    out.push_back({path + "." + key, "missing required property"});
            }
        }
        if (schema.contains("properties")) {
            for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it) {
                const std::string key = it.key();
                if (value.contains(key)) {
                    validate_impl(value[key], it.value(), path + "." + key, out);
                }
            }
        }
        if (schema.contains("additionalProperties") && schema["additionalProperties"].is_boolean()
            && !schema["additionalProperties"].get<bool>()) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                if (!schema.contains("properties") || !schema["properties"].contains(it.key()))
                    out.push_back({path + "." + it.key(), "unknown property"});
            }
        }
    }

    // array: items
    if (value.is_array()) {
        if (schema.contains("items") && !schema["items"].is_array()) {
            for (size_t i = 0; i < value.size(); ++i)
                validate_impl(value[i], schema["items"], path + "[" + std::to_string(i) + "]", out);
        }
        if (schema.contains("minItems") && static_cast<int>(value.size()) < schema["minItems"].get<int>())
            out.push_back({path, "fewer than minItems " + std::to_string(schema["minItems"].get<int>())});
        if (schema.contains("maxItems") && static_cast<int>(value.size()) > schema["maxItems"].get<int>())
            out.push_back({path, "more than maxItems " + std::to_string(schema["maxItems"].get<int>())});
    }
}

} // namespace

std::vector<SchemaViolation> validate_json_schema(
    const json& value, const json& schema, const std::string& path) {
    std::vector<SchemaViolation> out;
    if (schema.is_object()) validate_impl(value, schema, path, out);
    return out;
}

std::string violations_to_string(const std::vector<SchemaViolation>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += "; ";
        s += v[i].path + ": " + v[i].message;
    }
    return s;
}

} // namespace agent
