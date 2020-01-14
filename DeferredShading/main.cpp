#include <iostream>
#include<vector>
#include "display.h"
#include "shader.h"
#include "mesh.h"
#include "model.h"
#include "camera.h"
#include "Constants.h"
#include "HandelKeys.h"
#include <GL/GL.h>
#include <GL/glext.h>

void genQueries(GLuint qid[][1]);
void swapQueryBuffers();

void DrawQuadGL();
void initGBuffer();
void initLights();

void DrawCubeGL();

int main(int args, char** argv)
{
    float dTheta = 0.01;

	Model floor("./res/models/floor/floor.obj");
    glm::mat4 modelFloor;
    modelFloor = glm::translate(modelFloor, glm::vec3(0.00f, -1.00f, 1.00f));
    modelFloor = glm::translate(modelFloor, -floor.GetCenter());
    modelFloor = glm::scale(modelFloor, glm::vec3(0.30f, 1.0f, 0.3f));	// it's a bit too big for our scene, so scale it down

	Model wall("./res/models/wall/wall.obj");
	glm::mat4 modelWall;
	modelWall = glm::rotate(modelWall, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	modelWall = glm::translate(modelWall, glm::vec3(0.0, 3.5, -1.0  ));
	modelWall = glm::scale(modelWall, glm::vec3(0.30f, 1.0, 0.3));	// it's a bit too big for our scene, so scale it down

	glm::mat4 modelScene;
	Model scene("./res/models/scene/scene.obj");
	modelScene = glm::translate(modelScene, glm::vec3(0.50f, -.800f, 2.0f));
	runtime = true; avgNumFrames = int(glm::radians(360.0f) * 20); csv = true;  dTheta = 0;

	glm::mat4 ModelLight;
	Model light("./res/models/square/square.obj");
	ModelLight = glm::mat4();
	ModelLight = glm::scale(ModelLight, glm::vec3(LIGHT_SIZE*10.0f, LIGHT_SIZE*10.0f, 1.0f));	
	ModelLight = glm::translate(ModelLight, lightPosition);

	// configure g-buffer framebuffer
	initGBuffer();
	initLights();
	
	// Load Shader for Deferred Shaders
	Shader shaderGeometryPass("./res/gBuffer.vs", "./res/gBuffer.fs");
	shaderGeometryPass.use();
	shaderGeometryPass.disable();

	Shader shaderLightingPass("./res/deferredShading.vs", "./res/deferredShading.fs");
	shaderLightingPass.use();
	shaderLightingPass.setInt("gPosition", 0);
	shaderLightingPass.setInt("gNormal", 1);
	shaderLightingPass.setInt("gAlbedoSpec", 2);
	shaderLightingPass.disable();

	Shader shaderLightBox("./res/deferredLightBox.vs", "./res/deferredLightBox.fs");
	shaderLightBox.use();
	shaderLightBox.disable();
	
	glEnable(GL_CULL_FACE);
	genQueries(queryID_VIR);
	genQueries(queryID_lightPass);
	lastTime = SDL_GetTicks();
	
	// Render:
    while (!display.isClosed()) {
		numFrames++;
        glFinish();
        handleKeys();
    	
        Camera camera(cameraPosition, fov, (float)WIDTH, (float)HEIGHT, zNear, zFar, lookAt, bbox);
        glm::mat4 projection = camera.GetPerspProj();
        glm::mat4 view = camera.GetView();
		//glm::mat4 viewProj = camera.GetPersViewProj();
    	
        display.Clear(0.0f, 0.0f, 0.0f, 1.0f);
    	if(dTheta > 0)
			modelScene = glm::rotate(modelScene, glm::sin(dTheta), glm::vec3(0.0f, 1.0f, 0.0f)); // If dTheta is non-zero.
		glBeginQuery(GL_TIME_ELAPSED, queryID_VIR[queryBackBuffer][0]);
    	
		/* Deferred shading */
		// 1. Geometry Pass: Render scene's geometry/color data into gbuffer
		glViewport(0, 0, WIDTH, HEIGHT);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			shaderGeometryPass.use();
			shaderGeometryPass.setMat4("u_projection", projection);
			shaderGeometryPass.setMat4("u_view", view);
			shaderGeometryPass.setMat4("u_model", modelFloor);
			floor.Draw(shaderGeometryPass);
			shaderGeometryPass.setMat4("u_model", modelWall);
			wall.Draw(shaderGeometryPass);
			shaderGeometryPass.setMat4("u_model", modelScene);
			scene.Draw(shaderGeometryPass);
			shaderGeometryPass.disable();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
    	
    	// 2a. Lighting pass: Calculate lighting by iterating over a screen filled
    	//		quad pixel-by-pixel using the gbuffer's content
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WIDTH, HEIGHT);
		shaderLightingPass.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    	for(unsigned int i = 0; i < lightPos.size(); i++)
    	{
			shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Position", lightPos[i]);
			shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Color", lightColors[i]);
    		// update attenuation parameters and calculate radius
			const float constant = 1.0f;
			const float linear = 0.7f;
			const float quadratic = 1.8f;
			shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Linear", linear);
			shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Quadratic", quadratic);
    	}
		shaderLightingPass.setVec3("viewPos", cameraPosition);
		DrawQuadGL();
		shaderLightingPass.disable();
    	
		// 2b. Copy the content of geometry's depth buffer to default
    	//		framebuffer's depth buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// 3. Render light
		shaderLightBox.use();
		shaderLightBox.setMat4("u_projection", projection);
		shaderLightBox.setMat4("u_view", view);
    	for(int i = 0; i < lightPos.size(); i++)
    	{
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, lightPos[i]);
			model = glm::scale(model, glm::vec3(0.01));
			shaderLightBox.setMat4("u_model", model);
			shaderLightBox.setVec3("lightColor", lightColors[i]);
			DrawQuadGL();
    	}
		shaderLightBox.disable();
    	
		glEndQuery(GL_TIME_ELAPSED);
        /*****************************************
         * Display time and number of points     *
        /*****************************************/
        glGetQueryObjectui64v(queryID_VIR[queryFrontBuffer][0], GL_QUERY_RESULT, &virTime);
        avgVIR_time += virTime / 1000000.0;
        if (numFrames % avgNumFrames == 0 && runtime) {
			std::cout << "PCSS: " << avgVIR_time / avgNumFrames;
			std::cout << "\tLIGHT SIZE: " << LIGHT_SIZE;
            std::cout << std::endl;
			avgVIR_time = 0;
        }		
        swapQueryBuffers();
        display.Update();        
    }

	// Free the buffers
	glDeleteFramebuffers(1, &m_shadowMapFBO);
	glDeleteTextures(1, &depthTexture);
	for(int i = 0; i < NumTextureUnits; i++)
	{
		glDeleteTextures(1, &m_textures[i]);
		glDeleteTextures(1, &m_samplers[i]);
	}
    return 0;
}

