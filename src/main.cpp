#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <miniaudio.h>

int main(int argc, char *argv[])
{
	std::cout<<"Hello World!";
	glfwInit();
	GLFWwindow *window = glfwCreateWindow(320, 240, "ps1emu", nullptr, nullptr);
	glfwMakeContextCurrent(window);
	
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
	glEnable(GL_DEPTH_TEST);
	
	
	while(!glfwWindowShouldClose(window)){
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	
	glfwTerminate();
	return 0;
}