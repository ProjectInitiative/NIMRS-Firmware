CXX = g++
CXXFLAGS = -I../src -Imocks -Imocks/freertos -Wall -std=c++17

TARGET = run_tests
SRCS = test_main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
