#include "json.hpp"
#include "test_support.hpp"
int main(){using namespace g9;CHECK(parse_json("null").is_null());CHECK(parse_json("true").as_bool());CHECK_EQ(parse_json("-12.5e2").as_number(),-1250.0);CHECK_EQ(parse_json("\"\\uD83D\\uDE80\"").as_string(),std::string("🚀"));CHECK_THROWS(parse_json("{\"a\":1,\"a\":2}"));CHECK_THROWS(parse_json("01"));CHECK_THROWS(parse_json("true false"));CHECK_THROWS(parse_json("\"\\uD800\""));CHECK_EQ(serialize_json(parse_json(R"({"a":[1,true,null,"x"]})")),std::string(R"({"a":[1,true,null,"x"]})"));return test_support::finish();}
