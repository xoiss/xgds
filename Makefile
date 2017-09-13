TARGET := $(lastword $(subst /, ,$(CURDIR)))
ifneq (,$(WINDIR))
    TARGET := $(TARGET).exe
endif

SOURCES := $(shell ls *.cpp 2>/dev/null)

CXXFLAGS = -O0 -g3

.PHONY: all clean

ifneq (,$(SOURCES))
all: $(TARGET)
$(TARGET): $(SOURCES)
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@
else
all:
	@echo No source files found.
endif

clean:
	$(RM) *.o *.s *.ii $(TARGET)
