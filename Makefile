CXX = g++
AR ?= ar
PKG_CONFIG ?= pkg-config
CXXFLAGS += -std=c++17 -Wall -Wextra
CURL_CFLAGS = $(shell $(PKG_CONFIG) --cflags libcurl)
CURL_LIBS = $(shell $(PKG_CONFIG) --libs libcurl)
OBJECTS = timetree_utils.o timetree_structs.o

.PHONY: all library run test clean
all: libtimetree.a timetree_example.exe
library: libtimetree.a
libtimetree.a: $(OBJECTS)
	$(AR) rcs $@ $^
timetree_example.exe: main.o libtimetree.a
	$(CXX) $(LDFLAGS) -o $@ main.o libtimetree.a $(CURL_LIBS) $(LDLIBS)
main.o: CPPFLAGS += $(CURL_CFLAGS)
%.o: %.cpp timetree_utils.hpp timetree_structs.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
timetree_integration.exe: tests/integration.cpp libtimetree.a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -I. $< libtimetree.a $(LDLIBS) -o $@
timetree_recurrence.exe: tests/recurrence.cpp libtimetree.a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -I. $< libtimetree.a $(LDLIBS) -o $@
test: timetree_integration.exe timetree_recurrence.exe
	./timetree_integration.exe
	./timetree_recurrence.exe
run: timetree_example.exe
	./timetree_example.exe
clean:
	rm -f $(OBJECTS) main.o libtimetree.a timetree_example.exe timetree_integration.exe timetree_recurrence.exe
