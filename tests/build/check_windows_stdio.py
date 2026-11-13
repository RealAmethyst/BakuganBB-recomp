"""Check only the original windowed-subsystem stdio fixture, never the host."""

from pathlib import Path
import subprocess
import sys


executable = Path(sys.argv[1]).resolve()
if executable.name.lower() != "windowed_stdio_probe.exe":
    raise SystemExit("Only the original windowed stdio fixture may be run")
result = subprocess.run(
    [str(executable)], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
    timeout=15, check=True,
)
if sorted(result.stdout.splitlines()) != [
    "BRAWLERS_STATUS:thread_created", "synthetic stdout"
]:
    raise SystemExit("Windows GUI-subsystem redirected streams did not match")
print("Windows GUI-subsystem stdout and stderr are available through the launcher pipe")
