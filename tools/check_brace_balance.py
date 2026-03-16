import re
import pathlib

path = pathlib.Path(r"e:\Sega Saturn - joengine\joengine\Projects\game\src\class\ui_control.c")
text = path.read_text(encoding='utf-8', errors='ignore').splitlines()
start = None
end = None
for i,line in enumerate(text, start=1):
    if start is None and re.match(r"void\s+ui_control_draw_character_menu\b", line):
        start = i
    if start is not None and re.match(r"void\s+ui_control_draw_loading\b", line):
        end = i
        break

if start is None or end is None:
    print('Could not find function boundaries')
    raise SystemExit(1)

brace = 0
max_brace = 0
max_line = start
for i in range(start-1, end-1):
    line = text[i]
    brace += line.count('{')
    brace -= line.count('}')
    if brace > max_brace:
        max_brace = brace
        max_line = i + 1
    if brace < 0:
        print(f"Brace negative at line {i+1}")
        break

print(f"Function ui_control_draw_character_menu brace delta before ui_control_draw_loading: {brace}")
print(f"Max depth {max_brace} at line {max_line}")
print(f"start line: {start}, end line: {end}")
