
project('hitime', 'c',
        license: 'MIT',
        license_files: 'LICENSE',
        version: '1.1.3')
version = meson.project_version()

incdir = include_directories('include')
includes = files('include/hitime.h')
sources = files('src/hitime.c')

# Expected use-case is to build against static library.
hitime = static_library('hitime',
                        sources,
                        include_directories: incdir,
                        install: true)
#hitime = library('hitime',
#                 sources,
#                 include_directories: incdir,
#                 version: version,
#                 soversion: '1',
#                 install: true)
install_headers(includes, subdir: 'hitime')

# Unit tests
e_prove = executable('prove', 'test/bdd.h', 'test/prove.c', include_directories: incdir, link_with: hitime)
test('prove library correctness', e_prove)

# Performance executables
e_perform = executable('perform', 'test/bdd.h', 'test/perform.c', include_directories: incdir, link_with: hitime)
e_cache = executable('cache', 'test/bdd.h', 'test/cache.c', include_directories: incdir, link_with: hitime)

