# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -I./vendor -lpthread

# Target executable name
TARGET = recurrency

# Source files
SRC = src/main.cpp

# Default rule (what happens when you type 'make')
all: $(TARGET)

# Build rule
$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(CXXFLAGS)

# Clean rule (type 'make clean' to remove artifacts)
clean:
	rm -f $(TARGET)