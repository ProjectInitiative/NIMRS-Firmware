CXX = g++
CXXFLAGS = -std=c++17 -Isrc -Itests/mocks -Itests/mocks/freertos -Wall

SRCS = src/ConnectivityManager.cpp tests/mocks/mocks.cpp tests/test_ConnectivityManager.cpp
TARGET = tests/test_ConnectivityManager

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
