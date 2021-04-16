project('tiny-raster', 'cpp', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

executable('tiny-raster',
           'tr.cpp',
           'main.cpp',
           'utils.cpp',
           'simple-window.cpp',
           'external/tga/targa.c',
           dependencies:[
             dependency('SDL2'),
             dependency('openmp'),
             ],
           c_args: '-xc++',
           cpp_args: '-fopenmp',
           install : true)
