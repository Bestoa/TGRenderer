executable('01_triangles',
           '01_triangles.cpp',
           dependencies : [
             dep_glm,
             ],
           include_directories : include_dir,
           link_with : libtrcore,
           install : true)

executable('earth',
           'earth.cpp',
           '../src/helper/window.cpp',
           dependencies : [
             dep_glm,
             dep_sdl2,
             ],
           include_directories : include_dir,
           link_with : libtrcore,
           install : true)

executable('cornell',
           'cornell.cpp',
           '../src/helper/window.cpp',
           '../src/helper/objs.cpp',
           dependencies : [
             dep_glm,
             dep_sdl2,
             ],
           include_directories : include_dir,
           link_with : libtrcore,
           install : true)

executable('solar_system',
           'solar_system.cpp',
           '../src/helper/window.cpp',
           dependencies : [
             dep_glm,
             dep_sdl2,
             ],
           include_directories : include_dir,
           link_with : libtrcore,
           install : true)
