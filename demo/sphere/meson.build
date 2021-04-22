root = meson.source_root()
executable('sphere',
           'main.cpp',
           root + '/simple-window.cpp',
           dependencies:[
             dep_glm,
             dep_sdl2,
             ],
           cpp_args : [ '-I' + root, '-trigraphs' ],
           link_args : '-pthread',
           link_with: libtrcore,
           install : true)
