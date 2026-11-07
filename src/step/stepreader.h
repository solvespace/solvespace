//-----------------------------------------------------------------------------
// Experimental STEP dependency proof, not yet a geometry importer.
// No STEPcode headers, types or generated bindings cross this boundary.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_STEP_READER_H
#define SOLVESPACE_STEP_READER_H

#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace SolveSpace {
namespace Step {

struct BindingInfo {
    std::string protocol;
    std::string schema;
};

std::vector<BindingInfo> SupportedBindings();

// An owned, read-only value snapshot; references use Part 21 instance IDs.
// Opaque values are explicitly not decoded by this proof's adapter. String
// text retains STEP quoting/escapes; this is not yet a UI string decoder.
struct Value {
    enum class Kind { Null, Derived, Integer, Real, String, Enumeration,
                      Reference, Aggregate, Opaque };
    Kind kind = Kind::Null;
    std::int64_t integer = 0;
    double real = 0.0;
    int reference = 0;
    std::string text;
    std::string selectedType;
    std::vector<Value> elements;
};

struct ReadResult;

// This proof-only adapter is single-threaded, including queries/destruction.
// STEPcode still has global descriptor state and console diagnostics. Do not
// wire this API into the GUI or concurrent background imports as it stands.
class Document {
public:
    ~Document();
    const std::string &Protocol() const;
    const std::string &DeclaredSchema() const;
    size_t EntityCount() const;
    size_t EntityTypeCount() const;
    std::vector<int> EntitiesOfType(const std::string &type) const;
    std::vector<std::string> EntityTypes(int id) const;
    bool IsA(int id, const std::string &type) const;
    // component optionally selects a part of a complex instance. False means
    // the entity/attribute is missing, ambiguous, or cannot be safely decoded.
    bool Attribute(int id, const std::string &name, Value *value,
                   const std::string &component = "") const;

    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
    Document();
    friend ReadResult Read(std::istream &input);
};

struct ReadResult {
    enum class Status { Okay, InvalidFile, UnsupportedSchema };
    Status status = Status::InvalidFile;
    std::unique_ptr<Document> document;
    std::string error;
    std::string readerLog;
};

// Consumes one seekable, uncompressed Part 21 stream using the classic numeric
// locale. No filename is handed to STEPcode and the caller retains the stream.
// Parsing errors reject the whole document; this is not a partial-solid policy.
ReadResult Read(std::istream &input);

} // namespace Step
} // namespace SolveSpace

#endif
