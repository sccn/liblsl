# Configuration file for the Sphinx documentation builder.

# -- Project information -----------------------------------------------------
import subprocess, os

project = 'liblsl'
copyright = '2019, Christian Kothe, MIT'
author = 'Christian Kothe, David Medine, Chadwick Boulay, Matthew Grivich, Tristan Stenner'

# The full version, including alpha/beta/rc tags
try:
    gitver = subprocess.run(['git', 'describe', '--tags', 'HEAD'], capture_output=True)
    release = gitver.stdout.strip().decode()
except:
    release = '1.14'

# -- General configuration ---------------------------------------------------

extensions = [
    'breathe',
    'sphinx.ext.intersphinx',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
master_doc = 'index' # for Sphinx < 2.0

# html_static_path = ['_static']

breathe_projects = { 'liblsl': 'liblsl_api/xml/'}
breathe_default_project = 'liblsl'

# The reST default role (used for this markup: `text`) to use for all documents.
default_role = 'cpp:any'

# If true, '()' will be appended to :func: etc. cross-reference text.
add_function_parentheses = True
highlight_language = 'c++'
primary_domain = 'cpp'

# intersphinx
intersphinx_mapping = {
    'lsl': ('https://labstreaminglayer.readthedocs.io/', None),
    }


read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'
if read_the_docs_build:
    subprocess.call(['doxygen', 'Doxyfile_API'])
