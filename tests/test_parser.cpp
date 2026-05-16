// SPDX-License-Identifier: MIT
// Stand-alone smoke test. Parses the file at argv[1] and dumps a summary.
// Exit code: 0 on success, 1 on parse failure, 2 on misuse.

#include "showplan/showplan.hpp"

#include <cstdio>

static void dump_node(const showplan::PlanNode& n, int depth) {
    for (int i = 0; i < depth; ++i) std::printf("  ");
    std::printf("[%d] %s (%s)  est_rows=%.0f cost=%.4f",
                n.node_id, n.physical_op.c_str(), n.logical_op.c_str(),
                n.est_rows, n.est_op_cost);
    if (!n.table.empty()) {
        std::printf("  -> %s.%s", n.schema.c_str(), n.table.c_str());
        if (!n.index_name.empty()) std::printf(".%s", n.index_name.c_str());
    }
    if (n.is_lookup)   std::printf("  [lookup]");
    if (n.has_runtime) std::printf("  act=%lld", (long long)n.act_rows);
    std::printf("\n");
    for (const auto& c : n.children) dump_node(*c, depth + 1);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <plan.sqlplan>\n", argv[0]);
        return 2;
    }

    showplan::Plan plan;
    try {
        plan = showplan::parse_file(argv[1]);
    } catch (const showplan::ParseError& e) {
        std::fprintf(stderr, "parse error: %s\n", e.what());
        return 1;
    }

    std::printf("server: %s build %s   database: %s\n",
                plan.server_version.c_str(), plan.build.c_str(),
                showplan::predominant_database(plan).c_str());
    std::printf("statements: %zu\n", plan.statements.size());

    for (const auto& s : plan.statements) {
        std::printf("\n[stmt %d] type=%s subtree=%.4f compile=%dms "
                    "params=%zu stats=%zu\n",
                    s.statement_id, s.stmt_type.c_str(), s.subtree_cost,
                    s.compile_time_ms, s.parameters.size(), s.stats.size());
        if (!s.text.empty()) {
            std::printf("  sql: %.140s%s\n", s.text.c_str(),
                        s.text.size() > 140 ? "..." : "");
        }
        std::printf("  missing indexes: %zu, warnings: %zu\n",
                    s.missing_indexes.size(), s.warnings.size());
        for (const auto& mi : s.missing_indexes) {
            std::printf("    impact=%.1f  %s\n", mi.impact, mi.ddl.c_str());
        }
        for (const auto& w : s.warnings) {
            std::printf("    warn: %s  %s\n",
                        w.kind.c_str(), w.detail.c_str());
        }
        if (s.root) dump_node(*s.root, 1);
    }
    return 0;
}
