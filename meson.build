project('ToyRenderer', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

dep_glm = dependency('glm')
dep_json = dependency('nlohmann_json')
dep_sdl2 = dependency('SDL2')

libtrcore = shared_library('trcore',
           'tr.cpp',
           'utils.cpp',
           dependencies:[
             dependency('openmp'),
             dep_glm,
             ],
           cpp_args: '-fopenmp',
           c_args: '-xc++',
           install : true)

libtrobj = shared_library('trobj',
           'objs.cpp',
           link_with: libtrcore,
           dependencies:[
             dep_glm,
             dep_json,
             ],
           install : true)

executable('ToyRrenderer',
           'main.cpp',
           'simple-window.cpp',
           dependencies:[
             dep_glm,
             dep_sdl2,
             ],
           link_args: '-pthread',
           link_with: [ libtrcore, libtrobj ] ,
           install : true)

subdir('examples')
