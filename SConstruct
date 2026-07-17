import os
import sys
import rtconfig

if os.getenv('RTT_ROOT'):
    RTT_ROOT = os.getenv('RTT_ROOT')
else:
    RTT_ROOT = os.path.normpath(os.getcwd() + '/../../..')

sys.path = sys.path + [os.path.join(RTT_ROOT, 'tools')]
try:
    from building import *
except:
    print('Cannot found RT-Thread root directory, please check RTT_ROOT')
    print(RTT_ROOT)
    exit(-1)

# ---------------------------------------------------------------------------
# UF2 configuration — AT32F405 in external QSPI flash
# ---------------------------------------------------------------------------
UF2_LOAD_ADDRESS = '0x90000000'
UF2_FAMILY_ID    = '0xf35c900d'
UF2CONV          = os.environ.get('UF2CONV', './tools/uf2conv/uf2conv.py')
FLASH_TARGET     = '/media/{}/CherryUF2/CURRENT.UF2'.format(os.environ.get('USER', ''))

# ---------------------------------------------------------------------------
# Custom command-line options
# ---------------------------------------------------------------------------
AddOption(
    '--flash',
    dest    = 'flash',
    action  = 'store_true',
    default = False,
    help    = 'Copy rtthread.uf2 to the mounted CherryUF2 device after building',
)

# ---------------------------------------------------------------------------
# Build environment
# ---------------------------------------------------------------------------
TARGET = 'rtthread.' + rtconfig.TARGET_EXT

DefaultEnvironment(tools=[])
env = Environment(tools = ['mingw'],
    AS = rtconfig.AS, ASFLAGS = rtconfig.AFLAGS,
    CC = rtconfig.CC, CFLAGS = rtconfig.CFLAGS,
    AR = rtconfig.AR, ARFLAGS = '-rc',
    LINK = rtconfig.LINK, LINKFLAGS = rtconfig.LFLAGS)
env.PrependENVPath('PATH', rtconfig.EXEC_PATH)
env.Append(CCFLAGS=['-Wreturn-type'])

if rtconfig.PLATFORM in ['iccarm']:
    env.Replace(CCCOM = ['$CC $CFLAGS $CPPFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS -o $TARGET $SOURCES'])
    env.Replace(ARFLAGS = [''])
    env.Replace(LINKCOM = env["LINKCOM"] + ' --map project.map')

Export('env')
Export('RTT_ROOT')
Export('rtconfig')

SDK_ROOT = os.path.abspath('./')

if os.path.exists(SDK_ROOT + '/libraries'):
    libraries_path_prefix = SDK_ROOT + '/libraries'
else:
    libraries_path_prefix = os.path.dirname(SDK_ROOT) + '/libraries'

SDK_LIB = libraries_path_prefix
Export('SDK_LIB')

# prepare building environment
objs = PrepareBuilding(env, RTT_ROOT, has_libcpu=False)

# include cmsis
objs.extend(SConscript(os.path.join(libraries_path_prefix, 'CMSIS', 'SConscript')))

# include usb libraries
objs.extend(SConscript(os.path.join(libraries_path_prefix, 'usbotg_library', 'SConscript')))

# include drivers
objs.extend(SConscript(os.path.join(libraries_path_prefix, 'rt_drivers', 'SConscript')))

# make a building
DoBuilding(TARGET, objs)

# ---------------------------------------------------------------------------
# Post-build pipeline: elf -> bin -> uf2 [-> flash]
# ---------------------------------------------------------------------------
OBJCOPY = os.path.join(rtconfig.EXEC_PATH, 'arm-none-eabi-objcopy')

# elf -> bin
bin_target = env.Command(
    'rtthread.bin',
    TARGET,
    '{} -O binary $SOURCE $TARGET'.format(OBJCOPY),
)

# bin -> uf2
uf2_target = env.Command(
    'rtthread.uf2',
    bin_target,
    'python3 {} -c -b {} -f {} -o $TARGET $SOURCE'.format(
        UF2CONV, UF2_LOAD_ADDRESS, UF2_FAMILY_ID,
    ),
)

# uf2 -> flash (only when --flash is passed)
if GetOption('flash'):
    def flash_action(target, source, env):
        src = str(source[0])
        if not os.path.exists(os.path.dirname(FLASH_TARGET)):
            print('ERROR: CherryUF2 device not mounted at {}'.format(
                os.path.dirname(FLASH_TARGET)))
            return 1
        import shutil
        shutil.copy(src, FLASH_TARGET)
        if hasattr(os, 'sync'):
            os.sync()
        print('Flashed {} -> {}'.format(src, FLASH_TARGET))
        return 0

    flash_node = env.Command(
        'flash',           # pseudo-target (not a real file)
        uf2_target,
        flash_action,
    )
    env.AlwaysBuild(flash_node)
    env.Default(flash_node)
else:
    env.Default(uf2_target)
