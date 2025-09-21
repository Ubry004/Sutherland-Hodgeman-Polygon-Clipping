#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include <vector>
#include <cmath>

struct Vertex {
    float x, y;
};

bool inside(const Vertex& v, int boundary, float xmin, float ymin, float xmax, float ymax) {
    switch (boundary) {
    case 0: return v.x >= xmin; // Left
    case 1: return v.x <= xmax; // Right
    case 2: return v.y >= ymin; // Bottom
    case 3: return v.y <= ymax; // Top
    }
    return false;
}

Vertex intersect(const Vertex& v1, const Vertex& v2, int boundary, float xmin, float ymin, float xmax, float ymax) {
    Vertex res;
    float dx = v2.x - v1.x;
    float dy = v2.y - v1.y;

    switch (boundary) {
    case 0: // Left
        res.x = xmin;
        res.y = v1.y + dy * (xmin - v1.x) / dx;
        break;
    case 1: // Right
        res.x = xmax;
        res.y = v1.y + dy * (xmax - v1.x) / dx;
        break;
    case 2: // Bottom
        res.y = ymin;
        res.x = v1.x + dx * (ymin - v1.y) / dy;
        break;
    case 3: // Top
        res.y = ymax;
        res.x = v1.x + dx * (ymax - v1.y) / dy;
        break;
    }
    return res;
}

std::vector<Vertex> clipAgainstBoundary(
    const std::vector<Vertex>& input,
    int boundary,
    float xmin, float ymin, float xmax, float ymax)
{
    std::vector<Vertex> output;

    for (size_t i = 0; i < input.size(); i++) {
        Vertex current = input[i];
        Vertex prev = input[(i + input.size() - 1) % input.size()];

        bool currInside = inside(current, boundary, xmin, ymin, xmax, ymax);
        bool prevInside = inside(prev, boundary, xmin, ymin, xmax, ymax);

        if (prevInside && currInside) {
            output.push_back(current);
        }
        else if (prevInside && !currInside) {
            output.push_back(intersect(prev, current, boundary, xmin, ymin, xmax, ymax));
        }
        else if (!prevInside && currInside) {
            output.push_back(intersect(prev, current, boundary, xmin, ymin, xmax, ymax));
            output.push_back(current);
        }
    }
    return output;
}


std::vector<float> SutherlandHodgeman(
    const std::vector<float>& polygon, // flat: x0,y0,x1,y1,...
    float xmin, float ymin, float xmax, float ymax,
    int width, int height)
{
    // Convert flat array to Vertex list
    std::vector<Vertex> poly;
    for (size_t i = 0; i < polygon.size(); i += 2) {
        poly.push_back({ polygon[i], polygon[i + 1] });
    }

    // Clip against all 4 boundaries
    for (int b = 0; b < 4; b++) {
        poly = clipAgainstBoundary(poly, b, xmin, ymin, xmax, ymax);
    }

    // Convert back to NDC floats
    std::vector<float> ndc;
    for (auto& v : poly) {
        float ndcX = (2.0f * v.x) / width - 1.0f;
        float ndcY = (2.0f * v.y) / height - 1.0f;
        ndc.push_back(ndcX);
        ndc.push_back(ndcY);
    }
    return ndc;
}


std::string loadShaderSource(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open shader file: " << filePath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);


// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
int fbWidth = SCR_WIDTH;
int fbHeight = SCR_HEIGHT;
bool showClipped = true; // starts by showing clipped polygon


int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Sutherland-Hodgeman Polygon Clipping", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // Shaders
    // ------------------------------------

    std::string vertexCode = loadShaderSource("vertex_shader.glsl");
    std::string fragmentCode = loadShaderSource("fragment_shader.glsl");

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vShaderCode, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fShaderCode, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Buffers

    // Setup VAO and VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // position attribute (2 floats: x,y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // input
        // -----
        //processInput(window);

        // render
        // ------

        // Polygon in pixel space (Diamond)
        std::vector<float> polygon = {
            400,500,
            150,300,
            400,100,
            650,300
        };

        // Clip once per frame (pixel space clip window)
        std::vector<float> clipped = SutherlandHodgeman(polygon,
            200, 150, 600, 450,  // xmin,ymin,xmax,ymax (pixel coords) (300 by 400, centered)
            fbWidth, fbHeight);  // use framebuffer size

        // Choose which verts to upload (clipped or unclipped)
        std::vector<float> verts;
        if (showClipped) {
            verts = clipped;
        }
        else {
            // convert original polygon to NDC using current framebuffer size
            verts.reserve(polygon.size());
            for (size_t i = 0; i < polygon.size(); i += 2) {
                float ndcX = (2.0f * polygon[i]) / float(fbWidth) - 1.0f;
                float ndcY = (2.0f * polygon[i + 1]) / float(fbHeight) - 1.0f;
                verts.push_back(ndcX);
                verts.push_back(ndcY);
            }
        }

        // Upload to VBO
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

        // Draw calls
        size_t vertCount = verts.size() / 2;
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)vertCount);
        glBindVertexArray(0);

        // Swap buffers
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        showClipped = !showClipped;
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    fbWidth = width;
    fbHeight = height;
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{

}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

}