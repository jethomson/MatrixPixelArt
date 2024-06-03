Import("env")
env.Execute("$PYTHONEXE -m pip install minify-html")

from pathlib import Path
import minify_html

def minify(source, target, env):
  dir = Path("./html")
  for filename in dir.iterdir():
    html = filename.read_text()
    try:
      minified = minify_html.minify(html, minify_js=True, minify_css=True, keep_input_type_text_attr=True)
      with open(f'./html_minified/{filename.name}', 'w') as fout:
        print(minified, file=fout, end='')
    except SyntaxError as e:
      print(e)

env.AddPostAction("buildfs", minify)

