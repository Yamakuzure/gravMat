.PHONY: all clean

CXX       = g++
CXXFLAGS += -I../../.. -DPWX_THREADS
LDFLAGS  +=
LIBS      = -lsfml-system -lsfml-window -lsfml-graphics
TARGET    = ../../gravMat
MODULES   = consoleui.o sfmlui.o environment.o matter.o main.o

# Add font config if configured
ifneq (DEFAULT, ${FONT_PATH})
	CXXFLAGS := ${CXXFLAGS} -DFONT_PATH=\"${FONT_PATH}\"
endif
ifneq (DEFAULT, ${FONT_NAME})
	CXXFLAGS := ${CXXFLAGS} -DFONT_NAME=\"${FONT_NAME}\"
endif
# ==============================

all: $(TARGET)

clean:
	@echo "Cleaning gravMat"
	@rm -rf $(TARGET) $(MODULES)


$(TARGET): $(MODULES)
	@echo "Linking $(TARGET) ..."
	@$(CXX) -o $(TARGET) $(LDFLAGS) $(MODULES) $(LIBS)

%.o: %.cpp
	@echo "Generating $@ ..."
	@$(CXX) -c $< -o $@ $(CXXFLAGS) $(FFLAGS)

# --- Dependencies ---
consoleui.cpp: consoleui.h
environment.cpp: environment.h matter.h dustpixel.h
main.cpp: main.h sfmlui.h consoleui.h
sfmlui.cpp: sfmlui.h matter.h dustpixel.h
matter.cpp: environment.h matter.h dustpixel.h
consoleui.h: main.h
main.h: environment.h
sfmlui.h: main.h
matter.h: colormap.h
dustpixel.h: masspixel.h
