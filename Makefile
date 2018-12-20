.Phony: all clean install

# ------------------------------------
# Version
# ------------------------------------
VERSION := 0.8.7


DEBUG   ?= NO
# Note: Submit as "YES" to enable debugging
# Note: If any flag starting with -g is found in the CXXFLAGS, DEBUG is
#       switched to YES no matter whether set otherwise or not.

# These three are mutually exclusive. If all are set to yes,
# address-sanitizing has priority, followed by leak, thread
# is last.
SANITIZE_ADDRESS ?= NO
SANITIZE_LEAK    ?= NO
SANITIZE_THREAD  ?= NO


# ------------------------------------
# Font settings
# ------------------------------------
FONT_PATH ?= /usr/share/fonts/freefont
FONT_NAME ?= FreeMono.ttf


# ------------------------------------
# Tools and Flags
# ------------------------------------
CXX      ?= $(shell which g++)
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -fexceptions -pthread
CPPFLAGS += $(shell pkg-config --cflags pwxlib) $(shell pkg-config --cflags sfml-graphics)
CPPFLAGS += -DVERSION=\"${VERSION}\" -DFONT_PATH=\"$(FONT_PATH)\" -DFONT_NAME=\"$(FONT_NAME)\"
INSTALL  := $(shell which install)
LDFLAGS  += $(shell pkg-config --libs pwxlib) $(shell pkg-config --libs sfml-graphics) -lpthread
RM       := $(shell which rm) -f
SED      := $(shell which sed)
TARGET   := gravmat

# Use the compiler as the linker.
LD := $(CXX)


# ------------------------------------
# Install and target directories
# ------------------------------------
PREFIX     ?= /usr
DESTDIR    ?=
BINPREFIX  ?= $(PREFIX)
BINDIR     ?= ${BINPREFIX}/bin
DOCPREFIX  ?= $(PREFIX)
DOCDIR     ?= $(DOCPREFIX)/share/doc/gravMat-$(VERSION)


# ------------------------------------
# Debug Mode settings
# ------------------------------------
HAS_DEBUG_FLAG := NO
ifneq (,$(findstring -g,$(CXXFLAGS)))
  ifneq (,$(findstring -ggdb,$(CXXFLAGS)))
    HAS_DEBUG_FLAG := YES
  endif
  DEBUG := YES
endif

ifeq (YES,$(DEBUG))
  ifeq (NO,$(HAS_DEBUG_FLAG))
    CXXFLAGS := -ggdb ${CXXFLAGS} -Og
  endif

  CXXFLAGS := ${CXXFLAGS} -fstack-protector-strong -Wunused -DLIBPWX_DEBUG

  # address / thread sanitizer activation
  ifeq (YES,$(SANITIZE_ADDRESS))
    CXXFLAGS := ${CXXFLAGS} -fsanitize=address
    LDFLAGS  := ${LDFLAGS} -fsanitize=address
  else ifeq (YES,$(SANITIZE_LEAK))
    CXXFLAGS := ${CXXFLAGS} -fsanitize=leak
    LDFLAGS  := ${LDFLAGS} -fsanitize=leak
  else ifeq (YES,$(SANITIZE_THREAD))
    CXXFLAGS := ${CXXFLAGS} -fsanitize=thread -fPIC -O2 -ggdb
    LDFLAGS  := ${LDFLAGS} -fsanitize=thread -pie -O2 -ggdb
  endif

else
  CXXFLAGS := -march=native ${CXXFLAGS} -O2
endif


# ------------------------------------
# Source files, objects, deps, doc
# ------------------------------------
SOURCES  := $(sort $(wildcard src/*.cpp))
MODULES  := $(addprefix obj/,$(notdir $(SOURCES:.cpp=.o)))
DEPENDS  := $(addprefix dep/,$(notdir $(SOURCES:.cpp=.d)))
DOCFILES := \
AUTHORS ChangeLog code_of_conduct.md CONTRIBUTING.md \
INSTALL.md LICENSE NEWS.md README.md TODO.md


# ------------------------------------
# Rules
# ------------------------------------
.SUFFIXES: .cpp


# ------------------------------------
# Create dependencies
# ------------------------------------
dep/%.d: src/%.cpp
	@set -e; $(RM) $@; \
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< > $@.$$$$; \
	$(SED) 's,\($*\)\.o[ :]*,obj/\1.o $@ : ,g' < $@.$$$$ > $@; \
	$(RM) $@.$$$$


# ------------------------------------
# Compile modules
# ------------------------------------
obj/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<


# ------------------------------------
# Default target
# ------------------------------------
all: $(TARGET)


# ------------------------------------
# Regular targets
# ------------------------------------
clean:
	@echo "Cleaning makeSimplexTexture"
	@rm -rf $(TARGET) $(MODULES)

$(TARGET): $(MODULES)
	$(CXX) -o $(TARGET) $(MODULES) $(CPPFLAGS) $(LDFLAGS) $(CXXFLAGS)

install: $(TARGET)
	$(INSTALL) -d $(DESTDIR)${BINDIR}
	$(INSTALL) -m 755 $(TARGET) $(DESTDIR)${BINDIR}
	$(INSTALL) -d $(DESTDIR)${DOCDIR}
	$(INSTALL) -m 644 $(DOCFILES) $(DESTDIR)${DOCDIR}


# ------------------------------------
# Include all dependency files
# ------------------------------------
ifeq (,$(findstring clean,$(MAKECMDGOALS)))
  -include $(DEPENDS)
endif
