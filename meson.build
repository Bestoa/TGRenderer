project('tiny-raster', 'cpp', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

libtrcore = shared_library('trcore',
           'tr.cpp',
           'utils.cpp',
           'external/tga/targa.c',
           dependencies:[
             dependency('openmp'),
             dependency('glm'),
             ],
           cpp_args: '-fopenmp',
           c_args: '-xc++',
           install : true)

libtrobj = shared_library('trobj',
           'objs.cpp',
           link_with: libtrcore,
           dependencies:[
             dependency('nlohmann_json'),
             ],
           install : true)

executable('model-renderer',
           'main.cpp',
           'simple-window.cpp',
           dependencies:[
             dependency('SDL2'),
             dependency('glm'),
             ],
           link_args: '-pthread',
           link_with: [ libtrcore, libtrobj ],
           install : true)
