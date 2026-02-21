CXX = g++
CXXFLAGS = -I../src -Imocks -Imocks/freertos -Wall -std=c++17

# List of test binaries to build
TESTS = run_tests test_ConnectivityManager

# Sources for each test
SRCS_RUN_TESTS = test_main.cpp
SRCS_CONN_MGR = ../src/ConnectivityManager.cpp mocks/mocks.cpp test_ConnectivityManager.cpp

all: $(TESTS)

run_tests: $(SRCS_RUN_TESTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

test_ConnectivityManager: $(SRCS_CONN_MGR)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TESTS)
	./run_tests
	./test_ConnectivityManager

clean:
	rm -f $(TESTS)
