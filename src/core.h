#pragma once
#include <cstdint>
#include <span>

#include "bus.h"
#include "cpu.h"

constexpr auto cpuClockPerSecond = 33868800;
constexpr auto framesPerSecond = 60;
constexpr int  cpuClockPerFrame = cpuClockPerSecond / framesPerSecond;

enum class Button {
    Select   = 0,
    L3       = 1,
    R3       = 2,
    Start    = 3,
    Up       = 4,
    Right    = 5,
    Down     = 6,
    Left     = 7,
    L2       = 8,
    R2       = 9,
    L1       = 10,
    R1       = 11,
    Triangle = 12,
    Circle   = 13,
    Cross    = 14,
    Square   = 15
};

enum class Axis {
    LeftX,
    LeftY,
    RightX,
    RightY
};

struct DisplayRegion {
	int x;
	int y;
	int width;
	int height;
};

class Core {
	public:
		Core();
		~Core();
		void stepFrame();
		uint16_t* getFramebuffer();
		std::span<const int16_t> getAudioSamples();
		void clearAudioSamples();
		void setButton(int port, Button button, bool pressed);
		void setAxis(int port, Axis axis, float value);
		DisplayRegion getDisplayRegion() const;
		bool loadBIOS(unsigned char* fileData, int fileSize);
		
	private:
		Bus bus;
		CPU cpu;
};