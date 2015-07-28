#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include "shader.h"
#include "model.h"
#include "camera.h"
#include "map.h"
#include "log_macros.h"
#include "log_stdout.h"
#include "imgui.h"
#include "imgui_gflw.h"
#include "zone_map.h"
#include "water_map.h"

int main(int argc, char **argv)
{
	eqLogInit(EQEMU_LOG_LEVEL);
	eqLogRegister(std::shared_ptr<EQEmu::Log::LogBase>(new EQEmu::Log::LogStdOut()));

	if(!glfwInit()) {
		eqLogMessage(LogFatal, "Couldn't init graphical system.");
		return -1;
	}

	std::string filename = "tutorialb";
	if(argc >= 2) {
		filename = argv[1];
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
#ifndef EQEMU_GL_DEP
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, 0);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_RESIZABLE, 0);
#endif

	GLFWwindow *win = glfwCreateWindow(1280, 720, "Map View", nullptr, nullptr);
	if(!win) {
		eqLogMessage(LogFatal, "Couldn't create an OpenGL window.");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(win);

	glewExperimental = GL_TRUE;
	if(glewInit() != GLEW_OK) {
		eqLogMessage(LogFatal, "Couldn't init glew.");
		glfwTerminate();
		return -1;
	}

	glfwSetInputMode(win, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	glfwSetCursorPos(win, 1280 / 2, 720 / 2);

#ifndef EQEMU_GL_DEP
	ShaderProgram shader("shaders/basic.vert", "shaders/basic.frag");
#else
	ShaderProgram shader("shaders/basic130.vert", "shaders/basic130.frag");
#endif
	ShaderUniform uniform = shader.GetUniformLocation("MVP");
	ShaderUniform tint = shader.GetUniformLocation("Tint");

	Model *collide = nullptr;
	Model *invis = nullptr;
	Model *volume = nullptr;
	LoadMap(filename, &collide, &invis);
	LoadWaterMap(filename, &volume);

	std::unique_ptr<ZoneMap> z_map = std::unique_ptr<ZoneMap>(ZoneMap::LoadMapFile(filename));
	std::unique_ptr<WaterMap> w_map = std::unique_ptr<WaterMap>(WaterMap::LoadWaterMapfile(filename));

	if(collide == nullptr)
		eqLogMessage(LogWarn, "Couldn't load zone geometry from map file.");

	if (volume == nullptr)
		eqLogMessage(LogWarn, "Couldn't load zone areas from map file.");

	Camera cam(1280, 720, 45.0f, 0.1f, 15000.0f);

	ImVec4 clear_color = ImColor(114, 144, 154);
	ImGui_ImplGlfwGL3_Init(win, true);

	bool rendering = true;
	bool r_c = true;
	bool r_nc = true;
	bool r_vol = true;
	do {
		double current_frame_time = glfwGetTime();

		cam.UpdateInputs(win);

		ImGuiIO& io = ImGui::GetIO();
		glfwPollEvents();
		ImGui_ImplGlfwGL3_NewFrame();

		{
			glm::vec3 loc = cam.GetLoc();
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Text("Zone: %s", filename.c_str());
			ImGui::Text("%.2f, %.2f, %.2f", loc.x, loc.z, loc.y);
			if(z_map && w_map) {
				ImGui::Text("Best Z: %.2f, In Liquid: %s", z_map->FindBestZ(ZoneMap::Vertex(loc.x, loc.z, loc.y), nullptr),
							w_map->InLiquid(loc.x, loc.z, loc.y) ? "true" : "false");
			} else if(z_map) {
				ImGui::Text("Best Z: %.2f", z_map->FindBestZ(ZoneMap::Vertex(loc.x, loc.z, loc.y), nullptr));
			}
			else if(w_map) {
				ImGui::Text("In Liquid: %s", w_map->InLiquid(loc.x, loc.z, loc.y) ? "true" : "false");
			}
		}

		{
			ImGui::Begin("Options");
			ImGui::Checkbox("Render Collidable Polygons", &r_c);
			ImGui::Checkbox("Render Non-Collidable Polygons", &r_nc);
			ImGui::Checkbox("Render Loaded Volumes", &r_vol);
			ImGui::End();
		}
		
		if(glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(win) != 0)
			rendering = false;
		
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glDisable(GL_BLEND);
		
		shader.Use();
		
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		
		glm::mat4 model = glm::mat4(1.0);
		glm::mat4 mvp = cam.GetProjMat() * cam.GetViewMat() * model;
		uniform.SetValueMatrix4(1, false, &mvp[0][0]);
		
		glm::vec4 tnt(0.8f, 0.8f, 0.8f, 1.0f);
		tint.SetValuePtr4(1, &tnt[0]);
		
		if (collide && r_c)
			collide->Draw();
		
		tnt[0] = 0.5f;
		tnt[1] = 0.7f;
		tnt[2] = 1.0f;
		tint.SetValuePtr4(1, &tnt[0]);
		
		if (invis && r_nc)
			invis->Draw();
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		tnt[0] = 0.0f;
		tnt[1] = 0.0f;
		tnt[2] = 0.8f;
		tnt[3] = 0.2f;
		tint.SetValuePtr4(1, &tnt[0]);
		
		if (volume && r_vol)
			volume->Draw();
		
		glDisable(GL_BLEND);
		
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		
		tnt[0] = 0.0f;
		tnt[1] = 0.0f;
		tnt[2] = 0.0f;
		tnt[3] = 0.0f;
		tint.SetValuePtr4(1, &tnt[0]);
		
		if (collide && r_c)
			collide->Draw();
		
		if (invis && r_nc)
			invis->Draw();
		
		if (volume && r_vol)
			volume->Draw();

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		ImGui::Render();

		glfwSwapBuffers(win);
	} while (rendering);

	if(collide)
		delete collide;

	if (invis)
		delete invis;

	if (volume)
		delete volume;

	glfwTerminate();
	return 0;
}
