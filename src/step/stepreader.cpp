//-----------------------------------------------------------------------------
// STEPcode is private to this translation unit and the generated libraries.
// This reads schema-backed data only; it does not create SolveSpace geometry.
//-----------------------------------------------------------------------------
#include "stepreader.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <locale>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "clstepcore/Registry.h"
#include "clstepcore/STEPattribute.h"
#include "clstepcore/STEPaggregate.h"
#include "clstepcore/STEPcomplex.h"
#include "clstepcore/ExpDict.h"
#include "cleditor/STEPfile.h"
#include "cleditor/SdaiHeaderSchema.h"
#include "slvs_stepcode_diagnostics.h"

#ifdef SLVS_STEP_AP203
extern void SlvsStepAP203SchemaInit(Registry &);
#endif
#ifdef SLVS_STEP_AP214
extern void SlvsStepAP214SchemaInit(Registry &);
#endif
#ifdef SLVS_STEP_AP242
extern void SlvsStepAP242SchemaInit(Registry &);
#endif

namespace SolveSpace {
namespace Step {
namespace {

struct Binding {
    const char *protocol;
    const char *schema;
    void (*initialize)(Registry &);
};

const Binding bindings[] = {
#ifdef SLVS_STEP_AP203
    { "AP203", "CONFIG_CONTROL_DESIGN", SlvsStepAP203SchemaInit },
#endif
#ifdef SLVS_STEP_AP214
    { "AP214", "AUTOMOTIVE_DESIGN", SlvsStepAP214SchemaInit },
#endif
#ifdef SLVS_STEP_AP242
    { "AP242", "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF", SlvsStepAP242SchemaInit },
#endif
};

std::string Upper(std::string text) {
    for(char &c : text) {
        if(c >= 'a' && c <= 'z') c += 'A' - 'a';
    }
    return text;
}

std::string SimpleName(std::string name) {
    size_t dot = name.find_last_of('.');
    if(dot != std::string::npos) name = name.substr(dot + 1);
    return Upper(name);
}

// FILE_SCHEMA may append the standardized object identifier in braces. It
// identifies a schema revision, not another generated namespace. Preserve it
// in DeclaredSchema(); matching the base name is not a promise of all-edition
// compatibility. Different attribute layouts still fail the strict data read.
std::string SchemaName(const std::string &identifier) {
    size_t first = identifier.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return "";
    size_t end = identifier.find_first_of(" \t\r\n{", first);
    std::string name = Upper(identifier.substr(first, end - first));
    size_t suffix = identifier.find_first_not_of(" \t\r\n", end);
    if(suffix != std::string::npos) {
        size_t close = identifier.find('}', suffix);
        if(identifier[suffix] != '{' || close == std::string::npos ||
           identifier.find_first_not_of(" \t\r\n", close + 1) != std::string::npos) {
            return "";
        }
        if(identifier.find_first_not_of("0123456789 \t\r\n", suffix + 1) != close) {
            return "";
        }
    }
    return name;
}

void EmptyRegistry(Registry &) {}

SDAI_Application_instance *NextPart(SDAI_Application_instance *part) {
    auto complex = dynamic_cast<STEPcomplex *>(part);
    return complex ? complex->sc : nullptr;
}

// These protected reader operations already implement the Part 21 parser.
// Orchestrate the two passes on the same seekable stream: AppendFile() would
// reopen a narrow-string filename for pass two, even for stream input.
class Reader : public STEPfile {
public:
    Reader(Registry &registry, InstMgr &instances) : STEPfile(registry, instances, "", true) {
        _verbose = false;
        _maxErrorCount = 32;
    }
    ~Reader() override {
        // Upstream deletes its header registry before header instances. Empty
        // the latter now, while all their attribute descriptors are still live.
        HeaderInstances()->DeleteInstances();
    }

    bool Keyword(std::istream &input, const char *expected) {
        ReadTokenSeparator(input);
        // GetKeyword assumes its first get() succeeds (otherwise its local
        // character is uninitialized). Do not call it at a truncated EOF.
        if(!input.good() || input.peek() == std::char_traits<char>::eof()) return false;
        std::string keyword = GetKeyword(input, ";", Error());
        char delimiter = 0;
        input.get(delimiter);
        return keyword == expected && delimiter == ';';
    }

