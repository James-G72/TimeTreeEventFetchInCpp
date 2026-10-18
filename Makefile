CXX = g++
AR ?= ar
PKG_CONFIG ?= pkg-config
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags libcurl)
CXXFLAGS += -std=c++17 -Wall -Wextra
LDLIBS += $(shell $(PKG_CONFIG) --libs libcurl)
OBJECTS = timetree_utils.o timetree_structs.o

.PHONY: all run clean
all: libtimetree.a timetree_example.exe
libtimetree.a: $(OBJECTS)
	$(AR) rcs $@ $^
timetree_example.exe: main.o libtimetree.a
	$(CXX) $(LDFLAGS) -o $@ main.o libtimetree.a $(LDLIBS)
%.o: %.cpp timetree_utils.hpp timetree_structs.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
run: timetree_example.exe
	./timetree_example.exe
clean:
	rm -f $(OBJECTS) main.o libtimetree.a timetree_example.exe
