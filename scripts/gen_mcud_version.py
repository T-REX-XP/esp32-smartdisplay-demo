Import("env")
import json
import os

project_dir = env["PROJECT_DIR"]
json_path = os.path.join(project_dir, "mcud-version.json")
out_path = os.path.join(project_dir, "src", "router", "mcud_version.h")

with open(json_path, "r", encoding="utf-8") as f:
    j = json.load(f)

host = j.get("components", {}).get("host", "mcudd")
fw = j.get("components", {}).get("firmware", "esp32-router")

body = f"""/* Auto-generated from mcud-version.json — do not edit. */
#ifndef MCUD_VERSION_H
#define MCUD_VERSION_H

#define MCUD_RDCP_VERSION {j['rdcp']}u
#define MCUD_STACK_VERSION "{j['stack']}"
#define MCUD_STACK_RELEASE {j['release']}u
#define MCUD_PAGES_SCHEMA {j['pages_schema']}u
#define MCUD_COMPONENT_HOST "{host}"
#define MCUD_COMPONENT_FIRMWARE "{fw}"

#endif
"""

with open(out_path, "w", encoding="utf-8") as f:
    f.write(body)

print(f"Generated {out_path} from {json_path}")
