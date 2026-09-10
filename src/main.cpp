#include <iostream>
#include <cstdint>
#include <thread>
#include <chrono>
#include <cstring>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <miniaudio.h>

#include <core.h>

constexpr double TARGET_FPS = 59.94;
constexpr double FRAME_TIME = 1.0 / TARGET_FPS;

void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    auto* rb = static_cast<ma_pcm_rb*>(pDevice->pUserData);
    auto* out = static_cast<int16_t*>(pOutput);

    ma_uint32 framesRemaining = frameCount;

    while (framesRemaining > 0) {
        ma_uint32 framesToRead = framesRemaining;
        void* pBuffer = nullptr;


        if (ma_pcm_rb_acquire_read(rb, &framesToRead, &pBuffer) != MA_SUCCESS || framesToRead == 0) {
            break;
        }

        size_t samplesToCopy = framesToRead * 2;
        std::memcpy(out, pBuffer, samplesToCopy * sizeof(int16_t));

        ma_pcm_rb_commit_read(rb, framesToRead);

        out += samplesToCopy;
        framesRemaining -= framesToRead;
    }

    if (framesRemaining > 0) {
        std::memset(out, 0, framesRemaining * 2 * sizeof(int16_t));
    }

    (void)pInput;
}	

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_RELEASE) {
        return; // Ignore key repeats
    }

    bool pressed = (action == GLFW_PRESS);
    auto* core = static_cast<Core*>(glfwGetWindowUserPointer(window));

    switch (key) {
        case GLFW_KEY_UP:    core->setButton(0, Button::Up, pressed); break;
        case GLFW_KEY_DOWN:  core->setButton(0, Button::Down, pressed); break;
        case GLFW_KEY_LEFT:  core->setButton(0, Button::Left, pressed); break;
        case GLFW_KEY_RIGHT: core->setButton(0, Button::Right, pressed); break;

        case GLFW_KEY_Z:     core->setButton(0, Button::Cross, pressed); break;    // X
        case GLFW_KEY_X:     core->setButton(0, Button::Circle, pressed); break;   // O
        case GLFW_KEY_A:     core->setButton(0, Button::Square, pressed); break;   // Square
        case GLFW_KEY_S:     core->setButton(0, Button::Triangle, pressed); break; // Triangle

        case GLFW_KEY_ENTER: core->setButton(0, Button::Start, pressed); break;
        case GLFW_KEY_SPACE: core->setButton(0, Button::Select, pressed); break;
        case GLFW_KEY_Q:     core->setButton(0, Button::L1, pressed); break;
        case GLFW_KEY_W:     core->setButton(0, Button::R1, pressed); break;

        default: break;
    }
}

int main(int argc, char *argv[])
{
	Core core;
	std::cout<<"Hello World!";
	glfwInit();
	GLFWwindow *window = glfwCreateWindow(320, 240, "ps1emu", nullptr, nullptr);
	glfwSetWindowUserPointer(window, &core);
	glfwSetKeyCallback(window, key_callback);
	glfwMakeContextCurrent(window);
	
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
	GLuint fbo, texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	
	ma_pcm_rb rb;
	if(ma_pcm_rb_init(ma_format_s16, 2, 4096, nullptr, nullptr, &rb) != MA_SUCCESS) {
		std::cout<<"Error: Could not initialize ring buffer"<<std::endl;
		return -5;
	}
	
	ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = 44100;
	deviceConfig.dataCallback = audio_callback;
	deviceConfig.pUserData = &rb;
	ma_device device;
	
	if(ma_device_init(nullptr, &deviceConfig, &device) != MA_SUCCESS){
		std::cout<<"Error: Could not initialize audio device."<<std::endl;
		return -3;
	}
	
	if(ma_device_start(&device) != MA_SUCCESS){
		std::cout<<"Error: Could not start audio device."<<std::endl;
		ma_device_uninit(&device);
		return -4;
	}
	
	double previousTime = glfwGetTime();
	double accumulator = 0.0;
	
	while(!glfwWindowShouldClose(window)){
		glfwPollEvents();
		
		double currentTime = glfwGetTime();
		double frameDelta = currentTime - previousTime;
		previousTime = currentTime;
		if(frameDelta > 0.25) frameDelta = 0.25;
		accumulator += frameDelta;
		
		while(accumulator >= FRAME_TIME) {
			if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1)) {
				GLFWgamepadstate state;
				if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
					
					core.setButton(0, Button::Cross,    state.buttons[GLFW_GAMEPAD_BUTTON_A]);
					core.setButton(0, Button::Circle,   state.buttons[GLFW_GAMEPAD_BUTTON_B]);
					core.setButton(0, Button::Square,   state.buttons[GLFW_GAMEPAD_BUTTON_X]);
					core.setButton(0, Button::Triangle, state.buttons[GLFW_GAMEPAD_BUTTON_Y]);
					core.setButton(0, Button::L1,       state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]);
					core.setButton(0, Button::R1,       state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]);
					core.setButton(0, Button::L3,       state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]);
					core.setButton(0, Button::R3,       state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB]);
					core.setButton(0, Button::Start,    state.buttons[GLFW_GAMEPAD_BUTTON_START]);
					core.setButton(0, Button::Select,   state.buttons[GLFW_GAMEPAD_BUTTON_BACK]);
					core.setButton(0, Button::Up,       state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]);
					core.setButton(0, Button::Down,     state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]);
					core.setButton(0, Button::Left,     state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]);
					core.setButton(0, Button::Right,    state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]);

					core.setAxis(0, Axis::LeftX,  state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
					core.setAxis(0, Axis::LeftY,  state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
					core.setAxis(0, Axis::RightX, state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
					core.setAxis(0, Axis::RightY, state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
				}
			}
			core.stepFrame();
			accumulator -= FRAME_TIME;
			
			std::span<const int16_t> samples = core.getAudioSamples();
			ma_uint32 framesAvailable = static_cast<ma_uint32>(samples.size() / 2);
			const int16_t* src = samples.data();

			while (framesAvailable > 0) {
				ma_uint32 framesToWrite = framesAvailable;
				void* pBuffer = nullptr;

				if (ma_pcm_rb_acquire_write(&rb, &framesToWrite, &pBuffer) != MA_SUCCESS || framesToWrite == 0) {
					break; 
				}

				size_t samplesToCopy = framesToWrite * 2;
				std::memcpy(pBuffer, src, samplesToCopy * sizeof(int16_t));

				ma_pcm_rb_commit_write(&rb, framesToWrite);

				src += samplesToCopy;
				framesAvailable -= framesToWrite;
			}

			core.clearAudioSamples();
		}
		
		
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512,
			GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, core.getFramebuffer());
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		int winWidth, winHeight;
		glfwGetFramebufferSize(window, &winWidth, &winHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBlitFramebuffer(0, 0, 1024, 512, 
			0, winHeight, winWidth, 0,
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST);

		glfwSwapBuffers(window);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	
	ma_device_uninit(&device);
	ma_pcm_rb_uninit(&rb);
	glfwTerminate();
	return 0;
}