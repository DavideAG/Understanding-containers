PROJECT(MyDocker)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# Set source directory
AUX_SOURCE_DIRECTORY(./src/ MyDocker_SRC)


# Create executable
ADD_EXECUTABLE(
	MyDocker
	${MyDocker_SRC}
)
