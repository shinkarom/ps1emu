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
double previousTime = glfwGetTime();
double accumulator = 0.0;

void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    auto* rb = static_cast<ma_pcm_rb*>(pDevice->pUserData);
    auto* out = static_cast<int16_t*>(pOutput);

    ma_uint32 framesRemaining = frameCount;

    while (framesRemaining > 0) {
        ma_uint32 framesToRead = framesRemaining;
        void* pBuffer = nullptr;

        // 1. Acquire accessible chunk
        if (ma_pcm_rb_acquire_read(rb, &framesToRead, &pBuffer) != MA_SUCCESS || framesToRead == 0) {
            break; // Buffer is empty (underrun)
        }

        // 2. Copy the samples (1 frame = 2 samples: L and R)
        size_t samplesToCopy = framesToRead * 2;
        std::memcpy(out, pBuffer, samplesToCopy * sizeof(int16_t));

        // 3. Commit the read (only 2 arguments!)
        ma_pcm_rb_commit_read(rb, framesToRead);

        out += samplesToCopy;
        framesRemaining -= framesToRead;
    }

    // 4. Fill any remaining unfulfilled frames with silence (zeros)
    if (framesRemaining > 0) {
        std::memset(out, 0, framesRemaining * 2 * sizeof(int16_t));
    }

    (void)pInput;
}	

int main(int argc, char *argv[])
{
	Core core;
	std::cout<<"Hello World!";
	glfwInit();
	GLFWwindow *window = glfwCreateWindow(320, 240, "ps1emu", nullptr, nullptr);
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
	
	while(!glfwWindowShouldClose(window)){
		glfwPollEvents();
		
		double currentTime = glfwGetTime();
		double frameDelta = currentTime - previousTime;
		previousTime = currentTime;
		if(frameDelta > 0.25) frameDelta = 0.25;
		accumulator += frameDelta;
		
		while(accumulator >= FRAME_TIME) {
			core.stepFrame();
			accumulator -= FRAME_TIME;
			
			std::span<const int16_t> samples = core.getAudioSamples();
			ma_uint32 framesAvailable = static_cast<ma_uint32>(samples.size() / 2);
			const int16_t* src = samples.data();

			while (framesAvailable > 0) {
				ma_uint32 framesToWrite = framesAvailable;
				void* pBuffer = nullptr;

				// 1. Acquire writable chunk
				if (ma_pcm_rb_acquire_write(&rb, &framesToWrite, &pBuffer) != MA_SUCCESS || framesToWrite == 0) {
					// Buffer full: emulator running ahead of audio hardware
					break; 
				}

				// 2. Copy samples
				size_t samplesToCopy = framesToWrite * 2;
				std::memcpy(pBuffer, src, samplesToCopy * sizeof(int16_t));

				// 3. Commit the write (only 2 arguments!)
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
	
	ma_pcm_rb_uninit(&rb);
	ma_device_uninit(&device);
	glfwTerminate();
	return 0;
}