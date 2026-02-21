CXX = g++
CXXFLAGS = -std=c++17 -Isrc -Itests/mocks -Itests/mocks/freertos -Wall

# Targets
TARGET_CONN = tests/test_ConnectivityManager
TARGET_MAIN = tests/run_tests

# Sources
SRCS_CONN = src/ConnectivityManager.cpp tests/mocks/mocks.cpp tests/test_ConnectivityManager.cpp
SRCS_MAIN = tests/test_main.cpp tests/mocks/mocks.cpp

all: $(TARGET_CONN) $(TARGET_MAIN)
	./$(TARGET_MAIN)
	./$(TARGET_CONN)

$(TARGET_CONN): $(SRCS_CONN)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_MAIN): $(SRCS_MAIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET_CONN) $(TARGET_MAIN)
