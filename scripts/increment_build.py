from pathlib import Path

Import("env")

project_dir = Path(env["PROJECT_DIR"])
counter_file = project_dir / ".fw_build_number"

try:
    current = int(counter_file.read_text(encoding="utf-8").strip())
except (FileNotFoundError, ValueError):
    current = 0

build_number = current + 1
counter_file.write_text(f"{build_number}\n", encoding="utf-8")

env.Append(CPPDEFINES=[("FW_BUILD_NUMBER", build_number)])

print(f"[fw] build number: {build_number}")
