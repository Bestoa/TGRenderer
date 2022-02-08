project('TopGun Renderer', 'cpp',
  version : '0.4',
  default_options : ['warning_level=3', 'cpp_std=c++11', 'buildtype=release'])

dep_glm = dependency('glm')
dep_sdl2 = dependency('SDL2')

include_dir = [ include_directories('include'), include_directories('stb') ]

subdir('src')
subdir('examples')

executable('TGRenderer',
           'src/main.cpp',
           'src/helper/objs.cpp',
           'src/helper/window.cpp',
           dependencies : [
             dep_glm,
             dep_sdl2,
             ],
           include_directories : include_dir,
           link_with : [ libtrcore ],
           install : true)

