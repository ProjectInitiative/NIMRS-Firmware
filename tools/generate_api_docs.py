import os
import re
import sys


def parse_comment(comment):
    lines = comment.split("\n")
    api_data = {}
    params = []
    success = []
    errors = []

    for line in lines:
        line = line.strip()
        if line.startswith("*"):
            line = line[1:].strip()

        if line.startswith("@api "):
            match = re.match(r"@api\s+\{(\w+)\}\s+(\S+)\s*(.*)", line)
            if match:
                api_data["method"] = match.group(1)
                api_data["path"] = match.group(2)
                api_data["title"] = match.group(3)
        elif line.startswith("@apiName "):
            api_data["name"] = line.split(" ", 1)[1]
        elif line.startswith("@apiGroup "):
            api_data["group"] = line.split(" ", 1)[1]
        elif line.startswith("@apiDescription "):
            api_data["description"] = line.split(" ", 1)[1]
        elif line.startswith("@apiParam "):
            match = re.match(
                r"@apiParam\s+(?:\([^\)]+\)\s+)?\{([^}]+)\}\s+(\S+)\s*(.*)", line
            )
            if match:
                params.append(
                    {
                        "type": match.group(1),
                        "name": match.group(2),
                        "desc": match.group(3),
                    }
                )
        elif line.startswith("@apiSuccess "):
            match = re.match(r"@apiSuccess\s+\{([^}]+)\}\s*(.*)", line)
            if match:
                rest = match.group(2)
                name = ""
                desc = rest
                if " " in rest:
                    parts = rest.split(" ", 1)
                    name = parts[0]
                    desc = parts[1]
                else:
                    name = rest
                    desc = ""

                success.append({"type": match.group(1), "name": name, "desc": desc})
        elif line.startswith("@apiError "):
            match = re.match(r"@apiError\s+\{([^}]+)\}\s*(.*)", line)
            if match:
                rest = match.group(2)
                name = ""
                desc = rest
                if " " in rest:
                    parts = rest.split(" ", 1)
                    name = parts[0]
                    desc = parts[1]
                else:
                    name = rest
                    desc = ""

                errors.append({"type": match.group(1), "name": name, "desc": desc})

    if "method" in api_data and "path" in api_data:
        api_data["params"] = params
        api_data["success"] = success
        api_data["errors"] = errors
        return api_data
    return None


def scan_file(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    comments = re.findall(r"/\*\*(.*?)\*/", content, re.DOTALL)
    apis = []
    for comment in comments:
        if "@api " in comment:
            data = parse_comment(comment)
            if data:
                apis.append(data)
    return apis


def slugify(text):
    text = text.lower()
    text = re.sub(r"[^a-z0-9]+", "-", text)
    text = text.strip("-")
    return text


def generate_markdown(apis, output_file):
    groups = {}
    for api in apis:
        g = api.get("group", "General")
        if g not in groups:
            groups[g] = []
        groups[g].append(api)

    with open(output_file, "w", encoding="utf-8") as f:
        f.write("# API Documentation\n\n")
        f.write(
            "This documentation is automatically generated from the source code.\n\n"
        )

        sorted_groups = sorted(groups.keys())

        f.write("## Table of Contents\n\n")
        for g in sorted_groups:
            f.write(f"- [{g}](#{slugify(g)})\n")
            for api in groups[g]:
                title = api.get("title", f"{api['method']} {api['path']}")
                if not title:
                    title = f"{api['method']} {api['path']}"
                f.write(f"  - [{title}](#{slugify(title)})\n")
        f.write("\n")

        for g in sorted_groups:
            f.write(f"## {g}\n\n")
            for api in groups[g]:
                title = api.get("title", f"{api['method']} {api['path']}")
                if not title:
                    title = f"{api['method']} {api['path']}"

                f.write(f"### {title}\n\n")
                f.write(f'`{api["method"]} {api["path"]}`\n\n')

                if "description" in api:
                    f.write(f'{api["description"]}\n\n')

                if api["params"]:
                    f.write("#### Parameters\n\n")
                    f.write("| Name | Type | Description |\n")
                    f.write("| --- | --- | --- |\n")
                    for p in api["params"]:
                        f.write(f'| {p["name"]} | {p["type"]} | {p["desc"]} |\n')
                    f.write("\n")

                if api["success"]:
                    f.write("#### Success Response\n\n")
                    f.write("| Type | Field | Description |\n")
                    f.write("| --- | --- | --- |\n")
                    for s in api["success"]:
                        f.write(f'| {s["type"]} | {s["name"]} | {s["desc"]} |\n')
                    f.write("\n")

                if api["errors"]:
                    f.write("#### Error Response\n\n")
                    f.write("| Type | Field | Description |\n")
                    f.write("| --- | --- | --- |\n")
                    for e in api["errors"]:
                        f.write(f'| {e["type"]} | {e["name"]} | {e["desc"]} |\n')
                    f.write("\n")

                f.write("---\n\n")


def main():
    src_dir = "src"
    output_file = "docs/API.md"

    # Ensure docs dir exists
    if not os.path.exists("docs"):
        os.makedirs("docs")

    all_apis = []
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith(".cpp") or file.endswith(".h"):
                filepath = os.path.join(root, file)
                apis = scan_file(filepath)
                if apis:
                    print(f"Scanned {filepath}: Found {len(apis)} endpoints")
                    all_apis.extend(apis)

    all_apis.sort(key=lambda x: (x.get("group", ""), x["path"]))

    print(f"Found total {len(all_apis)} API endpoints.")
    generate_markdown(all_apis, output_file)
    print(f"Documentation generated at {output_file}")


if __name__ == "__main__":
    main()
