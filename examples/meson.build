root = meson.source_root()
executable('sphere',
           'sphere.cpp',
           root + '/simple-window.cpp',
           dependencies:[
             dep_glm,
             dep_sdl2,
             ],
           cpp_args : [ '-I' + root, '-trigraphs' ],
           link_args : '-pthread',
           link_with: libtrcore,
           install : true)

executable('01_triangles',
           '01_triangles.cpp',
           dependencies:[
             dep_glm,
             ],
           cpp_args : [ '-I' + root, '-trigraphs' ],
           link_with: libtrcore,
           install : true)
