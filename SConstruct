env = Environment()
env.Append(CCFLAGS=['-g', '-std=c++0x', '-pthread', '-Wall', '-Wextra', '-Werror', '$IM_CXXFLAGS', '$IM_LDFLAGS'])
env.Append(LIBS=['rt', 'pthread'])

# $$$ OBJECT FILES $$$
env.Object('Lib-Camera-Control/VelodyneMonitor.cpp')

# $$$ TARGETS $$$
env.Program(Split('''
	raw_lidar_reader.cpp
	Lib-Camera-Control/VelodyneMonitor.o
	'''))

env.Program(Split('''
	jpeg.cpp
	'''))
