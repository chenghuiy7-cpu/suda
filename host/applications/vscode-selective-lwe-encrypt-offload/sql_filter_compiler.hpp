#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "selective_filter_protocol.h"

struct SqlField {
    std::string name;
    uint32_t offset;
    uint8_t type;
};

struct SqlPredicate {
    uint32_t offset;
    uint8_t type;
    uint8_t comparison;
    uint64_t literal;
};

struct SqlFilterProgram {
    uint32_t record_bytes = 0;
    uint32_t projection_offset = 0;
    uint8_t projection_type = 0;
    std::vector<SqlPredicate> predicates;
    std::vector<uint8_t> tokens;
    std::string query;
};

bool parse_sql_schema(
    const std::string &text,
    uint32_t record_bytes,
    std::vector<SqlField> *fields,
    std::string *error);

bool compile_filter_sql(
    const std::string &query,
    uint32_t record_bytes,
    const std::vector<SqlField> &fields,
    SqlFilterProgram *program,
    std::string *error);

bool evaluate_filter_sql(
    const SqlFilterProgram &program,
    const uint8_t *record,
    size_t record_bytes,
    bool *selected,
    uint64_t *projection,
    std::string *error);

uint32_t sql_filter_type_width(uint8_t type);
const char *sql_filter_type_name(uint8_t type);
