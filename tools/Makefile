CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Iinclude
SRCS      = ui_sim.cpp canvas_console.cpp ../src/ui_pages.cpp
TARGET    = ui_sim

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

run-all: $(TARGET)
	./$(TARGET) --all