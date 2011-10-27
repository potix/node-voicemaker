srcdir = '.'
blddir = 'build'
VERSION = '0.0.4'

def set_options(opt):
  opt.tool_options('compiler_cxx')

def configure(conf):
  print "required dependency:"
  print "    mecab installed with --enable-shared" 
  print "    utf-8 encoded mecab dictionary installed" 
  print "    aquestalk2 installed"
  conf.check_tool('compiler_cxx')
  conf.check_tool('node_addon')
  conf.env.append_value("CXXFLAGS", "-I/usr/local/include")
  conf.env.append_value("LINKFLAGS", "-L/usr/local/lib")
  conf.env.append_value("LIB", "AquesTalk2")
  conf.env.append_value("LIB", "mecab")

def build(bld):
  obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
  obj.target = 'voicemaker'
  obj.source = 'voicemaker.cc'
