// SPDX-License-Identifier: MIT
// Identifier-anonymising rewriter for ShowPlanXML.
//
// Walks a plan XML document, replaces identifier-bearing attributes
// (Database, Schema, Table, Column, Index, Alias, ...) with deterministic
// per-category placeholder names (Db_1, Schema_1, Tbl_1, Col_1, Idx_1).
// The mapping is exposed so a caller can apply the same names to other
// surfaces (SQL text in traces, the .osession `objects` table, ...).
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace showplan {

class AnonymizeMapper {
public:
    AnonymizeMapper() = default;

    // Walk the given ShowPlanXML and return a rewritten XML with all
    // recognised identifier-bearing attributes replaced. The internal
    // mapping is updated so subsequent rewrite() calls reuse the same
    // placeholder for the same original name. StatementText attributes
    // are NOT rewritten here - the caller routes those through a SQL
    // tokenizer (libtsql) instead.
    std::string rewrite(std::string_view xml);

    // Original (case-insensitive, lowercased) identifier -> placeholder.
    // Use this to drive other rewriters (libtsql, plain string subst).
    const std::unordered_map<std::string, std::string>& mapping() const {
        return by_name_;
    }

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
