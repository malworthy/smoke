static const char* coreModuleSource =
"fn map(list, func) {\n"
" var newList = []\n" 
" for item in list newList << func(item)\n"
" return newList }\n"
"\n"
"fn filter(list, func) { var result = []; for i in list if func(i) then result << i; return result; }"
"\n"
"enum Type {Nil, Bool, Number, DateTime, String, Upvalue, Function, Native, Closure, List, Class, Instance, Method, Enum }";
