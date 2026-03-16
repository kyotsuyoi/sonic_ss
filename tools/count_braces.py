path = r"e:\Sega Saturn - joengine\joengine\Projects\game\src\class\ui_control.c"
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    s = f.read()
open_count = s.count('{')
close_count = s.count('}')
print(f"open:{open_count} close:{close_count}")
