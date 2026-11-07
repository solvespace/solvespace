// Standalone dependency tests: intentionally include no SolveSpace core or
// STEPcode headers. A PRIVATE link must not leak vendor definitions here.
#include "stepreader.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

#if defined(SC_STATIC) || defined(SC_SCHEMA_EXPORT) || defined(REAL_TYPE)
#error "STEPcode compile definitions leaked through the reader interface"
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

using namespace SolveSpace;
using Kind = Step::Value::Kind;
using Status = Step::ReadResult::Status;

namespace {
int cases = 0, checks = 0, failures = 0;
struct Failure {};

#if defined(_MSC_VER) && defined(_DEBUG)
int CrtFailure(int type, char *message, int *) {
    // Unattended malformed-input tests must fail, not wait for a CRT dialog.
    if(type != _CRT_ASSERT && type != _CRT_ERROR) return 0;
    std::fputs(message, stderr);
    std::_Exit(3);
}
#endif

void Check(bool okay, const char *expression, int line) {
    ++checks;
    if(!okay) {
        std::cerr << "  line " << line << ": " << expression << "\n";
        throw Failure();
    }
}
#define REQUIRE(expr) Check(!!(expr), #expr, __LINE__)

void Run(const std::string &name, const std::function<void()> &test) {
    const char *filter = std::getenv("SLVS_STEP_TEST_FILTER");
    if(filter && name.find(filter) == std::string::npos) return;
    ++cases;
    std::cout << name << ": " << std::flush;
    try {
        test();
        std::cout << "OK\n";
    } catch(const Failure &) {
        ++failures;
        std::cout << "FAIL\n";
    } catch(const std::exception &error) {
        ++failures;
        std::cout << "FAIL: " << error.what() << "\n";
    }
}

std::string TextFile(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if(!input) throw std::runtime_error("Cannot open test input " + path);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

std::string Replace(std::string text, const std::string &from, const std::string &to) {
    size_t position = text.find(from);
    if(position == std::string::npos) throw std::runtime_error("Test replacement not found: " + from);
    text.replace(position, from.size(), to);
    return text;
}

Step::ReadResult ReadText(const std::string &text) {
    std::istringstream input(text);
    return Step::Read(input);
}

std::string Exchange(const std::string &schema, const std::string &data) {
    return "ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('proof'),'2;1');\n"
           "FILE_NAME('','',(''),(''),'','','');\nFILE_SCHEMA(('" + schema +
           "'));\nENDSEC;\nDATA;\n" + data + "\nENDSEC;\nEND-ISO-10303-21;\n";
}

std::string Common(const std::string &schema) {
    auto text = Replace(TextFile(std::string(STEP_TEST_SOURCE_DIR) + "/test/step/common.step.in"),
                        "@SCHEMA@", schema);
    return Replace(text, "@CONVERSION_DIMENSIONS@", schema == "AUTOMOTIVE_DESIGN" ? "*" : "#160");
}

std::unique_ptr<Step::Document> Accepted(Step::ReadResult result) {
    if(!result.document) std::cerr << result.error << "\n" << result.readerLog;
    REQUIRE(result.status == Status::Okay);
    REQUIRE(result.document);
    REQUIRE(result.error.empty());
    return std::move(result.document);
}

Step::Value Attribute(const Step::Document &document, int id, const std::string &name,
                     const std::string &component = "") {
    Step::Value value;
    REQUIRE(document.Attribute(id, name, &value, component));
    return value;
}

void Rejected(const std::string &text, Status status = Status::InvalidFile) {
    auto result = ReadText(text);
    REQUIRE(!result.document);
    REQUIRE(result.status == status);
    REQUIRE(!result.error.empty());
}

bool HasBinding(const std::string &protocol) {
    for(const auto &binding : Step::SupportedBindings()) {
        if(binding.protocol == protocol) return true;
    }
    return false;
}

void CommonTopology(const Step::BindingInfo &binding) {
    auto document = Accepted(ReadText(Common(binding.schema)));
    REQUIRE(document->Protocol() == binding.protocol);
    REQUIRE(document->DeclaredSchema() == binding.schema);
    REQUIRE(document->EntityCount() > 80);
    size_t types = binding.protocol == "AP203" ? 254 : (binding.protocol == "AP214" ? 915 : 2407);
    REQUIRE(document->EntityTypeCount() == types);
    REQUIRE(document->EntitiesOfType("ADVANCED_FACE").size() == 4);
    REQUIRE(document->EntitiesOfType("EDGE_CURVE").size() == 6);
    REQUIRE(document->EntitiesOfType("MANIFOLD_SOLID_BREP").size() == 2);
    REQUIRE(document->IsA(141, "solid_model"));
    REQUIRE(document->IsA(141, "MANIFOLD_SOLID_BREP"));
    REQUIRE(!document->IsA(141, "BREP_WITH_VOIDS"));
    REQUIRE(document->IsA(181, "BREP_WITH_VOIDS"));
    REQUIRE(Attribute(*document, 141, "outer").reference == 140);
    auto faces = Attribute(*document, 140, "cfs_faces");
    REQUIRE(faces.kind == Kind::Aggregate && faces.elements.size() == 4);
    REQUIRE(faces.elements.front().reference == 131);
    REQUIRE(Attribute(*document, 131, "face_geometry").reference == 121);
    REQUIRE(Attribute(*document, 91, "bound").reference == 81);
    REQUIRE(Attribute(*document, 61, "edge_start").kind == Kind::Derived);
    REQUIRE(Attribute(*document, 61, "edge_element").reference == 53);
    REQUIRE(Attribute(*document, 61, "orientation").text == "F");
    auto point = Attribute(*document, 2, "coordinates");
    REQUIRE(point.elements.size() == 3 && point.elements.front().kind == Kind::Real);
    REQUIRE(std::abs(point.elements.front().real - 10.0) < 1e-12);
    REQUIRE(Attribute(*document, 1, "name").text == "'origin''s name'");
    auto voids = Attribute(*document, 181, "voids");
    REQUIRE(voids.elements.size() == 1 && voids.elements.front().reference == 180);
    REQUIRE(Attribute(*document, 180, "closed_shell_element").reference == 140);
    REQUIRE(Attribute(*document, 180, "cfs_faces").kind == Kind::Derived);
    REQUIRE(document->EntitiesOfType("TOROIDAL_SURFACE").size() == 1);
    REQUIRE(document->EntitiesOfType("no_such_type").empty());
    REQUIRE(document->EntityTypes(999999).empty());
    REQUIRE(!document->IsA(999999, "PLANE"));
    Step::Value missing;
    REQUIRE(!document->Attribute(141, "missing", &missing));
    REQUIRE(!document->Attribute(999999, "name", &missing));
    REQUIRE(!document->Attribute(141, "name", nullptr));
}

void UnitsAndComplexes(const Step::BindingInfo &binding) {
    auto document = Accepted(ReadText(Common(binding.schema)));
    REQUIRE(document->EntityTypes(151).size() == 3);
    REQUIRE(document->IsA(151, "SI_UNIT") && document->IsA(151, "LENGTH_UNIT"));
    REQUIRE(Attribute(*document, 151, "prefix", "SI_UNIT").text == "MILLI");
    REQUIRE(Attribute(*document, 151, "name", "SI_UNIT").text == "METRE");
    REQUIRE(Attribute(*document, 151, "dimensions", "NAMED_UNIT").kind == Kind::Derived);
    REQUIRE(Attribute(*document, 152, "prefix").kind == Kind::Null);
    REQUIRE(Attribute(*document, 154, "coordinate_space_dimension").integer == 3);
    auto units = Attribute(*document, 154, "units", "GLOBAL_UNIT_ASSIGNED_CONTEXT");
    REQUIRE(units.elements.size() == 3 && units.elements.front().reference == 151);
    REQUIRE(Attribute(*document, 162, "name").text == "'INCH'");
    REQUIRE(Attribute(*document, 162, "conversion_factor").reference == 161);
    auto dimensions = Attribute(*document, 162, "dimensions", "NAMED_UNIT");
    if(binding.protocol == "AP214") {
        REQUIRE(dimensions.kind == Kind::Derived);
    } else {
        REQUIRE(dimensions.kind == Kind::Reference && dimensions.reference == 160);
    }
    auto measure = Attribute(*document, 161, "value_component");
    REQUIRE(measure.kind == Kind::Real && measure.selectedType == "LENGTH_MEASURE");
    REQUIRE(std::abs(measure.real - 25.4) < 1e-12);
    REQUIRE(Attribute(*document, 161, "unit_component").reference == 151);
    auto uncertainty = Attribute(*document, 163, "value_component");
    REQUIRE(uncertainty.kind == Kind::Real && std::abs(uncertainty.real - 1e-6) < 1e-16);
    Step::Value missing;
    REQUIRE(!document->Attribute(151, "prefix", &missing, "NAMED_UNIT"));
}

void Splines(const Step::BindingInfo &binding) {
    auto document = Accepted(ReadText(Common(binding.schema)));
    REQUIRE(document->IsA(170, "RATIONAL_B_SPLINE_CURVE"));
    REQUIRE(document->IsA(170, "B_SPLINE_CURVE_WITH_KNOTS"));
    REQUIRE(Attribute(*document, 170, "degree").integer == 2);
    auto knots = Attribute(*document, 170, "knot_multiplicities");
    REQUIRE(knots.elements.size() == 2 && knots.elements.front().integer == 3);
    auto weights = Attribute(*document, 170, "weights_data");
    REQUIRE(weights.elements.size() == 3);
    REQUIRE(std::abs(weights.elements[1].real - std::sqrt(0.5)) < 1e-15);
    REQUIRE(document->IsA(171, "RATIONAL_B_SPLINE_SURFACE"));
    REQUIRE(Attribute(*document, 171, "u_degree").integer == 1);
    auto points = Attribute(*document, 171, "control_points_list");
    REQUIRE(points.elements.size() == 2 && points.elements.front().elements.size() == 2);
    REQUIRE(points.elements[1].elements[1].kind == Kind::Reference);
    REQUIRE(points.elements[1].elements[1].reference == 4);
    auto net = Attribute(*document, 171, "weights_data");
    REQUIRE(net.elements.size() == 2 && net.elements[0].elements.size() == 2);
    REQUIRE(std::abs(net.elements[0].elements[1].real - 0.5) < 1e-15);
    REQUIRE(std::abs(net.elements[1].elements[0].real - 2.0) < 1e-15);
}

std::vector<std::pair<std::string, std::string>> MalformedInputs(const Step::BindingInfo &binding) {
    const std::string point = "#1=CARTESIAN_POINT('',(1.,2.,3.));";
    const std::string valid = Exchange(binding.schema, point);
    return {
        { "signature", Replace(valid, "ISO-10303-21;", "ISO-10303;") },
        { "header_marker", Replace(valid, "HEADER;", "NOT_HEADER;") },
        { "missing_description", Replace(valid, "FILE_DESCRIPTION(('proof'),'2;1');", "") },
        { "missing_file_name", Replace(valid, "FILE_NAME('','',(''),(''),'','','');", "") },
        { "missing_schema", Replace(valid, "FILE_SCHEMA(('" + binding.schema + "'));", "") },
        { "duplicate_file_name", Replace(valid, "FILE_NAME('','',(''),(''),'','','');",
              "FILE_NAME('','',(''),(''),'','','');FILE_NAME('','',(''),(''),'','','');") },
        { "duplicate_schema_record", Replace(valid, "FILE_SCHEMA(('" + binding.schema + "'));",
              "FILE_SCHEMA(('" + binding.schema + "'));FILE_SCHEMA(('" + binding.schema + "'));") },
        { "multiple_schemas", Replace(valid, "FILE_SCHEMA(('" + binding.schema + "'));",
              "FILE_SCHEMA(('" + binding.schema + "','" + binding.schema + "'));") },
        { "truncated_header", valid.substr(0, valid.find("ENDSEC;")) },
        { "unknown_entity", Exchange(binding.schema, "#1=NOT_IN_THE_SCHEMA();") },
        { "required_attribute", Exchange(binding.schema, "#1=CARTESIAN_POINT($,(1.,2.,3.));") },
        { "wrong_attribute_type", Exchange(binding.schema, "#1=CARTESIAN_POINT('',('bad',2.,3.));") },
        { "dangling_reference", Exchange(binding.schema, "#1=VERTEX_POINT('',#999);") },
        { "wrong_reference_type", Exchange(binding.schema,
              "#1=VERTEX_POINT('',#2);#2=DIRECTION('',(1.,0.,0.));") },
        { "duplicate_id", Exchange(binding.schema, point + point) },
        { "zero_id", Exchange(binding.schema, "#0=CARTESIAN_POINT('',(1.,2.,3.));") },
        { "missing_end_marker", Replace(valid, "END-ISO-10303-21;", "") },
        { "missing_data_endsec", Replace(valid, "\nENDSEC;\nEND-ISO-10303-21;", "\nEND-ISO-10303-21;") },
        { "trailing_garbage", valid + "unexpected trailing content" },
        { "named_data_section", Replace(valid, "DATA;", "DATA('named');") },
        { "multiple_data_sections", Replace(valid, "END-ISO-10303-21;", "DATA;ENDSEC;END-ISO-10303-21;") },
        { "complex_bad_enumeration", Exchange(binding.schema,
              "#1=(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.NOT_A_PREFIX.,.METRE.));") }
    };
}

void RepeatedCleanup() {
    auto bindings = Step::SupportedBindings();
    std::vector<std::pair<std::string, std::string>> inputs;
    for(const auto &binding : bindings) {
        for(const auto &sample : MalformedInputs(binding)) {
            inputs.emplace_back(binding.protocol + "/" + sample.first, sample.second);
        }
    }
    auto exercise = [&]() {
        for(const auto &binding : bindings) {
            auto document = Accepted(ReadText(Common(binding.schema)));
            auto weights = Attribute(*document, 171, "weights_data");
            REQUIRE(weights.elements.size() == 2);
        }
        for(const auto &input : inputs) {
            if(std::getenv("SLVS_STEP_TEST_TRACE_CLEANUP")) {
                std::cerr << "\n  cleanup: " << input.first << std::flush;
            }
            Rejected(input.second);
        }
    };
    exercise(); // warm schema vectors, locale and descriptor-layout cache buckets
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemState before, after, difference;
    _CrtMemCheckpoint(&before);
#endif
    for(int i = 0; i < 20; ++i) exercise();
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemCheckpoint(&after);
    if(_CrtMemDifference(&difference, &before, &after)) {
        std::cerr << "Outstanding normal CRT blocks: " << difference.lCounts[_NORMAL_BLOCK]
                  << ", bytes: " << difference.lSizes[_NORMAL_BLOCK] << "\n";
    }
    REQUIRE(after.lCounts[_NORMAL_BLOCK] == before.lCounts[_NORMAL_BLOCK]);
    REQUIRE(after.lSizes[_NORMAL_BLOCK] == before.lSizes[_NORMAL_BLOCK]);
#endif
}

class CommaPunctuation : public std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};

void NistCorpus(const std::string &directory) {
    REQUIRE(HasBinding("AP242"));
    struct Sample { const char *file; size_t instances; size_t faces; };
    const Sample files[] = {
        { "nist_ctc_01_asme1_ap242-e1.stp", 4350, 117 },
        { "nist_ctc_03_asme1_ap242-e2.stp", 5920, 120 }
    };
    for(const auto &file : files) {
        auto document = Accepted(ReadText(TextFile(directory + "/" + file.file)));
        REQUIRE(document->Protocol() == "AP242");
        REQUIRE(document->EntityCount() == file.instances);
        REQUIRE(document->EntitiesOfType("MANIFOLD_SOLID_BREP").size() == 1);
        REQUIRE(document->EntitiesOfType("ADVANCED_FACE").size() == file.faces);
        REQUIRE(document->EntitiesOfType("CONVERSION_BASED_UNIT").size() == 3);
    }
    if(HasBinding("AP203")) {
        // This AP203-labelled sample includes presentation entities absent from
        // CONFIG_CONTROL_DESIGN. Do not drop them or silently use AP214/AP242.
        auto result = ReadText(TextFile(directory + "/AP203 geometry only/nist_ftc_11_asme1_rb.stp"));
        REQUIRE(!result.document && result.status == Status::InvalidFile);
        REQUIRE(result.readerLog.find("COLOUR_RGB") != std::string::npos);
    }
}

} // namespace

