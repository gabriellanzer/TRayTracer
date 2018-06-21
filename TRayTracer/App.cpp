#include "App.h"

//STB Image Includes
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//Using Ravine namespace
using namespace rav;

App::App()
{

}


App::~App()
{
	//Terminate GLFW
	glfwTerminate();
}

App & App::getApp()
{
	//Guaranteed to be destroyed
	static App app;

	return app;
}

void _update_fps_counter(GLFWwindow* window) {
	static double previous_seconds = glfwGetTime();
	static int frame_count;
	double current_seconds = glfwGetTime();
	double elapsed_seconds = current_seconds - previous_seconds;
	if (elapsed_seconds > 0.01) {
		previous_seconds = current_seconds;
		double fps = (double)frame_count / elapsed_seconds;
		char tmp[128];
		sprintf_s(tmp, "opengl @ fps: %.2f", fps);
		glfwSetWindowTitle(window, tmp);
		frame_count = 0;
	}
	frame_count++;
}

int App::Initialize(cint &width, cint &height, str name, bool fullscreen, bool vsync)
{
	//Try to initialize GLFW
	if (!glfwInit())
	{
		//Debug error
		fprintf(stderr, "ERROR: could not start GLFW3\n");
		return 1;
	}

	//Link error callback function
	glfwSetErrorCallback(App::error_callback);

	//Monitor Holder variable
	GLFWmonitor *monitor = NULL;

	//Get primary monitor if fullscreen pretended
	if (fullscreen)
		monitor = glfwGetPrimaryMonitor();

	//Create application window
	window = glfwCreateWindow(width, height, name, monitor, NULL);
	if (!window) {
		fprintf(stderr, "ERROR: could not open window with GLFW3\n");
		glfwTerminate();
		return 1;
	}

	//Now that our windows is has been set, set reshape callback
	glfwSetWindowSizeCallback(window, App::reshape);

	//Set as current active context
	glfwMakeContextCurrent(window);

	//Start GLEW
	glewExperimental = GL_TRUE;
	glewInit();

	//Check OpenGL version
	const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
	const GLubyte* version = glGetString(GL_VERSION); // version as a string
	printf("Renderer: %s\n", renderer);
	printf("OpenGL version supported %s\n", version);

	assert(version[0] > '3' && "Installed version of OpenGL doesn't support compute shader");

	//For proper rendering
	glEnable(GL_DEPTH_TEST);	//Enable depth-testing
	glDepthFunc(GL_LESS);		//Depth-testing interprets a smaller value as "closer"
	glEnable(GL_CULL_FACE);		//Cull faces whose normals don't point towards the camera

	//Texture2D enabling
	glEnable(GL_TEXTURE_2D);	//Enable 2D Texture slot

	//Enable caching of buttons pressed
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);

	//Set a nice blue background
	glClearColor(0.0f, 0.4509803921568627f, 0.8980392156862745f, 1.0f);

	//Initialize Raytracer
	raytracer.Setup(width, height, 1);

	//Return no error message
	return 0;
}

int App::Run()
{
	CreateDefaultProgram();
	CreateScreenQuad();
	CreatePostProcess();

	////Load texture with STB Image
	//int width, height, nrChannels;
	//unsigned char *data = stbi_load("container.jpg", &width, &height, &nrChannels, 0);
	//if (data)
	//{
	//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	//	glGenerateMipmap(GL_TEXTURE_2D);
	//	stbi_image_free(data);
	//}
	//else
	//{
	//	rvDebug.Log("Failed to load texture", RV_ERROR_MESSAGE);
	//}

	while (!glfwWindowShouldClose(window))
	{
		_update_fps_counter(window);

		#pragma region Inputs

		//double mouseX, mouseY;
		//glfwGetCursorPos(window, &mouseX, &mouseY);
		//glm::rotate()


		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			raytracer.cameraPos.z += 0.2;

		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			raytracer.cameraPos.x -= 0.2;

		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			raytracer.cameraPos.z -= 0.2;

		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			raytracer.cameraPos.x += 0.2;

		//Camera Matrices
		glm::mat4 cameraRot;
		glm::vec4 cameraPos;

		#pragma endregion

		//===============COMPUT RAYTRACING HERE====================
		GLint raytracePreview = raytracer.Compute();
		//===============COMPUT RAYTRACING HERE====================

		#pragma region PostProcess
		GLuint idPostProcess = postProcessPipeline->Process(raytracePreview);
		#pragma endregion

		#pragma region Draw Raytracer Output

		//Clear back color and depth buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Use shader program
		rvUseProgram(defaultPrg);

		//Texture binding
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, idPostProcess);

		//Bind VAO
		screenQuadVAO->Bind();

		//Draw given VBO
		screenQuadVBO->Draw();

		#pragma endregion

		//Swap front and back buffers
		glfwSwapBuffers(window);

		//Poll for and process events
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, 1);
		}

	}

	//Return Sucessfully Exit
	return EXIT_SUCCESS;
}

