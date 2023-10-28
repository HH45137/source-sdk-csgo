#! /usr/bin/env python

from waflib import Build, Context, Logs
import os

VERSION = '0.01'
APPNAME = 'csgo-src'
top = '.'
default_prefix = '../game' # Waf uses it to set default prefix

Context.Context.line_just = 55 # should fit for everything on 80x26

SubProjects = [
	'appframework',
	'bitmap',
	'bonesetup',
	'engine/voice_codecs/celt',
	'engine/voice_codecs/celt/source',
	'engine/voice_codecs/minimp3',
	'interfaces',
	'mathlib',
	'mathlib/extended',
	'thirdparty/JoltPhysics',
	'tier0',
	'tier1',
	'tier2',
	'tier3',
	'togl',
	'vphysics',
	'vstdlib'
]

def options(opt):
	opt.load('compiler_cxx compiler_c')

	grp = opt.add_option_group('Common options')

	grp.add_option('--cfg', action = 'store', dest = 'CFG', default = 'release',
		help = 'build for release, debug or asan [default: %default]')

	grp.add_option('--dedicated', action = 'store_true', dest = 'DEDICATED', default = False,
		help = 'build Xash Dedicated Server [default: %default]')

	grp.add_option('--64bits', action = 'store_true', dest = 'ALLOW64', default = False,
		help = 'allow targetting 64-bit engine [default: %default]')

	grp.add_option('--sdl', action = 'store_true', dest = 'SDL', default = False,
		help = 'build with SDL [default: %default]')

	grp.add_option('--togl', action = 'store_true', dest = 'TOGL', default = False,
		help = 'build with driectx to opengl layer [default: %default]')

	opt.recurse(SubProjects)

