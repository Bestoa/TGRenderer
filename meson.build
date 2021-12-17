project('TopGun Renderer', 'cpp',
  version : '0.2',
  default_options : ['warning_level=3', 'cpp_std=c++14'])

dep_glm = dependency('glm')
dep_sdl2 = dependency('SDL2')
dep_sdl2_image = dependency('SDL2_image')

root = meson.source_root()
include_dir = include_directories('include')
window_src = root + '/src/window.cpp'

subdir('src')
subdir('examples')