int main(int argc, char **argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportHook(CrtFailure);
#endif
    auto bindings = Step::SupportedBindings();
    Run("compiled_bindings", [&]() { REQUIRE(!bindings.empty()); });
    for(const auto &binding : bindings) {
        Run(binding.protocol + "/topology", [&]() { CommonTopology(binding); });
        Run(binding.protocol + "/units_and_complexes", [&]() { UnitsAndComplexes(binding); });
        Run(binding.protocol + "/rational_splines", [&]() { Splines(binding); });
        for(const auto &sample : MalformedInputs(binding)) {
            Run(binding.protocol + "/malformed/" + sample.first, [&]() { Rejected(sample.second); });
        }
        Run(binding.protocol + "/qualified_schema", [&]() {
            auto document = Accepted(ReadText(Exchange(binding.schema + " { 1 0 10303 1 }",
                "#1=CARTESIAN_POINT('',(1.,2.,3.));")));
            REQUIRE(document->Protocol() == binding.protocol);
            REQUIRE(document->DeclaredSchema().find('{') != std::string::npos);
        });
        Run(binding.protocol + "/independent_live_documents", [&]() {
            auto first = Accepted(ReadText(Common(binding.schema)));
            {
                auto second = Accepted(ReadText(Common(binding.schema)));
                REQUIRE(Attribute(*first, 141, "outer").reference == 140);
                REQUIRE(Attribute(*second, 141, "outer").reference == 140);
            }
            REQUIRE(Attribute(*first, 171, "control_points_list").elements.size() == 2);
            REQUIRE(first->IsA(141, "MANIFOLD_SOLID_BREP"));
        });
        Run(binding.protocol + "/numeric_locale", [&]() {
            std::locale comma(std::locale::classic(), new CommaPunctuation);
            std::istringstream input(Exchange(binding.schema, "#1=CARTESIAN_POINT('',(1.5,2.,3.));"));
            input.imbue(comma);
            auto document = Accepted(Step::Read(input));
            REQUIRE(input.getloc() == comma);
            REQUIRE(std::abs(Attribute(*document, 1, "coordinates").elements[0].real - 1.5) < 1e-15);
            std::istringstream invalid(Exchange(binding.schema, "#1=NOT_AN_ENTITY();"));
            invalid.imbue(comma);
            auto failure = Step::Read(invalid);
            REQUIRE(!failure.document);
            REQUIRE(invalid.getloc() == comma);
        });
        Run(binding.protocol + "/no_cross_schema_fallback", [&]() {
            const std::string data = "#1=COLOUR_RGB('',1.,0.,0.);";
            if(binding.protocol == "AP203") {
                Rejected(Exchange(binding.schema, data));
            } else {
                auto document = Accepted(ReadText(Exchange(binding.schema, data)));
                REQUIRE(document->Protocol() == binding.protocol);
                REQUIRE(document->IsA(1, "COLOUR_RGB"));
            }
        });
    }
    Run("unknown_and_disabled_schemas", [&]() {
        Rejected(Exchange("NOT_A_SCHEMA", ""), Status::UnsupportedSchema);
        Rejected(Exchange("AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF", ""),
                 Status::UnsupportedSchema);
        for(const std::string protocol : {"AP203", "AP214", "AP242"}) {
            if(HasBinding(protocol)) continue;
            const char *schema = protocol == "AP203" ? "CONFIG_CONTROL_DESIGN" :
                (protocol == "AP214" ? "AUTOMOTIVE_DESIGN" :
                 "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF");
            Rejected(Exchange(schema, ""), Status::UnsupportedSchema);
        }
    });
    Run("not_part21", [&]() {
        Rejected("");
        Rejected(std::string(10000, 'x'));
    });
    Run("cross_protocol_live_documents", [&]() {
        std::vector<std::unique_ptr<Step::Document>> documents;
        for(const auto &binding : bindings) {
            documents.push_back(Accepted(ReadText(Common(binding.schema))));
        }
        for(size_t i = 0; i < documents.size(); ++i) {
            REQUIRE(documents[i]->Protocol() == bindings[i].protocol);
            REQUIRE(Attribute(*documents[i], 141, "outer").reference == 140);
            REQUIRE(Attribute(*documents[i], 171, "weights_data").elements.size() == 2);
            Rejected(Exchange(bindings[i].schema, "#1=NOT_AN_ENTITY();"));
            REQUIRE(documents[i]->IsA(141, "MANIFOLD_SOLID_BREP"));
        }
        while(!documents.empty()) {
            documents.pop_back();
            for(const auto &document : documents) {
                REQUIRE(Attribute(*document, 2, "coordinates").elements[0].real == 10.0);
            }
        }
    });
    if(HasBinding("AP214")) {
        Run("AP214/real_cocreate_solid", [&]() {
            auto document = Accepted(ReadText(TextFile(std::string(STEP_TEST_SOURCE_DIR) +
                "/extlib/stepcode/data/ap214e3/io1-cm-214.stp")));
            REQUIRE(document->EntityCount() == 917);
            REQUIRE(document->EntitiesOfType("MANIFOLD_SOLID_BREP").size() == 1);
            REQUIRE(document->EntitiesOfType("ADVANCED_FACE").size() == 29);
            REQUIRE(std::abs(Attribute(*document, 50, "radius").real - 44.0) < 1e-12);
        });
        Run("AP214/real_spline_assembly", [&]() {
            auto document = Accepted(ReadText(TextFile(std::string(STEP_TEST_SOURCE_DIR) +
                "/extlib/stepcode/data/ap214e3/as1-oc-214.stp")));
            REQUIRE(document->EntityCount() == 6425);
            REQUIRE(document->EntitiesOfType("MANIFOLD_SOLID_BREP").size() == 5);
            REQUIRE(document->EntitiesOfType("B_SPLINE_SURFACE").size() == 28);
            auto surfaces = document->EntitiesOfType("B_SPLINE_SURFACE");
            REQUIRE(!Attribute(*document, surfaces.front(), "control_points_list").elements.empty());
        });
        Run("AP214/older_conversion_unit_layout_rejected", [&]() {
            auto result = ReadText(TextFile(std::string(STEP_TEST_SOURCE_DIR) +
                "/extlib/stepcode/data/ap214e3/dm1-id-214.stp"));
            REQUIRE(!result.document && result.status == Status::InvalidFile);
            REQUIRE(result.error.find("#25") != std::string::npos);
            REQUIRE(result.error.find("dimensions") != std::string::npos);
        });
    }
    if(HasBinding("AP242")) {
        Run("AP242/upstream_styled_item", [&]() {
            auto document = Accepted(ReadText(TextFile(std::string(STEP_TEST_SOURCE_DIR) +
                "/extlib/stepcode/test/p21/test_ap242_styled_item.stp")));
            REQUIRE(document->EntityCount() == 18);
            REQUIRE(document->EntitiesOfType("STYLED_ITEM").size() == 2);
            REQUIRE(Attribute(*document, 50, "item").reference == 20);
        });
    }
    if(argc == 3 && std::string(argv[1]) == "--nist-dir") {
        Run("AP242/NIST_real_solids", [&]() { NistCorpus(argv[2]); });
    } else if(argc != 1) {
        std::cerr << "Usage: solvespace-step-testsuite [--nist-dir directory]\n";
        return 2;
    }
    Run("repeated_load_and_failure_cleanup", RepeatedCleanup);
    std::cout << cases << " cases, " << checks << " checks, " << failures << " failures\n";
    if(!cases) return 2;
    return failures ? 1 : 0;
}
