static const char* coreModuleSource =
"fn map(list, func) {\n"
" var newList = []\n" 
" for item in list newList << func(item)\n"
" return newList }\n"
"\n"
"fn filter(list, func) { var result = []; for i in list if func(i) then result << i; return result; }"
"\n"
"enum Type {Nil, Bool, Number, DateTime, String, Upvalue, Function, Native, Closure, List, Class, Instance, Method, Enum }"
"enum Keys { 	Enter = 13, 	Escape = 27,     Space = 32,     Exclamation, 	DoubleQuote, 	Number, 	DollarSign, 	Percent, 	Ampersand, 	SingleQuote, 	LeftParenthesis, 	RightParenthesis, 	Asterisk, 	Plus, 	Comma, 	Minus, 	Period, 	Slash, 	Zero, 	One, 	Two, 	Three, 	Four, 	Five, 	Six, 	Seven, 	Eight, 	Nine, 	Colon, 	Semicolon, 	LessThan, 	Equals, 	GreaterThan, 	QuestionMark, 	AtSign,     A,     B,     C,     D,     E,     F,     G,     H,     I,     J,     K,     L,     M,     N,     O,     P,     Q,     R,     S,     T,     U,     V,     W,     X,     Y,     Z,     LeftBracket,     Backslash,     RightBracket,     Caret,     Underscore,     Backtick,     a,     b,     c,     d,     e,     f,     g,     h,     i,     j,     k,     l,     m,     n,     o,     p,     q,     r,     s,     t,     u,     v,     w,     x,     y,     z, 	LeftBrace, 	Pipe, 	RightBrace, 	Tilde, 	Delete, 	LeftArrow, 	RightArrow, 	UpArrow, 	DownArrow, 	PageUp, 	PageDown, 	Home, 	End }";

