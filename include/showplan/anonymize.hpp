// SPDX-License-Identifier: MIT
// Identifier-anonymising rewriter for ShowPlanXML.
//
// Walks a plan XML document, replaces identifier-bearing attributes
// (Database, Schema, Table, Column, Index, Alias, ...) with deterministic
// per-category placeholder names (Db_1, Schema_1, Tbl_1, Col_1, Idx_1).
// The mapping is exposed so a caller can apply the same names to other
// surfaces (SQL text in traces, the .osession `objects` table, ...).
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace showplan {

class AnonymizeMapper {
public:
    AnonymizeMapper() = default;

    // Caller-supplied rewriter for SQL-fragment attribute values
    // (StatementText / ScalarString). Invoked in a second pass, after
    // the full identifier mapping has been built from this document.
    using SqlRewriter = std::function<std::string(std::string_view)>;

    // Walk ShowPlanXML and return a rewritten XML. Two passes:
    //  1. Identifier-bearing attributes (Database, Schema, Table, ...)
    //     are replaced via the mapping.
    //  2. If `sql_rewriter` is set, SQL-fragment attributes
    //     (StatementText, ScalarString) are routed through it. The
    //     mapping is already complete at this point so callers using
    //     a SQL tokenizer (libtsql) see the full set of identifiers.
    // The mapping persists across calls so multi-document inputs stay
    // self-consistent.
    std::string rewrite(std::string_view xml,
                        const SqlRewriter& sql_rewriter = {});

    // Original (case-insensitive, lowercased) identifier -> placeholder.
    // Use this to drive other rewriters (libtsql, plain string subst).
    const std::unordered_map<std::string, std::string>& mapping() const {
        return by_name_;
    }

    // Ensure `name` has a placeholder in the mapping, creating one
    // ("Other_N") if missing, and return the placeholder. Used to
    // register identifiers found outside the showplan XML (e.g.
    // dotted names in the .osession objects table, SQL fragments in
    // statement text) so the cross-surface mapping stays consistent.
    std::string ensure(std::string_view name);

private:
    std::unordered_map<std::string, std::string> by_name_;
    int next_db_ = 0;
    int next_schema_ = 0;
    int next_table_ = 0;
    int next_column_ = 0;
    int next_index_ = 0;
    int next_alias_ = 0;
    int next_proc_ = 0;
    int next_other_ = 0;
};

}  // namespace showplan
