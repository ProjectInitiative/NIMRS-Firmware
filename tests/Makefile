CXX = g++
CXXFLAGS = -std=c++17 -Itests/mocks -Itests/mocks/freertos -Isrc -Wall

# Targets
TARGET_CONN = tests/test_ConnectivityManager
TARGET_MAIN = tests/run_tests
TARGET_LOGIC = tests/test_logic
TARGET_LOGGER = tests/test_logger

# Sources
SRCS_CONN = src/ConnectivityManager.cpp tests/mocks/mocks_infra.cpp tests/mocks/mocks_logger.cpp tests/test_ConnectivityManager.cpp
SRCS_MAIN = tests/test_main.cpp tests/mocks/mocks_infra.cpp tests/mocks/mocks_logger.cpp
SRCS_LOGIC = tests/test_logic.cpp src/BemfEstimator.cpp src/DspFilters.cpp src/RippleDetector.cpp
SRCS_LOGGER = tests/test_logger.cpp src/Logger.cpp tests/mocks/mocks_infra.cpp

all: $(TARGET_CONN) $(TARGET_MAIN) $(TARGET_LOGIC) $(TARGET_LOGGER)
	./$(TARGET_MAIN)
	./$(TARGET_CONN)
	./$(TARGET_LOGIC)
	./$(TARGET_LOGGER)

$(TARGET_CONN): $(SRCS_CONN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_MAIN): $(SRCS_MAIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_LOGIC): $(SRCS_LOGIC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_LOGGER): $(SRCS_LOGGER)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET_CONN) $(TARGET_MAIN) $(TARGET_LOGIC) $(TARGET_LOGGER)