int App::End()
{
	return 0;
}


void rav::App::CreateDefaultProgram()
{
	char* vertex_shader;
	rvLoadFile("./data/vertex_uv.vert", vertex_shader, true);

	char* fragment_shader;
	rvLoadFile("./data/fragment_base.frag", fragment_shader, true);

	GLuint vs = rvCreateShader("vertex_uv_vs", vertex_shader, RV_VERTEX_SHADER);

	GLuint fs = rvCreateShader("fragment_base_vs", fragment_shader, RV_FRAGMENT_SHADER);

	//Create program
	defaultPrg = rvCreateProgram("screen_pr", vs, fs);			//Create program with two shaders attached
																//rvSetAttributeLoc(pr, "vertex_position", 0);				//Set attribute location (before linking!)
																//rvSetAttributeLoc(pr, "vertex_coords", 1);				//Set attribute location (before linking!)
	rvLinkProgram(defaultPrg);									//Link program

}

void App::CreateScreenQuad() {

	screenQuadVBO = new VertexBuffer();
	screenQuadVAO = new VertexArray();

	GLint vp_loc = rvGetAttributeLoc(defaultPrg, "vertex_position");	//Get attribute location (after linking!)
	GLint vc_loc = rvGetAttributeLoc(defaultPrg, "vertex_coords");		//Get attribute location (after linking!)

	float points[] = {
		//Position				//UV
		-1.0f, -1.0f,  0.0f,	0.0f,  0.0f, //Bottom left
		 1.0f,  1.0f,  0.0f,	1.0f,  1.0f, //Top-right
		-1.0f,  1.0f,  0.0f,	0.0f,  1.0f, //Top-left
		-1.0f, -1.0f,  0.0f,	0.0f,  0.0f, //Bottom-left
		 1.0f, -1.0f,  0.0f,	1.0f,  0.0f, //Bottom-right
		 1.0f,  1.0f,  0.0f,	1.0f,  1.0f  //Top-right
	};

	//Add a description to it's buffer
	screenQuadVBO->AddBufferDescriptor({		//Vertex Position Attribute
		vp_loc,						//Location ID
		3,							//Size of attribute (3 = XYZ)
		GL_FLOAT,					//Type of attribute
		false,						//Not normalized
		sizeof(float) * 5,			//Size of buffer block per vertex (3 for XYZ and 2 for UV)
		(void*)(0 * sizeof(float))	//Stride of 0 bytes (starts at the beginning of the block)
									   });
	screenQuadVBO->AddBufferDescriptor({		//Vertex UV Attribute
		vc_loc,						//Location ID
		2,							//Size of attribute (2 = UV)
		GL_FLOAT,					//Type of attribute
		false,						//Not normalized
		sizeof(float) * 5,			//Size of buffer block per vertex (3 for XYZ and 2 for UV)
		(void*)(3 * sizeof(float))	//Stride of 3 bytes (starts 3 bytes away from the beginning of the block)
									   });

	//Copy data to VertexBuffer Object
	screenQuadVBO->Fill(sizeof(points), points);

	//Enable locations for the created shader program
	screenQuadVAO->EnableLocations(defaultPrg);

	//Use VBO for setting data pointers
	screenQuadVBO->SetAttributePointers();

}

inline void rav::App::CreatePostProcess()
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	GLuint default_vs = rvGetShader("vertex_uv_vs");
	GLuint default_fs = rvGetShader("fragment_base_vs");

	char* blur_file;
	rvLoadFile("./data/blurPass.frag", blur_file, true);
	GLuint blur_fs = rvCreateShader("fragment_blur", blur_file, RV_FRAGMENT_SHADER);

	char* sobel_file;
	rvLoadFile("./data/fragment_sobel.frag", sobel_file, true);
	GLuint sobel_fs = rvCreateShader("fragment_sobel", sobel_file, RV_FRAGMENT_SHADER);

	char* gray_file;
	rvLoadFile("./data/fragment_gray.frag", gray_file, true);
	GLuint gray_fs = rvCreateShader("fragment_gray", gray_file, RV_FRAGMENT_SHADER);

	postProcessPipeline = new PostProcess(default_vs, default_fs, width, height);
	postProcessPipeline->setScreenQuad(screenQuadVAO, screenQuadVBO);

	//decorate the post-process, adding more steps
	//postProcessPipeline = new PostProcessDecorator(postProcessPipeline, default_vs, sobel_fs, width, height);
	//postProcessPipeline = new PostProcessDecorator(postProcessPipeline, default_vs, blur_fs, width, height);


}