void initGBuffer()
{
	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	// position color buffer
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

	// Normal Color buffer
	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, WIDTH, HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

	// Color + specular color buffer
	glGenTextures(1, &gAlbedoSpec);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

	// depth
	/*glBindTexture(GL_TEXTURE_2D, m_depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, WIDTH, HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
		NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
	//*/
	// Tell oGL which color attachments we'll use for rendering
	GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	// Create and attach depth buffer
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WIDTH, HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

	// Check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer creation error!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);	
}

void initLights()
{
	srand(13);
	float X_increment = 10 * LIGHT_SIZE / NUM_LIGHTS;
	float Y_increment = 10 * LIGHT_SIZE / NUM_LIGHTS;
	for(unsigned int i = 0; i < NUM_LIGHTS; i++)
	{
		for (unsigned int j = 0; j < NUM_LIGHTS; j++)
		{
			glm::vec3 tempPos = lightPosition +
				(glm::vec3(X_increment * (i - NUM_LIGHTS/2.0), 0.0f, 0.0f)) +
				(glm::vec3(0.0f, Y_increment * (j - NUM_LIGHTS/2.0), 0.0f));
			lightPos.push_back(tempPos);
			float rColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
			float gColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
			float bColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
			lightColors.push_back(glm::vec3(rColor, gColor, bColor));
		}
	}
}

void DrawQuadGL()
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// call this function when initializating the OpenGL settings
void genQueries(GLuint qid[][1]) {

    glGenQueries(1, qid[queryBackBuffer]);
    glGenQueries(1, qid[queryFrontBuffer]);

    // dummy query to prevent OpenGL errors from popping out
    glQueryCounter(qid[queryFrontBuffer][0], GL_TIME_ELAPSED);
}

// aux function to keep the code simpler
void swapQueryBuffers() {

    if (queryBackBuffer) {
        queryBackBuffer = 0;
        queryFrontBuffer = 1;
    }
    else {
        queryBackBuffer = 1;
        queryFrontBuffer = 0;
    }
}
