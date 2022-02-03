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
#include <spdlog/spdlog.h>

#include <vector>
#include <string>

class Application
{
public:
    bool init()
    {
        vr::EVRInitError eError = vr::VRInitError_None;
        m_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);

        if (eError != vr::VRInitError_None)
        {
            m_pHMD = nullptr;

            char buf[1024];
            sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));

            spdlog::error("OpenVR initialization failed: {}", buf);
            return false;
        }
        spdlog::info("OpenVR initialization successful");

        m_strDriver = GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
        spdlog::info("Tracking system name: {}", m_strDriver);

        m_strDisplay = GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
        spdlog::info("Serial number: {}", m_strDisplay);

        setup_cameras();

        vr::VRInput()->GetActionHandle("/actions/demo/out/Haptic_Left", &m_rHand[0].m_actionHaptic);
        vr::VRInput()->GetInputSourceHandle("/user/hand/left", &m_rHand[0].m_source);
        vr::VRInput()->GetActionHandle("/actions/demo/in/Hand_Left", &m_rHand[0].m_actionPose);

        vr::VRInput()->GetActionHandle("/actions/demo/out/Haptic_Right", &m_rHand[1].m_actionHaptic);
        vr::VRInput()->GetInputSourceHandle("/user/hand/right", &m_rHand[1].m_source);
        vr::VRInput()->GetActionHandle("/actions/demo/in/Hand_Right", &m_rHand[1].m_actionPose);

        return true;
    }

    void update()
    {

    }

    void shutdown()
    {

    }
private:
    glm::mat4 GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
    {
        if (!m_pHMD)
        {
            return glm::mat4(1.0);
        }

        vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(nEye, camera_range[0], camera_range[1]);

        return glm::mat4(
            mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
            mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
            mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
            mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
        );
    }

    glm::mat4 GetHMDMatrixPoseEye(vr::Hmd_Eye nEye)
    {
        if (!m_pHMD)
        {
            return glm::mat4(1.0);
        }

        vr::HmdMatrix34_t matEyeRight = m_pHMD->GetEyeToHeadTransform(nEye);
        glm::mat4 matrixObj(
            matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0,
            matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
            matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
            matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f
        );

        return glm::transpose(matrixObj); //invert?
    }

    void setup_cameras()
    {
        m_mat4ProjectionLeft = GetHMDMatrixProjectionEye(vr::Eye_Left);
        m_mat4ProjectionRight = GetHMDMatrixProjectionEye(vr::Eye_Right);
        m_mat4eyePosLeft = GetHMDMatrixPoseEye(vr::Eye_Left);
        m_mat4eyePosRight = GetHMDMatrixPoseEye(vr::Eye_Right);
    }

    std::string GetTrackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError = nullptr)
    {
        uint32_t unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
        if (unRequiredBufferLen == 0)
            return "";

        char* pchBuffer = new char[unRequiredBufferLen];
        unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
        std::string sResult = pchBuffer;
        delete[] pchBuffer;
        return sResult;
    }

    vr::IVRSystem* m_pHMD = nullptr;
    std::string m_strDriver = "No Driver";
    std::string m_strDisplay = "No Display";

    glm::mat4 m_mat4ProjectionLeft;
    glm::mat4 m_mat4ProjectionRight;

    glm::mat4 m_mat4eyePosLeft;
    glm::mat4 m_mat4eyePosRight;

    glm::vec2 camera_range{ 0.1, 30.0 };

    struct ControllerInfo_t
    {
        vr::VRInputValueHandle_t m_source = vr::k_ulInvalidInputValueHandle;
        vr::VRActionHandle_t m_actionPose = vr::k_ulInvalidActionHandle;
        vr::VRActionHandle_t m_actionHaptic = vr::k_ulInvalidActionHandle;
        glm::mat4 m_rmat4Pose;
        std::string m_sRenderModelName;
        bool m_bShowController;
    };

    ControllerInfo_t m_rHand[2];
};

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

        Application app;

        if (!app.init())
        {
            app.shutdown();
            glfwTerminate();
            return -1;
        }

        while (!glfwWindowShouldClose(window)) 
        {
            app.update();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        app.shutdown();
        glfwTerminate();
    }

    return 0;
}