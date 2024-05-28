
project('hitime', 'c',
        license: 'MIT',
        license_files: 'LICENSE',
        version: '1.1.3')
version = '1.1.3'

incdir = include_directories('include')
includes = files('include/hitime.h', 'include/hitime_extra.h')
sources = files('src/hitime.c', 'src/hitime_extra.c')

hitime = library('hitime',
                 sources,
                 include_directories: incdir,
                 version: version,
                 soversion: '1',
                 install: true)
install_headers(includes, subdir: 'hitime')

# Unit tests
e_prove = executable('prove', 'test/bdd.h', 'test/prove.c', include_directories: incdir, link_with: hitime)
test('prove library correctness', e_prove)

# Performance executables
e_perform = executable('perform', 'test/bdd.h', 'test/perform.c', include_directories: incdir, link_with: hitime)
e_cache = executable('cache', 'test/bdd.h', 'test/cache.c', include_directories: incdir, link_with: hitime)

