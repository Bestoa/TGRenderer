cpp_args = []
if get_option('buffer_lock')
  cpp_args += ['-D__NEED_BUFFER_LOCK__=1' ]
else
  cpp_args += ['-D__NEED_BUFFER_LOCK__=0' ]
endif

thread_dep = dependency('threads', required : true)

libtrcore = shared_library('trcore',
           'core/trcore.cpp',
           'core/buffer.cpp',
           'core/texture.cpp',
           'core/program.cpp',
           'core/skybox.cpp',
           'core/utils.cpp',
           dependencies : [
             dep_glm,
             thread_dep,
             ],
           cpp_args : cpp_args,
           include_directories : include_dir,
           install : true)

