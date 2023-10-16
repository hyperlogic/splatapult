// 3d gaussian splat renderer

#include <algorithm>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <memory>
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdlib.h> //rand()

#include "debugdraw.h"
#include "flycam.h"
#include "gaussiancloud.h"
#include "image.h"
#include "joystick.h"
#include "log.h"
#include "pointcloud.h"
#include "program.h"
#include "texture.h"
#include "util.h"
#include "vertexbuffer.h"

static bool quitting = false;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

const float Z_NEAR = 0.1f;
const float Z_FAR = 1000.0f;
const float FOVY = glm::radians(45.0f);

std::shared_ptr<GaussianCloud> LoadGaussianCloud()
{
    auto gaussianCloud = std::make_shared<GaussianCloud>();

    /*
    if (!gaussianCloud->ImportPly("data/point_cloud.ply"))
    {
        Log::printf("Error loading GaussianCloud!\n");
        return nullptr;
    }
    */

    //
    // make an example GaussianClound, that contain red, green and blue axes.
    //
    std::vector<GaussianCloud::Gaussian>& gaussianVec = gaussianCloud->GetGaussianVec();
    const float AXIS_LENGTH = 1.0f;
    const int NUM_SPLATS = 2;
    const float DELTA = (AXIS_LENGTH / (float)NUM_SPLATS);
    const float S = logf(0.05f);
    const float SH_C0 = 0.28209479177387814f;
    const float SH_ONE = 1.0f / (2.0f * SH_C0);
    const float SH_ZERO = -1.0f / (2.0f * SH_C0);

    /*
    // x axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = i * DELTA + DELTA;
        g.position[1] = 0.0f;
        g.position[2] = 0.0f;
        // red
        g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    // y axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = 0.0f;
        g.position[1] = i * DELTA + DELTA;
        g.position[2] = 0.0f;
        // green
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    // z axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = 0.0f;
        g.position[1] = 0.0f;
        g.position[2] = i * DELTA + DELTA;
        // blue
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ONE;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    */

    GaussianCloud::Gaussian g;
    memset(&g, 0, sizeof(GaussianCloud::Gaussian));
    g.position[0] = 0.0f;
    g.position[1] = 0.0f;
    g.position[2] = 0.0f;
    // white
    g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ONE;
    g.opacity = 100.0f;
    g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
    g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    gaussianVec.push_back(g);

    return gaussianCloud;
}

int SDLCALL Watch(void *userdata, SDL_Event* event)
{
    if (event->type == SDL_APP_WILLENTERBACKGROUND)
    {
        quitting = true;
    }

    return 1;
}

static bool s_debug = false;

struct Gaussian
{
    Gaussian() {}
    Gaussian(const glm::vec3& pIn, const glm::mat3& covIn, const glm::vec3& colorIn, float alphaIn) :
        p(pIn), cov(covIn), color(colorIn), alpha(alphaIn)
    {
        k = 1.0f / (15.7496f * sqrtf(glm::determinant(cov)));
        covInv = glm::inverse(cov);
    }

    float Eval(const glm::vec3& x) const
    {
        glm::vec3 d = x - p;

        if (s_debug)
        {
            PrintVec(d, "    d");
            PrintMat(cov, "    cov");
            Log::printf("    k = %.5f\n", k);
        }

        return k * expf(-0.5f * glm::dot(d, covInv * d));
    }

    glm::vec3 p;
    glm::mat3 cov;
    glm::vec3 color;
    float alpha;

    float k;
    glm::mat3 covInv;
};


std::shared_ptr<std::vector<Gaussian>> BuildGaussianVec(std::shared_ptr<const GaussianCloud> gCloud)
{
    auto gVec = std::make_shared<std::vector<Gaussian>>();
    gVec->reserve(gCloud->GetGaussianVec().size());
    for (auto&& g : gCloud->GetGaussianVec())
    {

        const float SH_C0 = 0.28209479177387814f;
        float alpha = 1.0f / (1.0f + expf(-g.opacity));
        glm::vec3 color(0.5f + SH_C0 * g.f_dc[0],
                        0.5f + SH_C0 * g.f_dc[1],
                        0.5f + SH_C0 * g.f_dc[2]);
        Gaussian gg(glm::vec3(g.position[0], g.position[1], g.position[2]),
                    g.ComputeCovMat(),
                    color, alpha);

        gVec->push_back(gg);
    }
    return gVec;
}

