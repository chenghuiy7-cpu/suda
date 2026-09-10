#include "sql_filter_compiler.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

static void store(void *record, size_t offset, uint64_t value, size_t width)
{
    std::memcpy(static_cast<uint8_t *>(record) + offset, &value, width);
}

int main()
{
    std::vector<SqlField> fields;
    std::string error;
    assert(parse_sql_schema(
        "id:u32@0, quantity:u8@4, price:u64@8, delta:i32@24",
        128, &fields, &error));
    SqlFilterProgram program;
    assert(compile_filter_sql(
        "SELECT quantity FROM lineitem WHERE "
        "(quantity > 10 AND price <= 500) OR (id = 103 AND delta < -1)",
        128, fields, &program, &error));
    assert(program.predicates.size() == 4 && program.tokens.size() == 7);
    struct Case { uint32_t id; uint8_t quantity; uint64_t price; int32_t delta; bool selected; };
    const Case cases[] = {
        {100, 3, 100, -9, false}, {101, 17, 400, 1, true},
        {102, 9, 200, -4, false}, {103, 24, 900, -2, true}};
    for (const Case &item : cases) {
        uint8_t record[128] = {};
        store(record, 0, item.id, 4); store(record, 4, item.quantity, 1);
        store(record, 8, item.price, 8); store(record, 24, uint32_t(item.delta), 4);
        bool selected = false; uint64_t projection = 0;
        assert(evaluate_filter_sql(program, record, sizeof(record),
                                   &selected, &projection, &error));
        assert(selected == item.selected && projection == item.quantity);
    }
    SqlFilterProgram bad;
    assert(!compile_filter_sql("SELECT missing FROM t WHERE quantity > 1",
                               128, fields, &bad, &error));
    assert(!parse_sql_schema("cross:u64@60", 128, &fields, &error));
    std::cout << "SQL filter compiler tests passed\n";
}
