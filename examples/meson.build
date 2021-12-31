executable('sphere',
           'sphere.cpp',
           window_src,
           dependencies : [
             dep_glm,
             dep_sdl2,
             ],
           include_directories : include_dir,
           link_args : '-pthread',
           link_with : libtrcore,
           install : true)

executable('01_triangles',
           '01_triangles.cpp',
           dependencies : [
             dep_glm,
             ],
           include_directories : include_dir,
           link_with : libtrcore,
           install : true)
