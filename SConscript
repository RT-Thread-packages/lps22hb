from building import *
Import('rtconfig')

src   = []
cwd   = GetCurrentDir()

# add lps22hb src files.
src += Glob('sensor_st_lps22hb.c')
src += Glob('libraries/lps22hb.c')
src += Glob('libraries/lps22hb_reg.c')

# add lps22hb include path.
path  = [cwd, cwd + '/libraries']

# add src and include to group.
group = DefineGroup('lps22hb', src, depend = ['PKG_USING_LPS22HB'], CPPPATH = path)

Return('group')
