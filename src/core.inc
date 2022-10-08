static const char* coreModuleSource =
"fn map(list, func) {\n"
" var newList = []\n" 
" for item in list add(newList, func(item))\n"
" return newList }\n";