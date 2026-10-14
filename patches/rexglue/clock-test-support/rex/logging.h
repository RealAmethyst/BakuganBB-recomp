#pragma once
extern unsigned probe_warning_count;
#define REXLOG_WARN(...) (++probe_warning_count)