    bool Header(std::istream &input, std::string *identifier) {
        if(!Keyword(input, "ISO-10303-21")) return false;
        auto position = input.tellg();
        if(!Keyword(input, "HEADER")) return false;
        input.seekg(position);
        if(ReadHeader(input) < SEVERITY_USERMSG) return false;
        for(const char *type : { "File_Description", "File_Name", "File_Schema" }) {
            if(HeaderInstances()->EntityKeywordCount(type) != 1) return false;
        }
        auto header = dynamic_cast<SdaiFile_schema *>(
            HeaderInstances()->GetApplication_instance("File_Schema"));
        if(!header || header->schema_identifiers_()->EntryCount() != 1) return false;
        auto name = dynamic_cast<StringNode *>(header->schema_identifiers_()->GetHead());
        if(!name) return false;
        *identifier = schemaName();
        return !identifier->empty();
    }

    bool Data(std::istream &input) {
        if(!Keyword(input, "DATA")) return Fail("Expected one unparameterized DATA section.");
        auto start = input.tellg();
        if(start == std::streampos(-1)) return Fail("Cannot seek STEP data stream.");
        int count = ReadData1(input);
        if(_entsNotCreated || ErrorCount() || Error().severity() < SEVERITY_USERMSG ||
           !input.good()) return Fail("Failed to create every instance in the first read pass.");
        if(!Trailer(input)) return Fail("Missing end marker or unexpected data after the DATA section.");

        std::set<int> ids;
        for(int i = 0; i < instances().InstanceCount(); ++i) {
            int id = instances().GetApplication_instance(i)->StepFileId();
            if(id <= 0 || !ids.insert(id).second) {
                Error().AppendToUserMsg("Duplicate or non-positive Part 21 instance ID.\n");
                return false;
            }
        }

        input.clear();
        input.seekg(start);
        int valid = ReadData2(input);
        if(valid != count || _entsInvalid || ErrorCount() ||
           Error().severity() < SEVERITY_USERMSG || !input.good()) {
            return Fail("Invalid instance values in the second read pass.");
        }
        if(!Trailer(input)) return Fail("Second read pass did not reach the expected end marker.");
        // STEPcomplex does not propagate every component's read status to its
        // head. Inspect each part and attribute, not just the file's summary.
        for(int i = 0; i < instances().InstanceCount(); ++i) {
            for(auto part = instances().GetApplication_instance(i); part; part = NextPart(part)) {
                if(part->Error().severity() < SEVERITY_USERMSG) {
                    return Fail("Invalid complex component in instance #" +
                        std::to_string(part->StepFileId()) + ": " + part->EntityName() +
                        "\n" + part->Error().DetailMsg());
                }
                part->ResetAttributes();
                while(auto attribute = part->NextAttribute()) {
                    // In a complex record, EXPRESS redeclarations have no
                    // separate serialized value (technical corrigendum). The
                    // base component carries the value instead.
                    if(attribute->getADesc()->AttrType() == AttrType_Redefining) continue;
                    if(attribute->Error().severity() < SEVERITY_USERMSG ||
                       (!attribute->IsDerived() && !attribute->Nullable() && attribute->is_null())) {
                        Error().AppendToUserMsg("Invalid attribute in instance #" +
                            std::to_string(part->StepFileId()) + ": " + attribute->Name() + "\n");
                        return false;
                    }
                }
            }
        }
        return true;
    }

private:
    bool Fail(const std::string &message) {
        Error().AppendToUserMsg(message + "\n");
        return false;
    }
    bool Trailer(std::istream &input) {
        if(!Keyword(input, "END-ISO-10303-21")) return false;
        ReadTokenSeparator(input);
        return input.peek() == std::char_traits<char>::eof() && !input.bad();
    }
};

// All calls into the pinned runtime, including queries and destruction, are
// serialized. In particular, its header descriptors and token helpers aren't
// thread-safe. No vendor objects are retained by the linked geometry cache.
std::recursive_mutex &RuntimeMutex() {
    static std::recursive_mutex mutex;
    return mutex;
}
using RuntimeLock = std::lock_guard<std::recursive_mutex>;

class LogBuffer : public std::streambuf {
public:
    std::string text;
private:
    int_type overflow(int_type c) override {
        if(!traits_type::eq_int_type(c, traits_type::eof()) && text.size() < 65536) {
            text.push_back(traits_type::to_char_type(c));
        }
        return traits_type::not_eof(c);
    }
};

// The build-tree runtime overlay uses only this thread's diagnostic sink.
// Restore the caller's stream locale on every exit path; never use locale::global.
class ReadScope {
public:
    explicit ReadScope(std::istream &input)
        : log(&buffer), input(input), locale(input.getloc()) {
        input.imbue(std::locale::classic());
        previous = SlvsStepCode::SetLog(&log);
    }
    ~ReadScope() {
        SlvsStepCode::SetLog(previous);
        input.imbue(locale);
    }
    LogBuffer buffer;
    std::ostream log;
private:
    std::istream &input;
    std::locale locale;
    std::ostream *previous;
};

bool DecodeAggregate(STEPaggregate *, const TypeDescriptor *, InstMgr &, Value *, unsigned);

bool DecodeSelect(SDAI_Select *select, InstMgr &instances, Value *value, unsigned depth) {
    if(!select || depth > 32) return false;
    if(select->is_null()) return true;
    const TypeDescriptor *type = select->CurrentUnderlyingType();
    if(auto integer = select->IntegerValue()) {
        value->kind = Value::Kind::Integer;
        value->integer = *integer;
    } else if(auto real = select->RealValue()) {
        if(!std::isfinite(*real)) return false;
        value->kind = Value::Kind::Real;
        value->real = *real;
    } else if(auto string = select->StringValue()) {
        value->kind = Value::Kind::String;
        value->text = string->c_str();
    } else if(auto entity = select->EntityValue()) {
        if(isNilSTEPentity(entity)) return false;
        value->kind = Value::Kind::Reference;
        value->reference = entity->StepFileId();
    } else if(auto enumeration = select->EnumValue()) {
        value->kind = Value::Kind::Enumeration;
        enumeration->asStr(value->text);
    } else if(auto aggregate = select->AggregateValue()) {
        if(!type || !DecodeAggregate(aggregate, type->AggrElemTypeDescriptor(),
                                    instances, value, depth + 1)) return false;
    } else if(auto nested = select->SelectValue()) {
        if(!DecodeSelect(nested, instances, value, depth + 1)) return false;
    } else {
        value->kind = Value::Kind::Opaque;
        select->STEPwrite(value->text);
    }
    if(type && type->Name()) value->selectedType = Upper(type->Name());
    return true;
}

bool DecodeAggregate(STEPaggregate *aggregate, const TypeDescriptor *elementType,
                     InstMgr &instances, Value *value, unsigned depth) {
    if(!aggregate || !elementType || depth > 32) return false;
    if(aggregate->is_null()) return true;
    value->kind = Value::Kind::Aggregate;
    for(auto node = aggregate->GetHead(); node; node = node->NextNode()) {
        Value item;
        if(auto real = dynamic_cast<RealNode *>(node)) {
            if(real->is_null() || !std::isfinite(real->value)) return false;
            item.kind = Value::Kind::Real;
            item.real = real->value;
        } else if(auto integer = dynamic_cast<IntNode *>(node)) {
            if(integer->is_null()) return false;
            item.kind = Value::Kind::Integer;
            item.integer = integer->value;
        } else if(auto entity = dynamic_cast<EntityNode *>(node)) {
            if(isNilSTEPentity(entity->node)) return false;
            item.kind = Value::Kind::Reference;
            item.reference = entity->node->StepFileId();
        } else if(auto string = dynamic_cast<StringNode *>(node)) {
            item.kind = Value::Kind::String;
            item.text = string->value.c_str();
        } else if(auto enumeration = dynamic_cast<EnumNode *>(node)) {
            if(!enumeration->node || enumeration->node->is_null()) return false;
            item.kind = Value::Kind::Enumeration;
            enumeration->node->asStr(item.text);
        } else if(auto select = dynamic_cast<SelectNode *>(node)) {
            if(!DecodeSelect(select->node, instances, &item, depth + 1)) return false;
        } else if(auto generic = dynamic_cast<GenericAggrNode *>(node)) {
            // Nested aggregates are stored as generic tokens by STEPcode. Ask
            // STEPcode to read those tokens with the inner aggregate descriptor;
            // do not introduce a second STEP syntax parser in the importer.
            auto descriptor = dynamic_cast<const AggrTypeDescriptor *>(
                elementType->NonRefTypeDescriptor());
            if(!descriptor) {
                item.kind = Value::Kind::Opaque;
                generic->value.asStr(item.text);
            } else {
                std::string text;
                generic->value.asStr(text);
                std::istringstream input(text);
                input.imbue(std::locale::classic());
                ErrorDescriptor error;
                std::unique_ptr<STEPaggregate> nested(descriptor->CreateAggregate());
                if(!nested || nested->STEPread(input, &error,
                    descriptor->AggrElemTypeDescriptor(), &instances) < SEVERITY_USERMSG ||
                   !DecodeAggregate(nested.get(), descriptor->AggrElemTypeDescriptor(),
                                    instances, &item, depth + 1)) return false;
            }
        } else {
            return false;
        }
        value->elements.push_back(std::move(item));
    }
    return true;
}

bool DecodeAttribute(STEPattribute *attribute, InstMgr &instances, Value *value) {
    if(attribute->IsDerived()) {
        value->kind = Value::Kind::Derived;
    } else if(attribute->is_null()) {
        value->kind = Value::Kind::Null;
    } else if(auto integer = attribute->Integer()) {
        value->kind = Value::Kind::Integer;
        value->integer = *integer;
    } else if(auto real = attribute->Real() ? attribute->Real() : attribute->Number()) {
        if(!std::isfinite(*real)) return false;
        value->kind = Value::Kind::Real;
        value->real = *real;
    } else if(auto string = attribute->String()) {
        value->kind = Value::Kind::String;
        value->text = string->c_str();
    } else if(auto entity = attribute->Entity()) {
        if(isNilSTEPentity(entity)) return false;
        value->kind = Value::Kind::Reference;
        value->reference = entity->StepFileId();
    } else if(auto select = attribute->Select()) {
        return DecodeSelect(select, instances, value, 0);
    } else if(auto aggregate = attribute->Aggregate()) {
        return DecodeAggregate(aggregate, attribute->getADesc()->AggrElemTypeDescriptor(),
                               instances, value, 0);
    } else {
        SDAI_Enum *enumeration = attribute->Enum();
        if(!enumeration) enumeration = attribute->Boolean();
        if(!enumeration) enumeration = attribute->Logical();
        if(enumeration) {
            value->kind = Value::Kind::Enumeration;
            enumeration->asStr(value->text);
        } else {
            value->kind = Value::Kind::Opaque;
            value->text = attribute->asStr();
        }
    }
    return true;
}

} // namespace

class Document::Impl {
public:
    // Member destruction is in reverse order: reader (including headers),
    // owned data instances, then registry/descriptors. InstMgr defaults to NOT
    // owning its instances; opt in explicitly, including failure paths.
    Registry registry;
    InstMgr instances;
    Reader reader;
    std::string protocol;
    std::string declaredSchema;

