#!/usr/bin/env python3
"""Check GitHub Actions YAML structure and immutable action references."""

import pathlib
import re
import sys

try:
    import yaml
except ImportError as exc:
    raise SystemExit("PyYAML is required (see tools/ci/requirements-ci.txt)") from exc

ROOT = pathlib.Path(__file__).resolve().parents[2]
PATTERN = re.compile(r"^[\w.-]+(?:/[\w.-]+)+@([0-9a-f]{40})$")


def main():
    folder = ROOT / ".github" / "workflows"
    workflows = sorted([*folder.glob("*.yml"), *folder.glob("*.yaml")])
    if not workflows:
        raise SystemExit("No workflow YAML files found")
    deployment_files = []
    for file in workflows:
        # BaseLoader keeps GitHub's literal 'on' key, which YAML 1.1 otherwise
        # treats as the boolean True in some YAML parsers.
        data = yaml.load(file.read_text(encoding="utf-8"), Loader=yaml.BaseLoader)
        if not isinstance(data, dict) or "on" not in data or "jobs" not in data:
            raise SystemExit(f"{file}: missing on/jobs mapping")
        if not isinstance(data["jobs"], dict) or not data["jobs"]:
            raise SystemExit(f"{file}: invalid job definitions")
        for line_number, line in enumerate(file.read_text(encoding="utf-8").splitlines(), 1):
            use_line = re.match(r"^\s*-?\s*uses:\s*(\S+)(.*)$", line)
            if use_line and not use_line.group(1).startswith("./"):
                if not re.search(r"\s+#\s+v\d", use_line.group(2)):
                    raise SystemExit(
                        f"{file}:{line_number}: remote actions need version comments")
        for job_name, job in data["jobs"].items():
            if not isinstance(job, dict):
                raise SystemExit(f"{file}: invalid job: {job_name}")
            for step in job.get("steps", []):
                if not isinstance(step, dict):
                    raise SystemExit(f"{file}: invalid step in {job_name}")
                action = step.get("uses")
                if not action or str(action).startswith("./"):
                    continue
                if not PATTERN.fullmatch(str(action)):
                    raise SystemExit(f"{file}: unpinned action: {action}")
                if str(action).split("@", 1)[0] == "actions/deploy-pages":
                    deployment_files.append(file)
        print(f"PASS: {file.relative_to(ROOT)}")
    if len(deployment_files) != 1:
        raise SystemExit("Exactly one Pages deployment action must exist")
    return 0


if __name__ == "__main__":
    sys.exit(main())
