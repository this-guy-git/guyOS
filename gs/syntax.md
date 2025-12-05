# GScript syntax ideas

Directives:
- `#GUYOS_cmd;`    marks a command entry point
- `#start main;`    specifies entry function
- `#include "io.gh";` include header/stdlib

Functions:
- `<type> name(params) { ... }` returns type
- Supported types (planned): `int`, `str`, `void`

Variables:
- `<type> name = value;`
- `<type> name;` // default init

Headers (`.gh`):
- Begin with `#GUYOS_header`
- Contain declarations/definitions, no entry point
- End with `END`

Control/ops (planned):
- `if (expr) { ... } else { ... }`
- Comparison `==`, `!=`, `<`, `>` (at least for int/str equality first)
- String concat with `+`

Entry/exit conventions:
- `main` returns int exit codes: 0 success, 1 error (with optional message), 2 restart, -1 request exit shell (reserved/subject to change)
- `return code, "message";` allowed for code==1 or 2 to attach text

Notes:
- This file is descriptive; the current compiler only supports `print/println/exit`. Implementation will expand toward this grammar.
