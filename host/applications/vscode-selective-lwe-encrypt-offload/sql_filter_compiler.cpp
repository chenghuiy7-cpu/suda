#include "sql_filter_compiler.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim(const std::string &value)
{
    size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

bool parse_type(const std::string &name, uint8_t *type)
{
    const std::string n = lower(name);
    const char *names[] = {"u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64"};
    for (uint8_t i = 0; i < 8; ++i) {
        if (n == names[i]) { *type = i; return true; }
    }
    return false;
}

const SqlField *find_field(const std::vector<SqlField> &fields, const std::string &name)
{
    const std::string wanted = lower(name);
    for (const SqlField &field : fields)
        if (lower(field.name) == wanted) return &field;
    return nullptr;
}

enum class TokenKind { Identifier, Number, Comparison, LParen, RParen, End };
struct LexToken { TokenKind kind; std::string text; };

bool tokenize(const std::string &input, std::vector<LexToken> *tokens, std::string *error)
{
    tokens->clear();
    for (size_t i = 0; i < input.size();) {
        unsigned char c = input[i];
        if (std::isspace(c) || c == ';') { ++i; continue; }
        if (std::isalpha(c) || c == '_') {
            size_t start = i++;
            while (i < input.size() &&
                   (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) ++i;
            tokens->push_back({TokenKind::Identifier, input.substr(start, i - start)});
        } else if (std::isdigit(c) ||
                   (c == '-' && i + 1 < input.size() &&
                    std::isdigit(static_cast<unsigned char>(input[i + 1])))) {
            size_t start = i++;
            while (i < input.size() &&
                   (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == 'x' || input[i] == 'X')) ++i;
            tokens->push_back({TokenKind::Number, input.substr(start, i - start)});
        } else if (c == '(' || c == ')') {
            tokens->push_back({c == '(' ? TokenKind::LParen : TokenKind::RParen,
                               std::string(1, static_cast<char>(c))});
            ++i;
        } else if (c == '=' || c == '!' || c == '<' || c == '>') {
            size_t start = i++;
            if (i < input.size() && input[i] == '=') ++i;
            std::string op = input.substr(start, i - start);
            if (op == "!") { *error = "expected != after !"; return false; }
            tokens->push_back({TokenKind::Comparison, op});
        } else {
            *error = "unsupported SQL character at offset " + std::to_string(i);
            return false;
        }
    }
    tokens->push_back({TokenKind::End, ""});
    return true;
}

class Parser {
public:
    Parser(const std::vector<LexToken> &tokens, const std::vector<SqlField> &fields,
           SqlFilterProgram *program, std::string *error)
        : tokens_(tokens), fields_(fields), program_(program), error_(error) {}

    bool parse()
    {
        if (!keyword("select")) return fail("query must start with SELECT");
        if (peek().kind != TokenKind::Identifier) return fail("SELECT requires one field name");
        const SqlField *projection = find_field(fields_, take().text);
        if (!projection) return fail("unknown SELECT field");
        program_->projection_offset = projection->offset;
        program_->projection_type = projection->type;
        if (keyword("from")) {
            if (peek().kind != TokenKind::Identifier) return fail("FROM requires a table name");
            take(); // SSD range already identifies the table; name is documentary.
        }
        if (!keyword("where")) return fail("SELECT query requires WHERE");
        if (!parse_or()) return false;
        if (peek().kind != TokenKind::End) return fail("unexpected token after WHERE expression: " + peek().text);
        if (program_->predicates.empty()) return fail("WHERE requires at least one comparison");
        if (program_->predicates.size() > SELECTIVE_FILTER_V2_MAX_PREDICATES ||
            program_->tokens.size() > SELECTIVE_FILTER_V2_MAX_TOKENS)
            return fail("query exceeds FPGA limit of 8 comparisons and 15 expression tokens");
        return true;
    }

private:
    bool parse_or()
    {
        if (!parse_and()) return false;
        while (keyword("or")) {
            if (!parse_and()) return false;
            program_->tokens.push_back(SELECTIVE_FILTER_TOKEN_OR);
        }
        return true;
    }
    bool parse_and()
    {
        if (!parse_not()) return false;
        while (keyword("and")) {
            if (!parse_not()) return false;
            program_->tokens.push_back(SELECTIVE_FILTER_TOKEN_AND);
        }
        return true;
    }
    bool parse_not()
    {
        if (keyword("not")) {
            if (!parse_not()) return false;
            program_->tokens.push_back(SELECTIVE_FILTER_TOKEN_NOT);
            return true;
        }
        return parse_primary();
    }
    bool parse_primary()
    {
        if (peek().kind == TokenKind::LParen) {
            take();
            if (!parse_or()) return false;
            if (peek().kind != TokenKind::RParen) return fail("missing closing parenthesis");
            take();
            return true;
        }
        if (peek().kind != TokenKind::Identifier) return fail("expected field comparison");
        const std::string field_name = take().text;
        const SqlField *field = find_field(fields_, field_name);
        if (!field) return fail("unknown WHERE field: " + field_name);
        if (peek().kind != TokenKind::Comparison) return fail("expected comparison after " + field_name);
        const std::string op = take().text;
        uint8_t comparison;
        if (op == "=") comparison = SELECTIVE_FILTER_CMP_EQ;
        else if (op == "!=") comparison = SELECTIVE_FILTER_CMP_NE;
        else if (op == "<") comparison = SELECTIVE_FILTER_CMP_LT;
        else if (op == "<=") comparison = SELECTIVE_FILTER_CMP_LE;
        else if (op == ">") comparison = SELECTIVE_FILTER_CMP_GT;
        else if (op == ">=") comparison = SELECTIVE_FILTER_CMP_GE;
        else return fail("unsupported comparison: " + op);
        if (peek().kind != TokenKind::Number) return fail("comparison requires an integer literal");
        uint64_t literal;
        if (!parse_literal(take().text, field->type, &literal)) return false;
        if (program_->predicates.size() >= SELECTIVE_FILTER_V2_MAX_PREDICATES)
            return fail("query exceeds FPGA limit of 8 comparisons");
        program_->predicates.push_back({field->offset, field->type, comparison, literal});
        program_->tokens.push_back(static_cast<uint8_t>(program_->predicates.size() - 1));
        return true;
    }
    bool parse_literal(const std::string &text, uint8_t type, uint64_t *literal)
    {
        errno = 0;
        char *end = nullptr;
        const bool signed_type = type >= SELECTIVE_FILTER_TYPE_I8;
        const unsigned bits = sql_filter_type_width(type) * 8;
        if (signed_type) {
            long long value = std::strtoll(text.c_str(), &end, 0);
            const int64_t min_value = bits == 64 ? std::numeric_limits<int64_t>::min()
                : -(int64_t(1) << (bits - 1));
            const int64_t max_value = bits == 64 ? std::numeric_limits<int64_t>::max()
                : (int64_t(1) << (bits - 1)) - 1;
            if (errno || !end || *end || value < min_value || value > max_value)
                return fail("literal " + text + " does not fit " + sql_filter_type_name(type));
            *literal = static_cast<uint64_t>(static_cast<int64_t>(value));
        } else {
            if (!text.empty() && text[0] == '-')
                return fail("negative literal used with unsigned field");
            unsigned long long value = std::strtoull(text.c_str(), &end, 0);
            const uint64_t max_value = bits == 64 ? UINT64_MAX : (uint64_t(1) << bits) - 1;
            if (errno || !end || *end || value > max_value)
                return fail("literal " + text + " does not fit " + sql_filter_type_name(type));
            *literal = static_cast<uint64_t>(value);
        }
        return true;
    }
    bool keyword(const char *word)
    {
        if (peek().kind == TokenKind::Identifier && lower(peek().text) == word) { take(); return true; }
        return false;
    }
    const LexToken &peek() const { return tokens_[position_]; }
    LexToken take() { return tokens_[position_++]; }
    bool fail(const std::string &message) { *error_ = message; return false; }

    const std::vector<LexToken> &tokens_;
    const std::vector<SqlField> &fields_;
    SqlFilterProgram *program_;
    std::string *error_;
    size_t position_ = 0;
};

uint64_t read_value(const uint8_t *record, uint32_t offset, uint8_t type)
{
    uint64_t value = 0;
    const unsigned width = sql_filter_type_width(type);
    for (unsigned i = 0; i < width; ++i) value |= uint64_t(record[offset + i]) << (i * 8);
    if (type >= SELECTIVE_FILTER_TYPE_I8 && width < 8 && (value & (uint64_t(1) << (width * 8 - 1))))
        value |= ~((uint64_t(1) << (width * 8)) - 1);
    return value;
}

bool compare(uint64_t lhs, uint64_t rhs, uint8_t type, uint8_t op)
{
    if (type >= SELECTIVE_FILTER_TYPE_I8) {
        int64_t a = static_cast<int64_t>(lhs), b = static_cast<int64_t>(rhs);
        switch (op) {
        case SELECTIVE_FILTER_CMP_EQ: return a == b; case SELECTIVE_FILTER_CMP_NE: return a != b;
        case SELECTIVE_FILTER_CMP_LT: return a < b; case SELECTIVE_FILTER_CMP_LE: return a <= b;
        case SELECTIVE_FILTER_CMP_GT: return a > b; default: return a >= b;
        }
    }
    switch (op) {
    case SELECTIVE_FILTER_CMP_EQ: return lhs == rhs; case SELECTIVE_FILTER_CMP_NE: return lhs != rhs;
    case SELECTIVE_FILTER_CMP_LT: return lhs < rhs; case SELECTIVE_FILTER_CMP_LE: return lhs <= rhs;
    case SELECTIVE_FILTER_CMP_GT: return lhs > rhs; default: return lhs >= rhs;
    }
}

} // namespace

uint32_t sql_filter_type_width(uint8_t type)
{
    return type <= SELECTIVE_FILTER_TYPE_I64 ? 1U << (type & 3U) : 0;
}

const char *sql_filter_type_name(uint8_t type)
{
    static const char *names[] = {"u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64"};
    return type < 8 ? names[type] : "invalid";
}

bool parse_sql_schema(const std::string &text, uint32_t record_bytes,
                      std::vector<SqlField> *fields, std::string *error)
{
    fields->clear();
    std::istringstream input(text);
    std::string item;
    while (std::getline(input, item, ',')) {
        item = trim(item);
        size_t colon = item.find(':'), at = item.find('@');
        if (colon == std::string::npos || at == std::string::npos || colon == 0 || at <= colon + 1) {
            *error = "schema fields must use name:type@offset"; return false;
        }
        SqlField field;
        field.name = trim(item.substr(0, colon));
        if (field.name.empty() || find_field(*fields, field.name)) {
            *error = "empty or duplicate schema field: " + field.name; return false;
        }
        if (!parse_type(trim(item.substr(colon + 1, at - colon - 1)), &field.type)) {
            *error = "unsupported type in schema field " + field.name; return false;
        }
        std::string offset_text = trim(item.substr(at + 1));
        errno = 0; char *end = nullptr;
        unsigned long value = std::strtoul(offset_text.c_str(), &end, 0);
        if (errno || !end || *end || value > UINT32_MAX) {
            *error = "invalid offset in schema field " + field.name; return false;
        }
        field.offset = static_cast<uint32_t>(value);
        uint32_t width = sql_filter_type_width(field.type);
        if (field.offset + width > record_bytes) {
            *error = "schema field exceeds record boundary: " + field.name; return false;
        }
        if ((field.offset & 63U) + width > 64) {
            *error = "schema field crosses a 64-byte FPGA beat: " + field.name; return false;
        }
        fields->push_back(field);
    }
    if (fields->empty()) { *error = "schema is empty"; return false; }
    return true;
}

bool compile_filter_sql(const std::string &query, uint32_t record_bytes,
                        const std::vector<SqlField> &fields,
                        SqlFilterProgram *program, std::string *error)
{
    std::vector<LexToken> tokens;
    if (!tokenize(query, &tokens, error)) return false;
    *program = SqlFilterProgram{};
    program->record_bytes = record_bytes;
    program->query = query;
    Parser parser(tokens, fields, program, error);
    return parser.parse();
}

bool evaluate_filter_sql(const SqlFilterProgram &program, const uint8_t *record,
                         size_t record_bytes, bool *selected, uint64_t *projection,
                         std::string *error)
{
    if (!record || record_bytes < program.record_bytes) { *error = "record is too short"; return false; }
    std::vector<bool> results;
    for (const SqlPredicate &p : program.predicates)
        results.push_back(compare(read_value(record, p.offset, p.type), p.literal, p.type, p.comparison));
    std::vector<bool> stack;
    for (uint8_t token : program.tokens) {
        if (token <= SELECTIVE_FILTER_TOKEN_PREDICATE_MAX) {
            if (token >= results.size()) { *error = "invalid predicate token"; return false; }
            stack.push_back(results[token]);
        } else if (token == SELECTIVE_FILTER_TOKEN_NOT) {
            if (stack.empty()) { *error = "invalid NOT token"; return false; }
            stack.back() = !stack.back();
        } else if (token == SELECTIVE_FILTER_TOKEN_AND || token == SELECTIVE_FILTER_TOKEN_OR) {
            if (stack.size() < 2) { *error = "invalid binary token"; return false; }
            bool rhs = stack.back(); stack.pop_back(); bool lhs = stack.back(); stack.pop_back();
            stack.push_back(token == SELECTIVE_FILTER_TOKEN_AND ? lhs && rhs : lhs || rhs);
        } else { *error = "unknown expression token"; return false; }
    }
    if (stack.size() != 1) { *error = "invalid expression stack"; return false; }
    *selected = stack.back();
    *projection = read_value(record, program.projection_offset, program.projection_type);
    return true;
}
