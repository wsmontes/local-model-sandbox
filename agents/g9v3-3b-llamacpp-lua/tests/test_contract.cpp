#include "contract.hpp"
#include "test_support.hpp"
int main(){using namespace g9;CHECK_EQ(parse_request(parse_json(R"({"prompt":"fix"})")).prompt,std::string("fix"));CHECK_THROWS(parse_request(parse_json("{}")));CHECK_THROWS(parse_request(parse_json(R"({"prompt":""})")));CHECK_THROWS(parse_request(parse_json(R"({"prompt":12})")));CHECK_THROWS(parse_request(parse_json(R"({"prompt":"x","context":{}})")));return test_support::finish();}
