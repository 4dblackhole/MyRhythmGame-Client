#pragma once
#include <string>
#include <string_view>
#include <vector>
struct RecordCondition
{
    std::string sql;
    std::vector<std::string> values;
    static RecordCondition Parse(std::string_view condition);
};
