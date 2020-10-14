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
AUX_SOURCE_DIRECTORY(./src/namespaces/cgroup/ MyDocker_SRC_namespaces_cgroup)

# Set seccomp source directory
AUX_SOURCE_DIRECTORY(./src/seccomp/ MyDocker_SRC_seccomp)

# Create executable
ADD_EXECUTABLE(
	MyDocker
	${MyDocker_SRC}
	${MyDocker_SRC_helpers}
	${MyDocker_SRC_namespaces_mount}
	${MyDocker_SRC_namespaces_user}
	${MyDocker_SRC_namespaces_network}
	${MyDocker_SRC_namespaces_cgroup}
	${MyDocker_SRC_seccomp}
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Seccomp)

if (SECCOMP_FOUND)
	add_library( seccomp SHARED IMPORTED )

	set_target_properties( seccomp PROPERTIES IMPORTED_LOCATION ${SECCOMP_LIBRARIES} )

	TARGET_LINK_LIBRARIES(MyDocker seccomp cap)
endif()