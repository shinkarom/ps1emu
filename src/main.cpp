#include <iostream>
#include <cstdint>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <miniaudio.h>

#include <core.h>

void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    auto* out = static_cast<int16_t*>(pOutput);

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        int16_t sample = 0;

        *out++ = sample;
        *out++ = sample;
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
	
	glEnable(GL_DEPTH_TEST);
	
	ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = 44100;
	deviceConfig.dataCallback = audio_callback;
	deviceConfig.pUserData = &core;
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
		core.stepFrame();
		
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glfwSwapBuffers(window);
	}
	
	ma_device_uninit(&device);
	glfwTerminate();
	return 0;
}