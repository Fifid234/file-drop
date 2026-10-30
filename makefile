ifeq ($(OS),windows)
	CXX = x86_64-w64-mingw32-g++
    TARGET = file-drop.exe
    LDFLAGS = -lws2_32 -static
else
    CXX = g++
    TARGET = file-drop
    LDFLAGS = 
endif

$(TARGET):
	$(CXX) src/main.cpp -o $(TARGET) $(LDFLAGS)