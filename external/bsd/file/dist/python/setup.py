# Python distutils build script for magic extension
from distutils.core import setup, Extension

magic_module = Extension('magic',
    libraries = ['magic'],
    library_dirs = ['./','../','../src','/usr/lib/'],
    include_dirs = ['./','../','../src','/usr/include/'],
    sources = ['py_magic.c'])

setup (name = 'Magic file extensions',
    version = '0.1',
    author = 'Brett Funderburg',
    author_email = 'brettf@deepfile.com',
    license = 'BSD',
    description = 'libmagic python bindings',
    ext_modules = [magic_module])
