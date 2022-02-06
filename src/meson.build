libtrcore = shared_library('trcore',
           'trcore.cpp',
           'buffer.cpp',
           'texture.cpp',
           'program.cpp',
           'skybox.cpp',
           'utils.cpp',
           dependencies : [
             dep_glm,
             ],
           link_args : '-pthread',
           include_directories : include_dir,
           install : true)

executable('TGRenderer',
           'main.cpp',
           'objs.cpp',
           window_src,
           dependencies : [
             dep_glm,
             dep_sdl2,
             ],
           include_directories : include_dir,
           link_args : '-pthread',
           link_with : [ libtrcore ],
           install : true)