    Impl() : registry(EmptyRegistry), instances(1), reader(registry, instances) {}

    SDAI_Application_instance *Entity(int id) {
        auto node = instances.FindFileId(id);
        return node ? node->GetApplication_instance() : nullptr;
    }
};

std::vector<BindingInfo> SupportedBindings() {
    std::vector<BindingInfo> result;
    for(const Binding &binding : bindings) result.push_back({binding.protocol, binding.schema});
    return result;
}

Document::Document() : impl(new Impl) {}
Document::~Document() {
    RuntimeLock lock(RuntimeMutex());
    impl.reset();
}

const std::string &Document::Protocol() const { return impl->protocol; }
const std::string &Document::DeclaredSchema() const { return impl->declaredSchema; }
size_t Document::EntityCount() const {
    RuntimeLock lock(RuntimeMutex());
    return impl->instances.InstanceCount();
}
size_t Document::EntityTypeCount() const {
    RuntimeLock lock(RuntimeMutex());
    return impl->registry.GetEntityCnt();
}

std::vector<int> Document::EntitiesOfType(const std::string &type) const {
    RuntimeLock lock(RuntimeMutex());
    std::vector<int> result;
    auto descriptor = impl->registry.FindEntity(type.c_str());
    if(!descriptor) return result;
    for(int i = 0; i < impl->instances.InstanceCount(); ++i) {
        auto entity = impl->instances.GetApplication_instance(i);
        if(entity->IsA(descriptor)) result.push_back(entity->StepFileId());
    }
    return result;
}

std::vector<std::string> Document::EntityTypes(int id) const {
    RuntimeLock lock(RuntimeMutex());
    std::vector<std::string> result;
    for(auto part = impl->Entity(id); part; part = NextPart(part)) {
        result.push_back(Upper(part->EntityName()));
    }
    return result;
}

bool Document::IsA(int id, const std::string &type) const {
    RuntimeLock lock(RuntimeMutex());
    auto entity = impl->Entity(id);
    auto descriptor = impl->registry.FindEntity(type.c_str());
    return entity && descriptor && entity->IsA(descriptor);
}

bool Document::Attribute(int id, const std::string &name, Value *value,
                         const std::string &component) const {
    RuntimeLock lock(RuntimeMutex());
    if(!value) return false;
    STEPattribute *found = nullptr;
    for(auto part = impl->Entity(id); part; part = NextPart(part)) {
        if(!component.empty() && Upper(part->EntityName()) != Upper(component)) continue;
        part->ResetAttributes();
        while(auto attribute = part->NextAttribute()) {
            if(part->IsComplex() &&
               attribute->getADesc()->AttrType() == AttrType_Redefining) continue;
            if(SimpleName(attribute->Name()) != Upper(name)) continue;
            unsigned depth = 0;
            while(attribute->RedefiningAttr()) {
                if(++depth > 32) return false;
                attribute = attribute->RedefiningAttr();
            }
            if(found && found != attribute) return false;
            found = attribute;
        }
    }
    if(!found) return false;
    Value decoded;
    if(!DecodeAttribute(found, impl->instances, &decoded)) return false;
    *value = std::move(decoded);
    return true;
}

ReadResult Read(std::istream &input) {
    RuntimeLock lock(RuntimeMutex());
    ReadResult result;
    ReadScope scope(input);
    try {
        auto position = input.tellg();
        if(position == std::streampos(-1)) {
            result.error = "STEP input must be a readable, seekable Part 21 stream.";
        } else {
            std::unique_ptr<Document> document(new Document);
            auto &data = *document->impl;
            if(!data.reader.Header(input, &data.declaredSchema)) {
                result.error = "Invalid Part 21 header; exactly one FILE_SCHEMA identifier is required.";
            } else {
                std::string name = SchemaName(data.declaredSchema);
                const Binding *selected = nullptr;
                for(const Binding &binding : bindings) {
                    if(name == binding.schema) selected = &binding;
                }
                if(!selected) {
                    result.status = ReadResult::Status::UnsupportedSchema;
                    result.error = "No compiled STEP binding for FILE_SCHEMA '" + data.declaredSchema + "'.";
                } else {
                    selected->initialize(data.registry);
                    data.protocol = selected->protocol;
                    if(!data.reader.Data(input)) {
                        result.error = "Invalid or unsupported Part 21 data; no document was accepted.\n" +
                            data.reader.Error().UserMsg() + data.reader.Error().DetailMsg();
                    } else {
                        result.status = ReadResult::Status::Okay;
                        result.document = std::move(document);
                    }
                }
            }
        }
    } catch(const std::exception &error) {
        result.error = std::string("STEP reader failed: ") + error.what();
    }
    result.readerLog = scope.buffer.text;
    return result;
}

} // namespace Step
} // namespace SolveSpace
