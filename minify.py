Import("env")
env.Execute("$PYTHONEXE -m pip install minify-html")

from pathlib import Path
import minify_html
import gzip
import os
import tempfile

def minify(source, target, env):
  #html_dir = Path("./html/converter.htm")
  #for html_filepath in [html_dir]:
  html_dir = Path("./html")
  for html_filepath in html_dir.iterdir():
    html_filename = html_filepath.name
    html = html_filepath.read_text()
    html_minified = minify_html.minify(html, minify_js=True, minify_css=True, keep_input_type_text_attr=True)
    with gzip.open(f'./html_minified/{html_filename}.gz', 'wb') as fgz:
      fgz.write(html_minified.encode("utf-8"))

env.AddPostAction("buildfs", minify)

