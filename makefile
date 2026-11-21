CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -MMD -MP

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  # Homebrew prefix: /opt/homebrew on Apple Silicon, /usr/local on Intel
  BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
  INCLUDES    := -I$(BREW_PREFIX)/include
  LIBS        := -L$(BREW_PREFIX)/lib -lsfml-graphics -lsfml-window -lsfml-system \
                 -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
  # Linux / other Unix: let pkg-config find SFML, fall back to plain -l flags
  PKG_OK   := $(shell pkg-config --exists sfml-graphics && echo yes)
  ifeq ($(PKG_OK),yes)
    INCLUDES := $(shell pkg-config --cflags sfml-graphics)
    LIBS     := $(shell pkg-config --libs sfml-graphics)
  else
    INCLUDES :=
    LIBS     := -lsfml-graphics -lsfml-window -lsfml-system
  endif
  LIBS += -pthread
endif

VERSION_FILE := VERSION
CUR_VERSION  := $(shell cat $(VERSION_FILE) 2>/dev/null || echo 1.0.0)
NEXT_VERSION := $(shell echo $(CUR_VERSION) | awk -F. '{printf "%d.%d.%d", $$1, $$2, $$3 + 1}')

ENGINE_DIR := Engines/Arpeggio-$(NEXT_VERSION)
TARGET     := $(ENGINE_DIR)/Arpeggio-$(NEXT_VERSION)
SCRIPT     := $(ENGINE_DIR)/arpeggio-$(NEXT_VERSION).sh
LATEST     := Engines/Arpeggio-$(CUR_VERSION)/Arpeggio-$(CUR_VERSION)

SRCS   := main.cpp Moves.cpp simulateMoves.cpp minimax.cpp gui.cpp UCI/UciOut.cpp UCI/UciParser.cpp UCI/GameManager.cpp TranspositionTable.cpp
OBJS   := $(SRCS:.cpp=.o)
# everything needed to rebuild a version elsewhere (e.g. the Mac test box)
SNAP_FILES := makefile timer.cpp $(SRCS) $(wildcard *.h UCI/*.h)

# every `make` builds a new version: Engines/Arpeggio-X.Y.Z/ with its .sh
all: $(TARGET)

$(TARGET): $(OBJS)
	mkdir -p $(ENGINE_DIR)
	$(CXX) $(OBJS) $(LIBS) -o $@ || { rmdir $(ENGINE_DIR); exit 1; }
	printf '#!/bin/sh\nexec "$$(dirname "$$0")/Arpeggio-$(NEXT_VERSION)" uci\n' > $(SCRIPT)
	chmod +x $(SCRIPT)
	tar czf $(ENGINE_DIR)/src.tar.gz $(SNAP_FILES)
	echo $(NEXT_VERSION) > $(VERSION_FILE)
	@echo "built Arpeggio $(NEXT_VERSION)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# rebuild objects when a header they include changes
-include $(OBJS:.o=.d)

# builds the current tree as a given version number WITHOUT bumping VERSION.
# used by scripts/match.sh to reproduce a laptop version on the Mac from its
# src.tar.gz, so version numbers stay identical across machines.
as-version: $(OBJS)
	@test -n "$(V)" || { echo "usage: make as-version V=X.Y.Z"; exit 1; }
	mkdir -p Engines/Arpeggio-$(V)
	$(CXX) $(OBJS) $(LIBS) -o Engines/Arpeggio-$(V)/Arpeggio-$(V)
	printf '#!/bin/sh\nexec "$$(dirname "$$0")/Arpeggio-$(V)" uci\n' > Engines/Arpeggio-$(V)/arpeggio-$(V).sh
	chmod +x Engines/Arpeggio-$(V)/arpeggio-$(V).sh

# runs the latest built version (GUI mode) without building a new one
run:
	./$(LATEST)

# removes object files only; built engines in Engines/ are never deleted
clean:
	rm -f $(OBJS) $(OBJS:.o=.d)

.PHONY: all run clean as-version
