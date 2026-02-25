#!/usr/bin/env python3
import sys
import os

def main():
    if len(sys.argv) != 3:
        print("Usage: generate_lamejs_header.py <input_js_file> <output_header_file>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    try:
        with open(input_path, 'r', encoding='utf-8') as f:
            js_content = f.read()
    except Exception as e:
        print(f"Error reading {input_path}: {e}")
        sys.exit(1)

    header_content = f"""#ifndef LAME_JS_H
#define LAME_JS_H
#include <Arduino.h>
const char LAME_MIN_JS[] PROGMEM = R"rawliteral(
{js_content}
)rawliteral";
#endif
"""

    try:
        dir_name = os.path.dirname(output_path)
        if dir_name:
            os.makedirs(dir_name, exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"Generated {output_path} from {input_path}")
    except Exception as e:
        print(f"Error writing {output_path}: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
