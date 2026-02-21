CXX = g++
CXXFLAGS = -std=c++17 -Itests/mocks -Itests/mocks/freertos -Isrc -Wall

# Targets
TARGET_CONN = tests/test_ConnectivityManager
TARGET_MAIN = tests/run_tests
TARGET_LOGIC = tests/test_logic
TARGET_MOTOR = tests/test_MotorController

# Sources
SRCS_CONN = src/ConnectivityManager.cpp tests/mocks/mocks.cpp tests/test_ConnectivityManager.cpp
SRCS_MAIN = tests/test_main.cpp tests/mocks/mocks.cpp
SRCS_LOGIC = tests/test_logic.cpp src/BemfEstimator.cpp src/DspFilters.cpp src/RippleDetector.cpp
SRCS_MOTOR = tests/test_MotorController.cpp src/MotorController.cpp tests/mocks/mocks.cpp

all: $(TARGET_CONN) $(TARGET_MAIN) $(TARGET_LOGIC) $(TARGET_MOTOR)
	./$(TARGET_MAIN)
	./$(TARGET_CONN)
	./$(TARGET_LOGIC)
	./$(TARGET_MOTOR)

$(TARGET_CONN): $(SRCS_CONN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_MAIN): $(SRCS_MAIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_LOGIC): $(SRCS_LOGIC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_MOTOR): $(SRCS_MOTOR)
	$(CXX) $(CXXFLAGS) -DSKIP_MOCK_MOTOR_CONTROLLER -o $@ $^

clean:
	rm -f $(TARGET_CONN) $(TARGET_MAIN) $(TARGET_LOGIC) $(TARGET_MOTOR)
