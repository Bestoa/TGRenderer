project('TopGun Renderer', 'cpp',
  version : '0.4',
  default_options : ['warning_level=3', 'cpp_std=c++11', 'buildtype=release'])

dep_glm = dependency('glm')
dep_sdl2 = dependency('SDL2')

root = meson.source_root()
include_dir = [ include_directories('include'), include_directories('stb') ]
window_src = root + '/src/window.cpp'

subdir('src')
subdir('examples')
