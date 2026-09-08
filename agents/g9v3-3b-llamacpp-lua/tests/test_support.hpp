#pragma once
#include <iostream>
namespace test_support { inline int failures=0,checks=0; inline void check(bool ok,const char*e,const char*f,int l){++checks;if(!ok){++failures;std::cerr<<f<<':'<<l<<" CHECK failed: "<<e<<'\n';}} inline int finish(){return failures?1:0;} }
#define CHECK(x) ::test_support::check(static_cast<bool>(x),#x,__FILE__,__LINE__)
#define CHECK_EQ(a,b) do{auto _a=(a);auto _b=(b);::test_support::check(_a==_b,#a " == " #b,__FILE__,__LINE__);}while(0)
#define CHECK_THROWS(x) do{bool _t=false;try{(void)(x);}catch(...){_t=true;}::test_support::check(_t,"throws(" #x ")",__FILE__,__LINE__);}while(0)
