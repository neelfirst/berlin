env = Environment()
env.Append(CCFLAGS=['-g', '-std=c++0x', '-pthread', '-Wall', '-Wextra', '-Werror', '$IM_CXXFLAGS', '$IM_LDFLAGS'])
env.Append(LIBS=['rt', 'pthread', 'cv', 'highgui'])

# $$$ TARGETS $$$
env.Program(Split('''
	raw_lidar_reader.cpp
	'''))
