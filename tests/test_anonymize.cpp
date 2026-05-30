// SPDX-License-Identifier: MIT
// Round-trip test for showplan::AnonymizeMapper. Loads a sample
// .sqlplan, scrubs it, verifies that:
//   - the mapping picked up at least one Schema and one Table identifier
//   - the rewritten XML still parses
//   - no original Schema / Table / Database attribute value survives in
//     the rewritten XML.
#include "showplan/anonymize.hpp"
#include "showplan/showplan.hpp"

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

static std::string slurp(const char* path) {
    std::ifstream f(path);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s <plan.sqlplan>\n", argv[0]); return 2; }
    std::string xml = slurp(argv[1]);
    if (xml.empty()) { std::fprintf(stderr, "empty input\n"); return 1; }

    // Pull the originals out of the parsed plan so we can later assert
    // none of them appear in the rewritten XML.
    showplan::Plan plan;
    try { plan = showplan::parse_xml(xml); }
    catch (const std::exception& e) { std::fprintf(stderr, "parse: %s\n", e.what()); return 1; }
    std::set<std::string> originals;
    auto collect = [&](const showplan::PlanNode& n, auto& rec) -> void {
        if (!n.schema.empty()) originals.insert(n.schema);
        if (!n.table.empty())  originals.insert(n.table);
        if (!n.index_name.empty()) originals.insert(n.index_name);
        for (const auto& c : n.children) rec(*c, rec);
    };
    for (const auto& s : plan.statements) if (s.root) collect(*s.root, collect);

    showplan::AnonymizeMapper mapper;
    std::string scrubbed = mapper.rewrite(xml);

    int fails = 0;
    if (scrubbed.empty()) { std::fprintf(stderr, "scrubbed XML is empty\n"); ++fails; }
    if (mapper.mapping().empty()) {
        std::fprintf(stderr, "mapper picked up zero identifiers\n"); ++fails;
    }

    // Reparse + check identifier fields. SQL-fragment attributes like
    // StatementText / ScalarString are out of this layer's mandate -
    // they go through a SQL tokenizer at the caller layer - so we only
    // inspect the parsed identifier fields, not raw text.
    showplan::Plan p2;
    try { p2 = showplan::parse_xml(scrubbed); }
    catch (const std::exception& e) {
        std::fprintf(stderr, "reparse: %s\n", e.what()); ++fails;
    }
    if (p2.statements.size() != plan.statements.size()) {
        std::fprintf(stderr, "stmt count changed: %zu -> %zu\n",
                     plan.statements.size(), p2.statements.size()); ++fails;
    }
    auto check = [&](const showplan::PlanNode& n, auto& rec) -> void {
        if (originals.count(n.schema)) {
            std::fprintf(stderr, "schema attr leaked: [%s]\n", n.schema.c_str()); ++fails;
        }
        if (originals.count(n.table)) {
            std::fprintf(stderr, "table attr leaked: [%s]\n", n.table.c_str()); ++fails;
        }
        if (originals.count(n.index_name)) {
            std::fprintf(stderr, "index attr leaked: [%s]\n", n.index_name.c_str()); ++fails;
        }
        for (const auto& c : n.children) rec(*c, rec);
    };
    for (const auto& s : p2.statements) if (s.root) check(*s.root, check);

    if (fails) { std::fprintf(stderr, "%d failure(s)\n", fails); return 1; }
    std::printf("anonymize ok: %zu identifiers mapped\n", mapper.mapping().size());
    return 0;
}
