CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall
LDFLAGS = -lbe -lstdc++
TARGET = Jezziku
SRCS = Jezziku.cpp
OBJS = $(SRCS:.cpp=.o)
ICON = jezz_icon
RDEF = Jezziku.rdef
RSRC = Jezziku.rsrc

all: $(TARGET)

$(TARGET): $(OBJS) $(RSRC)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)
	xres -o $(TARGET) $(RSRC)
	mimeset --all $(TARGET)

$(RSRC): $(RDEF) $(ICON)
	rc -o $(RSRC) $(RDEF)

%.o: %.cpp Jezziku.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(RSRC)

run: $(TARGET)
	./$(TARGET)