def configure(conf):
	conf.env.CFG		= conf.options.CFG
	conf.env.DEDICATED	= conf.options.DEDICATED
	conf.env.ALLOW64	= conf.options.ALLOW64
	conf.env.SDL		= conf.options.SDL
	conf.env.TOGL		= conf.options.TOGL
	conf.env.LIBDIR		= conf.env.PREFIX + '/bin'

	conf.env.MSVC_TARGETS = ['x86' if not conf.options.ALLOW64 else 'x64']

	# Load compilers early
	conf.load('compiler_c compiler_cxx')

	# HACKHACK: override msvc DEST_CPU value by something that we understand
	if conf.env.DEST_CPU == 'amd64':
		conf.env.DEST_CPU = 'x86_64'

	# Force XP compatibility, all build targets should add subsystem=bld.env.MSVC_SUBSYSTEM
	if conf.env.MSVC_TARGETS[0] == 'x86':
		conf.env.MSVC_SUBSYSTEM = 'WINDOWS,5.01'
		conf.env.CONSOLE_SUBSYSTEM = 'CONSOLE,5.01'
	else:
		conf.env.MSVC_SUBSYSTEM = 'WINDOWS'
		conf.env.CONSOLE_SUBSYSTEM = 'CONSOLE'

	FLAGS = {
		'common': {
			'msvc': ['/DEBUG', '/FS', '/MP'], # always create PDB, doesn't affect result binaries
			'clang' : ['-fPIC', '-fvisibility=hidden'],
			'default': ['-Wl,--no-undefined', '-fPIC', '-fvisibility=hidden']
		},
		'lto': {
			'msvc': ['/O2', '/LTCG', '/Zi', '/MT'],
			'gcc': ['-O3', '-flto=auto', '-g'],
			'default': ['-O3', '-flto', '-g']
		},
		'release': {
			'msvc': ['/O2', '/Zi', '/MT'],
			'default': ['-O3', '-g']
		},
		'debug': {
			'msvc': ['/Od', '/Zi', '/MTd'],
			'default': ['-O0', '-g']
		},
		'asan': {
			'msvc': ['/O0', '/fsanitize=address', '/MT'],
			'default': ['-fsanitize=address', '-pthread']
		}
	}

	flags = get_flags_by_compiler(FLAGS['common'], conf.env.COMPILER_CC)
	flags += get_flags_by_compiler(FLAGS[conf.env.CFG], conf.env.COMPILER_CC)

	if conf.env.COMPILER_CC != 'msvc':
		if conf.env.ALLOW64:
			if conf.env.COMPILER_CC == 'msvc':
				flags += ['/MACHINE:X64']
			elif conf.env.DEST_OS == 'darwin':
				flags += ['-arch', 'x86_64']
			else:
				flags += ['-m64']
		else:
			if conf.env.COMPILER_CC == 'msvc':
				flags += ['/MACHINE:X86', '/arch:SSE2']
			elif conf.env.DEST_OS == 'darwin':
				flags += ['-arch', 'i386']
			else:
				flags += ['-m32']

	if conf.env.DEST_CPU in ['x86', 'x86_64'] and conf.env.COMPILER_CC != 'msvc':
		flags += ['-march=core2', '-mtune=nocona']

	cxxflags = ['/std:c++17'] if conf.env.COMPILER_CC == 'msvc' else ['-std=c++17'] # require c++17 support

	defines = [
		'CSTRIKE_REL_BUILD=1',
		'RAD_TELEMETRY_DISABLED'
	]

	if conf.env.CFG == 'debug':
		defines += ['DEBUG', '_DEBUG']
	else:
		defines += ['NDEBUG']

	if conf.env.DEST_OS == 'linux':
		defines += [
			'LINUX=1',
			'_LINUX=1',
			'POSIX=1',
			'_POSIX=1',
			'GNUC',
			'_DLL_EXT=.so'
		]
	elif conf.env.DEST_OS == 'win32':
		defines += [
			'WIN32',
			'_WIN32',
			'_WINDOWS',
			'_USRDLL',
			'_CRT_SECURE_NO_DEPRECATE',
			'_CRT_NONSTDC_NO_DEPRECATE',
			'_ALLOW_RUNTIME_LIBRARY_MISMATCH',
			'_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH',
			'_ALLOW_MSC_VER_MISMATCH',
			'_HAS_STD_BYTE=0',
			'_DLL_EXT=.dll'
		]
		if conf.env.ALLOW64:
			defines += [
				'WIN64',
				'_WIN64'
			]

	if conf.env.ALLOW64:
		defines += ['PLATFORM_64BITS']

	if conf.env.DEDICATED:
		defines += ['DEDICATED']
	
	if conf.env.COMPILER_CC == 'gcc':
		defines += ['COMPILER_GCC']
	if conf.env.COMPILER_CC == 'clang':
		defines += ['COMPILER_CLANG']
	if conf.env.COMPILER_CC == 'msvc':
		defines += ['COMPILER_MSVC']
		defines += ['COMPILER_MSVC64'] if conf.env.ALLOW64 else ['COMPILER_MSVC32']

	includes = [
		os.path.abspath('public'),
		os.path.abspath('public/tier0'),
		os.path.abspath('public/tier1'),
		os.path.abspath('common')
	]

	if conf.env.TOGL:
		defines += [
			'GL_GLEXT_PROTOTYPES',
			'DX_TO_GL_ABSTRACTION'
		]

	if conf.env.SDL:
		defines += ['USE_SDL']
		includes += [os.path.abspath('thirdparty/SDL2')]
		conf.check_cc(lib='SDL2')

	if conf.env.DEST_OS == 'win32':
		a = [ 'user32', 'shell32', 'gdi32', 'advapi32', 'dbghelp', 'psapi', 'ws2_32' ]
		if conf.env.COMPILER_CC == 'msvc':
			for i in a:
				conf.check_lib_msvc(i)
		else:
			for i in a:
				conf.check_cc(lib = i)

	conf.env.append_unique('CFLAGS', flags)
	conf.env.append_unique('CXXFLAGS', cxxflags + flags)
	conf.env.append_unique('LINKFLAGS', flags)
	conf.env.append_unique('DEFINES', defines)
	conf.env.append_unique('INCLUDES', includes)

	if conf.env.COMPILER_CC != 'msvc':
		opt_flags = [
			'-Wall'
		]
		opt_flags += ['-fcolor-diagnostics'] if conf.env.COMPILER_CC == 'clang' else ['-fdiagnostics-color=always']
		conf.env.append_unique('CFLAGS', opt_flags)
		conf.env.append_unique('CXXFLAGS', opt_flags)

	for i in SubProjects:
		depth_push(i)
		saveenv = conf.env
		conf.setenv(i, conf.env)
		conf.recurse(i)
		conf.setenv('')
		conf.env = saveenv
		depth_pop()

def get_flags_by_compiler(flags, compiler):
	if compiler in flags: return flags[compiler]
	elif 'default' in flags: return flags['default']

DEPTH = ''
def depth_push(path):
	global DEPTH
	DEPTH = os.path.join(DEPTH, path)
def depth_pop():
	global DEPTH
	DEPTH = os.path.dirname(DEPTH)

def build(bld):
	if not bld.all_envs:
		bld.load_envs()
	for i in SubProjects:
		saveenv = bld.env
		bld.env = bld.all_envs[i]
		bld.recurse(i)
		bld.env = saveenv
