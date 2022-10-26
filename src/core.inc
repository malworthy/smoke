static const char* coreModuleSource =
"fn map(list, func) {\n"
" var newList = []\n" 
" for item in list add(newList, func(item))\n"
" return newList }\n"
"\n"
"fn filter(list, func) { var result = []; for i in list if func(i) then add(result, i); return result; }";