void Render(std::shared_ptr<std::vector<Gaussian>> gVec, const glm::mat4& camMat)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    glm::mat3 camMat3 = glm::mat3(camMat);

    const float aspect = (float)width / (float)height;

    glm::vec3 x0 = glm::vec3(camMat[3]);

    const int PIXEL_STEP = 2;
    const float theta0 = FOVY / 2.0f;
    const float dTheta = -FOVY / (float)height;
    for (int y = 0; y < height; y += PIXEL_STEP)
    {
        float theta = theta0 + dTheta * y;
        float phi0 = -FOVY * aspect / 2.0f;
        float dPhi = (FOVY * aspect) / (float)width;
        for (int x = 0; x < width; x += PIXEL_STEP)
        {
            float phi = phi0 + dPhi * x;
            glm::vec3 localRay(sinf(phi), sinf(theta), -cosf(theta));
            glm::vec3 ray = glm::normalize(camMat3 * localRay);

            s_debug = false;
            if (x == width/2 - 10 && y == height/2 - 10)
            {
                s_debug = true;
                Log::printf("pixel[%d, %d] ray\n", x, y);
                PrintVec(ray, "    ray");
            }

            // integrate along the ray, (midpoint rule)
            const float RAY_LENGTH = 5.0f;
            const int NUM_STEPS = 50;
            float t0 = 0.1f;
            float dt = (RAY_LENGTH - t0) / NUM_STEPS;
            glm::vec3 accum(0.0f, 0.0f, 0.0f);
            for (int i = 0; i < NUM_STEPS; i++)
            {
                float t = t0 + i * dt;
                for (auto&& g : *gVec)
                {
                    float gg = g.Eval(x0 + ray * t);

                    if (s_debug)
                    {
                        Log::printf("    ** gg = %.5f\n", gg);
                    }
                    accum += (gg * dt);
                }
            }
            if (s_debug)
            {
                Log::printf("integral = %.5f\n", accum.x);
            }

            accum *= 0.025f;

            // draw
            /*
            uint8_t r = (uint8_t)(glm::clamp(accum.x, 0.0f, 1.0f) * 255.0f);
            uint8_t g = (uint8_t)(glm::clamp(accum.y, 0.0f, 1.0f) * 255.0f);
            uint8_t b = (uint8_t)(glm::clamp(accum.z, 0.0f, 1.0f) * 255.0f);
            */
            uint8_t r = (uint8_t)(glm::clamp(accum.x, 0.0f, 1.0f) * 255.0f);
            uint8_t g = (uint8_t)(glm::clamp(accum.y, 0.0f, 1.0f) * 255.0f);
            uint8_t b = (uint8_t)(glm::clamp(accum.z, 0.0f, 1.0f) * 255.0f);
            //uint8_t a = (uint8_t)(glm::clamp(accum, 0.0f, 1.0f) * 255.0f);

            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_VIDEO) != 0)
    {
        Log::printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    const int32_t WIDTH = 100;
    const int32_t HEIGHT = 100;
    window = SDL_CreateWindow("3dgstoy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                              SDL_WINDOW_RESIZABLE);

	renderer = SDL_CreateRenderer(window, -1, 0);
	if (!renderer)
    {
        Log::printf("Failed to SDL Renderer: %s\n", SDL_GetError());
		return 1;
	}

    JOYSTICK_Init();

    SDL_AddEventWatch(Watch, NULL);

    auto gaussianCloud = LoadGaussianCloud();
    if (!gaussianCloud)
    {
        Log::printf("Error loading GaussianCloud\n");
        return 1;
    }

    // AJT: Hack
    gaussianCloud->ExportPly("data/single_gaussian.ply");

    auto gaussianVec = BuildGaussianVec(gaussianCloud);

    const float MOVE_SPEED = 2.5f;
    const float ROT_SPEED = 1.0f;

    /*
      [{"id": 0, "img_name": "00001", "width": 1959, "height": 1090,
  "position": [-3.0089893469241797, -0.11086489695181866, -3.7527640949141428],
  "rotation": [[0.876134201218856, 0.06925962026449776, 0.47706599800804744],
               [-0.04747421839895102, 0.9972110940209488, -0.057586739349882114],
               [-0.4797239414934443, 0.027805376500959853, 0.8769787916452908]],
  "fy": 1164.6601287484507, "fx": 1159.5880733038064},
    */

    // AJT HACK: for train model, first camera in json
    glm::vec3 trainPos(-3.0089893469241797f, -0.11086489695181866f, -3.7527640949141428f);
    glm::mat4 trainCam(glm::vec4(0.876134201218856f, 0.06925962026449776f, 0.47706599800804744f, trainPos.x),
                       glm::vec4(-0.04747421839895102, 0.9972110940209488, -0.057586739349882114, trainPos.y),
                       glm::vec4(-0.4797239414934443, 0.027805376500959853, 0.8769787916452908, trainPos.z),
                       glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    trainCam = glm::transpose(trainCam);

    // convert cam into y-up and -z forward.
    glm::mat4 cam(-glm::vec4(-glm::vec3(trainCam[0]), 0.0f),
                  glm::vec4(-glm::vec3(trainCam[1]), 0.0f),
                  glm::vec4(-glm::vec3(trainCam[2]), 0.0f),
                  glm::vec4(trainPos, 1.0f));

    //FlyCam flyCam(trainPos, glm::quat(glm::mat3(cam)), MOVE_SPEED, ROT_SPEED);
    FlyCam flyCam(glm::vec3(0.0f, 0.0f, 1.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), MOVE_SPEED, ROT_SPEED);
    SDL_JoystickEventState(SDL_ENABLE);

    uint32_t frameCount = 1;
    uint32_t frameTicks = SDL_GetTicks();
    uint32_t lastTicks = SDL_GetTicks();
    while (!quitting)
    {
        // update dt
        uint32_t ticks = SDL_GetTicks();

        const int FPS_FRAMES = 100;
        if ((frameCount % FPS_FRAMES) == 0)
        {
            float delta = (ticks - frameTicks) / 1000.0f;
            float fps = (float)FPS_FRAMES / delta;
            frameTicks = ticks;
            Log::printf("fps = %.2f\n", fps);
        }
        float dt = (ticks - lastTicks) / 1000.0f;
        lastTicks = ticks;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                quitting = true;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                // ESC to quit
                if (event.key.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    quitting = true;
                }
                break;
            case SDL_JOYAXISMOTION:
				JOYSTICK_UpdateMotion(&event.jaxis);
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				JOYSTICK_UpdateButton(&event.jbutton);
				break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    // The window was resized
                    int newWidth = event.window.data1;
                    int newHeight = event.window.data2;
                    // Handle the resize event, e.g., adjust your viewport, etc.
                    //Resize(newWidth, newHeight);
                    SDL_RenderSetLogicalSize(renderer, newWidth, newHeight);
                }
                break;
            }
        }

        Joystick* joystick = JOYSTICK_GetJoystick();
        if (joystick)
        {
            float roll = 0.0f;
            roll -= (joystick->buttonStateFlags & (1 << Joystick::LeftBumper)) ? 1.0f : 0.0f;
            roll += (joystick->buttonStateFlags & (1 << Joystick::RightBumper)) ? 1.0f : 0.0f;
            flyCam.SetInput(glm::vec2(joystick->axes[Joystick::LeftStickX], joystick->axes[Joystick::LeftStickY]),
                            glm::vec2(joystick->axes[Joystick::RightStickX], joystick->axes[Joystick::RightStickY]), roll);
            flyCam.Process(dt);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        int width, height;
        SDL_GetWindowSize(window, &width, &height);

        glm::mat4 cameraMat = flyCam.GetCameraMat();

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawPoint(renderer, width / 2, height / 2);

        Render(gaussianVec, cameraMat);

        //RenderPointCloud(pointProg, pointTex, pointCloud, pointCloudVAO, cameraMat);
        //RenderSplats(splatProg, gaussianCloud, splatVAO, cameraMat);

        //DebugDraw_Transform(cam);


        frameCount++;

        SDL_RenderPresent(renderer);
    }

    DebugDraw_Shutdown();
    JOYSTICK_Shutdown();
    SDL_DelEventWatch(Watch, NULL);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
