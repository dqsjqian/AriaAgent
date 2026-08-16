// AriaAgent — lightweight JSON-Schema validator for tool arguments.
//
// Ported design from deepseek-harness packages/core/tools/src/json-schema.ts:
// validates an argument object against a (subset of) JSON Schema and reports
// path-qualified violations (e.g. "args.a: expected number, got string").
// Supports: type (string/number/integer/boolean/array/object),
// required, enum, properties, items, minLength/maxLength, minimum/maximum.
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace agent {

struct SchemaViolation {
    std::string path;      // "a.b[2]" style
    std::string message;
};

// Validate `value` against `schema`. Returns a list of violations (empty =
// valid). `path` is the prefix used for nested messages (default "args").
std::vector<SchemaViolation> validate_json_schema(
    const nlohmann::json& value,
    const nlohmann::json& schema,
    const std::string& path = "args");

// Human-readable one-line join of violations: "args.a: ..., args.b: ..."
std::string violations_to_string(const std::vector<SchemaViolation>& violations);

} // namespace agent
