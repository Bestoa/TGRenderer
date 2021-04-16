project('tiny-raster', 'cpp', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

libtr = shared_library('tr',
           'tr.cpp',
           'utils.cpp',
           'external/tga/targa.c',
           dependencies:[
             dependency('openmp'),
             ],
           cpp_args: '-fopenmp',
           c_args: '-xc++',
           install : true)

executable('model-renderer',
           'main.cpp',
           'simple-window.cpp',
           dependencies:[
             dependency('SDL2'),
             ],
           link_args: '-pthread',
           link_with: libtr,
           install : true)
