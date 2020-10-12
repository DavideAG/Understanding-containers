PROJECT(MyDocker)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# Set source directory
AUX_SOURCE_DIRECTORY(./src/ MyDocker_SRC)

# Set helpers source directory
AUX_SOURCE_DIRECTORY(./src/helpers/ MyDocker_SRC_helpers)

# Set mount namespaces source directory
AUX_SOURCE_DIRECTORY(./src/namespaces/mount/ MyDocker_SRC_namespaces_mount)

# Set user namespaces source directory
AUX_SOURCE_DIRECTORY(./src/namespaces/user/ MyDocker_SRC_namespaces_user)

# Set network namespaces source directory
AUX_SOURCE_DIRECTORY(./src/namespaces/network/ MyDocker_SRC_namespaces_network)

# Set cgroup namespaces source directory
AUX_SOURCE_DIRECTORY(./src/namespaces/cgroup MyDocker_SRC_namespaces_cgroup)

# Create executable
ADD_EXECUTABLE(
	MyDocker
	${MyDocker_SRC}
	${MyDocker_SRC_helpers}
	${MyDocker_SRC_namespaces_mount}
	${MyDocker_SRC_namespaces_user}
	${MyDocker_SRC_namespaces_network}
)
SET_TARGET_PROPERTIES(MyDocker PROPERTIES COMPILE_FLAGS "-lcap")

target_link_libraries(MyDocker PRIVATE cap)
