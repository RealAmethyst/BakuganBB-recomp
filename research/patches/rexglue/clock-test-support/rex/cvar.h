#pragma once
#define REXCVAR_DECLARE(type, name) extern type name
#define REXCVAR_DEFINE_BOOL(name, value, category, description) bool name = value
#define REXCVAR_GET(name) name
