import re
import os

with open('src/WebAssets.h', 'r') as f:
    content = f.read()

def extract(name, filename):
    pattern = r'const char ' + name + r'\[\] PROGMEM = R"rawliteral\((.*?)\)rawliteral";'
    match = re.search(pattern, content, re.DOTALL)
    if match:
        with open(os.path.join('temp_web', filename), 'w') as out:
            out.write(match.group(1).strip())
        print(f"Extracted {filename}")
    else:
        print(f"Failed to extract {filename}")

extract('INDEX_HTML', 'index.html')
extract('STYLE_CSS', 'style.css')
extract('APP_JS', 'app.js')

with open('temp_web/lame.min.js', 'w') as f:
    f.write('// mock lamejs')
