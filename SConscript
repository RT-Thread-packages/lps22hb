from building import *
Import('rtconfig')

src   = []
cwd   = GetCurrentDir()

# add lps22hb src files.
src += Glob('libraries/lps22hb.c')
src += Glob('libraries/lps22hb_reg.c')

if GetDepend('PKG_LPS22HB_USING_SENSOR_V1'):
    src += Glob('st_lps22hb_sensor_v1.c')

# add lps22hb include path.
path  = [cwd, cwd + '/libraries']

# add src and include to group.
group = DefineGroup('lps22hb', src, depend = ['PKG_USING_LPS22HB'], CPPPATH = path)

Return('group')
