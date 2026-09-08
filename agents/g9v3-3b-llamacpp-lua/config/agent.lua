local agent = {}
agent.model = { path = "models/ai9stars_G9v3-3B-Q4_K_M.gguf", context_size = 8192, batch_size = 512, gpu_layers = 0, threads = 0 }
agent.generation = { thinking = false, max_tokens = 768, temperature = 0.7, top_p = 0.95, top_k = 40, min_p = 0.0, repeat_penalty = 1.0, repeat_last_n = 64, seed = -1 }
agent.loop = { max_steps = 32, max_tool_calls_per_step = 8, max_tool_result_bytes = 65536 }
agent.workspace = { default_access = "rw", deny = { ".git/**", ".env", ".env.*", "**/*.pem", "**/*.key", "models/**", "third_party/**" } }
agent.guardrails = { profile = "balanced", command_sandbox = "strict" }
agent.system_prompt = [[You are G9 Local Coding Agent, an offline software-engineering agent operating only inside the granted workspace.
Inspect before editing. Prefer small, reviewable changes. Use search and read tools to establish context, apply_patch for structured edits, and run_command for local builds/tests when policy permits.
Never invent tool results. Never claim a command or test ran unless a tool result confirms it. Respect denied paths and approvals. Do not attempt network access.
When work is complete, summarize the change, tests run, and any remaining uncertainty concisely.]]
local function escape_json(s) return '"' .. s:gsub('[%z\1-\31\\"]', function(c) local map={['"']='\\"',['\\']='\\\\',['\b']='\\b',['\f']='\\f',['\n']='\\n',['\r']='\\r',['\t']='\\t'}; return map[c] or string.format('\\u%04x',string.byte(c)) end) .. '"' end
local function is_array(t) local n=#t;for k,_ in pairs(t) do if type(k)~='number' or k<1 or k>n or k%1~=0 then return false end end;return true end
local function json(v) local tv=type(v);if tv=='nil' then return 'null' end;if tv=='boolean' or tv=='number' then return tostring(v) end;if tv=='string' then return escape_json(v) end;if tv~='table' then error('unsupported JSON value in prompt renderer') end;local out={};if is_array(v) then for i=1,#v do out[#out+1]=json(v[i]) end;return '['..table.concat(out,',')..']' end;local keys={};for k,_ in pairs(v) do if type(k)~='string' then error('JSON object key must be a string') end;keys[#keys+1]=k end;table.sort(keys);for _,k in ipairs(keys) do out[#out+1]=escape_json(k)..':'..json(v[k]) end;return '{'..table.concat(out,',')..'}' end
function agent.before_prompt(request) return request end
function agent.build_messages(request) return { {role="system",content=agent.system_prompt}, {role="user",content=request.prompt} } end
function agent.generation_settings(_request) return agent.generation end
local function message(role,content) return '<|im_start|>'..role..'\n'..content..'<|im_end|>\n' end
function agent.render_prompt(messages,generation,tools,history) local out={'<s>'};local first=false;for _,m in ipairs(messages) do local content=m.content;if m.role=='system' and not first and #tools>0 then content=content..'\n\n<tools>'..json(tools)..'</tools>';first=true end;out[#out+1]=message(m.role,content) end;for _,h in ipairs(history) do if h.kind=='assistant' then out[#out+1]=message('assistant',h.content) elseif h.kind=='tool' then out[#out+1]=message('tool',h.content) end end;out[#out+1]='<|im_start|>assistant\n<think>\n';if not generation.thinking then out[#out+1]='\n</think>\n\n' end;return table.concat(out) end
function agent.after_response(response) return response end
return agent
