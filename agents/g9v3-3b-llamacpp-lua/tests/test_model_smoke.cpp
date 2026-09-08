#include "llama_runtime.hpp"
#include <iostream>
#ifndef G9_MODEL_PATH
#error G9_MODEL_PATH is required
#endif
int main(){try{g9::ModelConfig c;c.context_size=1024;c.batch_size=128;c.threads=1;g9::LlamaRuntime r(G9_MODEL_PATH,c,[](std::string_view){});g9::GenerationConfig g;g.max_tokens=8;auto out=r.generate("<s><|im_start|>user\nHi<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n",g);return(out.completion_tokens>0||out.hit_eog)?0:1;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
