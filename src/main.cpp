#include <openvr/openvr.h>
#include <Magnum/Magnum.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <Magnum/Shaders/VertexColor.h>
#include <GLFW/glfw3.h>
#include <Magnum/GlmIntegration/Integration.h>
#include <Magnum/GlmIntegration/GtcIntegration.h>
#include <Magnum/GlmIntegration/GtxIntegration.h>

#include <vector>

int main(int argc, char** argv) 
{
    if (!glfwInit()) return -1;

    glm::ivec2 window_size{ 800, 600 };

    GLFWwindow* const window = glfwCreateWindow(window_size.x, window_size.y, "GLFW", nullptr, nullptr);

    if (!window) 
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    {
        Magnum::Platform::GLContext ctx{ argc, argv };

        while (!glfwWindowShouldClose(window)) 
        {
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glfwTerminate();
    }

    return 0;
}