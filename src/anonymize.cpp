// SPDX-License-Identifier: MIT
#include "showplan/anonymize.hpp"

#include "pugixml.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace showplan {

namespace {

enum class Category {
    Database, Schema, Table, Column, Index, Alias, Procedure, Other,
};

// Identifier-bearing attribute names we recognise. Picked from the
// common ShowPlanXML schema surface: Object / ColumnReference /
// SchemaObject / OutputColumn / SeekRange / etc.
Category classify(const char* attr_name) {
    if (std::strcmp(attr_name, "Database") == 0)        return Category::Database;
    if (std::strcmp(attr_name, "Schema") == 0)          return Category::Schema;
    if (std::strcmp(attr_name, "Table") == 0)           return Category::Table;
    if (std::strcmp(attr_name, "Index") == 0)           return Category::Index;
    if (std::strcmp(attr_name, "Column") == 0)          return Category::Column;
    if (std::strcmp(attr_name, "ComputedColumn") == 0)  return Category::Column;
    if (std::strcmp(attr_name, "Alias") == 0)           return Category::Alias;
    if (std::strcmp(attr_name, "ProcName") == 0)        return Category::Procedure;
    if (std::strcmp(attr_name, "FunctionName") == 0)    return Category::Procedure;
    if (std::strcmp(attr_name, "ServerName") == 0)      return Category::Other;
    if (std::strcmp(attr_name, "ProcedureName") == 0)   return Category::Procedure;
    if (std::strcmp(attr_name, "TableName") == 0)       return Category::Table;
    return Category::Other;
}

std::string lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back((c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c);
    return out;
}

}  // namespace

std::string AnonymizeMapper::rewrite(std::string_view xml) {
    pugi::xml_document doc;
    auto load = doc.load_buffer(xml.data(), xml.size(), pugi::parse_default,
                                pugi::encoding_auto);
    if (!load) return std::string{xml};  // unparseable; pass through unchanged

    // Walk every element, examine every attribute, replace identifier-bearing
    // ones via category-specific counters. pugixml's recursive iteration is
    // depth-first which is fine; order determines numbering.
    struct Walker : pugi::xml_tree_walker {
        AnonymizeMapper* m;
        bool for_each(pugi::xml_node& n) override {
            if (n.type() != pugi::node_element) return true;
            for (pugi::xml_attribute a : n.attributes()) {
                const char* name = a.name();
                const char* value = a.value();
                if (!value || !*value) continue;
                Category cat = classify(name);
                // Anything unrecognised stays as-is. Object IDs, numeric
                // attributes (StatementId, etc.) and the StatementText
                // attribute all fall through here.
                if (cat == Category::Other && std::strcmp(name, "ServerName") != 0)
                    continue;
                std::string key = lower(value);
                auto it = m->by_name_.find(key);
                std::string replacement;
                if (it != m->by_name_.end()) {
                    replacement = it->second;
                } else {
                    char buf[32];
                    int n_;
                    const char* prefix = "Other_";
                    switch (cat) {
                        case Category::Database:  prefix = "Db_";     n_ = ++m->next_db_;     break;
                        case Category::Schema:    prefix = "Schema_"; n_ = ++m->next_schema_; break;
                        case Category::Table:     prefix = "Tbl_";    n_ = ++m->next_table_;  break;
                        case Category::Column:    prefix = "Col_";    n_ = ++m->next_column_; break;
                        case Category::Index:     prefix = "Idx_";    n_ = ++m->next_index_;  break;
                        case Category::Alias:     prefix = "Alias_";  n_ = ++m->next_alias_;  break;
                        case Category::Procedure: prefix = "Proc_";   n_ = ++m->next_proc_;   break;
                        case Category::Other:     prefix = "Srv_";    n_ = ++m->next_other_;  break;
                    }
                    std::snprintf(buf, sizeof(buf), "%s%d", prefix, n_);
                    replacement = buf;
                    m->by_name_.emplace(key, replacement);
                }
                a.set_value(replacement.c_str());
            }
            return true;
        }
    };
    Walker w; w.m = this;
    doc.traverse(w);

    struct Sink : pugi::xml_writer {
        std::string out;
        void write(const void* data, size_t size) override {
            out.append(static_cast<const char*>(data), size);
        }
    };
    Sink sink;
    doc.save(sink, "", pugi::format_raw, pugi::encoding_auto);
    return std::move(sink.out);
}

}  // namespace showplan
