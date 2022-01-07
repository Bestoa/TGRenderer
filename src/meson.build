libtrcore = shared_library('trcore',
           'tr.cpp',
           'buffer.cpp',
           'texture.cpp',
           'program.cpp',
           'utils.cpp',
           dependencies : [
             dep_glm,
             ],
           link_args : '-pthread',
           include_directories : include_dir,
           install : true)

libtrobj = shared_library('trobj',
           'objs.cpp',
           link_with : libtrcore,
           dependencies : [
             dep_glm,
             ],
           include_directories : include_dir,
           install : true)

executable('TGRenderer',
           'main.cpp',
           window_src,
           dependencies : [
             dep_glm,
             dep_sdl2,
             dep_sdl2_image,
             ],
           include_directories : include_dir,
           link_args : '-pthread',
           link_with : [ libtrcore, libtrobj ] ,
           install : true)

