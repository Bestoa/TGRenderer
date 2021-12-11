project('ToyRenderer', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

dep_glm = dependency('glm')
dep_sdl2 = dependency('SDL2')
dep_sdl2_image = dependency('SDL2_image')

libtrcore = shared_library('trcore',
           'tr.cpp',
           'utils.cpp',
           dependencies:[
             dep_glm,
             ],
           c_args: '-xc++',
           install : true)

libtrobj = shared_library('trobj',
           'objs.cpp',
           link_with: libtrcore,
           dependencies:[
             dep_glm,
             ],
           install : true)

executable('ToyRrenderer',
           'main.cpp',
           'simple-window.cpp',
           dependencies:[
             dep_glm,
             dep_sdl2,
             dep_sdl2_image,
             ],
           link_args: '-pthread',
           link_with: [ libtrcore, libtrobj ] ,
           install : true)

subdir('examples')
