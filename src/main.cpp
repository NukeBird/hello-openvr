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

        m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderSize.x, &m_nRenderSize.y);

        CreateFrameBuffer(m_nRenderSize.x, m_nRenderSize.y, leftEyeDesc);
        CreateFrameBuffer(m_nRenderSize.x, m_nRenderSize.y, rightEyeDesc);

        vr::VRInput()->GetActionHandle("/actions/demo/in/HideCubes", &m_actionHideCubes);
        vr::VRInput()->GetActionHandle("/actions/demo/in/HideThisController", &m_actionHideThisController);
        vr::VRInput()->GetActionHandle("/actions/demo/in/TriggerHaptic", &m_actionTriggerHaptic);
        vr::VRInput()->GetActionHandle("/actions/demo/in/AnalogInput", &m_actionAnalongInput);

        vr::VRInput()->GetActionSetHandle("/actions/demo", &m_actionsetDemo);

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
        if (!m_pHMD)
        {
            return;
        }

        HandleInput();
        PrintControllerAxes();

        glEnable(GL_MULTISAMPLE);

        // Left Eye
        glBindFramebuffer(GL_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId);
        glViewport(0, 0, m_nRenderSize.x, m_nRenderSize.y);
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //RenderScene(vr::Eye_Left);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDisable(GL_MULTISAMPLE);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, leftEyeDesc.m_nResolveFramebufferId);

        glBlitFramebuffer(0, 0, m_nRenderSize.x, m_nRenderSize.y, 0, 0, m_nRenderSize.x, m_nRenderSize.y,
            GL_COLOR_BUFFER_BIT,
            GL_LINEAR);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        glEnable(GL_MULTISAMPLE);

        // Right Eye
        glBindFramebuffer(GL_FRAMEBUFFER, rightEyeDesc.m_nRenderFramebufferId);
        glViewport(0, 0, m_nRenderSize.x, m_nRenderSize.y);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //RenderScene(vr::Eye_Right);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDisable(GL_MULTISAMPLE);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, rightEyeDesc.m_nRenderFramebufferId);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rightEyeDesc.m_nResolveFramebufferId);

        glBlitFramebuffer(0, 0, m_nRenderSize.x, m_nRenderSize.y, 0, 0, m_nRenderSize.x, m_nRenderSize.y,
            GL_COLOR_BUFFER_BIT,
            GL_LINEAR);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        
        vr::Texture_t leftEyeTexture = {(void*)(uintptr_t)leftEyeDesc.m_nResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture );
		vr::Texture_t rightEyeTexture = {(void*)(uintptr_t)rightEyeDesc.m_nResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture );
        
        if (m_iTrackedControllerCount != m_iTrackedControllerCount_Last || m_iValidPoseCount != m_iValidPoseCount_Last)
        {
            m_iValidPoseCount_Last = m_iValidPoseCount;
            m_iTrackedControllerCount_Last = m_iTrackedControllerCount;

            spdlog::info("PoseCount:{}({}) Controllers:{}\n", m_iValidPoseCount, m_strPoseClasses, m_iTrackedControllerCount);
        }

        UpdateHMDMatrixPose();

        //spdlog::info("Head {}", stringify(m_mat4HMDPose * glm::vec4(0, 0, 0, 1)));


        auto head_pos = glm::vec4(m_mat4HMDPose[3][0], m_mat4HMDPose[3][1], m_mat4HMDPose[3][2], 0);
        spdlog::info("Head pos {}", stringify(head_pos));
    }

    ~Application()
    {
        if (m_pHMD)
        {
            vr::VR_Shutdown();
            m_pHMD = nullptr;
        }
    }
private:
    void HandleInput()
    {
        vr::VREvent_t event;
        while (m_pHMD->PollNextEvent(&event, sizeof(event)))
        {
            ProcessVREvent(event);
        }

        // Process SteamVR action state
        // UpdateActionState is called each frame to update the state of the actions themselves. The application
        // controls which action sets are active with the provided array of VRActiveActionSet_t structs.
        vr::VRActiveActionSet_t actionSet = { 0 };
        actionSet.ulActionSet = m_actionsetDemo;
        vr::VRInput()->UpdateActionState(&actionSet, sizeof(actionSet), 1);

        vr::InputAnalogActionData_t analogData;
        if (vr::VRInput()->GetAnalogActionData(m_actionAnalongInput, &analogData, sizeof(analogData), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analogData.bActive)
        {
            m_vAnalogValue[0] = analogData.x;
            m_vAnalogValue[1] = analogData.y;
        }

        m_rHand[0].m_bShowController = true;
        m_rHand[1].m_bShowController = true;

        vr::VRInputValueHandle_t ulHideDevice;
        if (GetDigitalActionState(m_actionHideThisController, &ulHideDevice))
        {
            for (int i : {0, 1})
            {
                if (ulHideDevice == m_rHand[i].m_source)
                {
                    m_rHand[i].m_bShowController = false;
                }
            }
        }


        for (int i : {0, 1})
        {
            vr::InputPoseActionData_t poseData;
            if (vr::VRInput()->GetPoseActionDataForNextFrame(m_rHand[i].m_actionPose, vr::TrackingUniverseStanding, &poseData, sizeof(poseData), vr::k_ulInvalidInputValueHandle) != vr::VRInputError_None
                || !poseData.bActive || !poseData.pose.bPoseIsValid)
            {
                m_rHand[i].m_bShowController = false;
            }
            else
            {
                m_rHand[i].m_rmat4Pose = ConvertSteamVRMatrix(poseData.pose.mDeviceToAbsoluteTracking);

                vr::InputOriginInfo_t originInfo;
                if (vr::VRInput()->GetOriginTrackedDeviceInfo(poseData.activeOrigin, &originInfo, sizeof(originInfo)) == vr::VRInputError_None
                    && originInfo.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid)
                {
                    std::string sRenderModelName = GetTrackedDeviceString(originInfo.trackedDeviceIndex, vr::Prop_RenderModelName_String);
                    if (sRenderModelName != m_rHand[i].m_sRenderModelName)
                    {
                        //m_rHand[i].m_pRenderModel = FindOrLoadRenderModel(sRenderModelName.c_str());
                        m_rHand[i].m_sRenderModelName = sRenderModelName;
                    }
                }
            }
        }
    }

    struct FramebufferDesc
    {
        GLuint m_nDepthBufferId;
        GLuint m_nRenderTextureId;
        GLuint m_nRenderFramebufferId;
        GLuint m_nResolveTextureId;
        GLuint m_nResolveFramebufferId;
    };

    bool CreateFrameBuffer(int nWidth, int nHeight, FramebufferDesc& framebufferDesc)
    {
        glGenFramebuffers(1, &framebufferDesc.m_nRenderFramebufferId);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nRenderFramebufferId);

        glGenRenderbuffers(1, &framebufferDesc.m_nDepthBufferId);
        glBindRenderbuffer(GL_RENDERBUFFER, framebufferDesc.m_nDepthBufferId);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, nWidth, nHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebufferDesc.m_nDepthBufferId);

        glGenTextures(1, &framebufferDesc.m_nRenderTextureId);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.m_nRenderTextureId);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, nWidth, nHeight, true);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.m_nRenderTextureId, 0);

        glGenFramebuffers(1, &framebufferDesc.m_nResolveFramebufferId);
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nResolveFramebufferId);

        glGenTextures(1, &framebufferDesc.m_nResolveTextureId);
        glBindTexture(GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId, 0);

        // check FBO status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return true;
    }

    void UpdateHMDMatrixPose()
    {
        if (!m_pHMD)
        {
            return;
        }

        vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

        m_iValidPoseCount = 0;
        m_strPoseClasses = "";
        for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
        {
            if (m_rTrackedDevicePose[nDevice].bPoseIsValid)
            {
                m_iValidPoseCount++;
                m_rmat4DevicePose[nDevice] = ConvertSteamVRMatrix(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
                if (m_rDevClassChar[nDevice] == 0)
                {
                    switch (m_pHMD->GetTrackedDeviceClass(nDevice))
                    {
                    case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
                    case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
                    case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
                    case vr::TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
                    case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
                    default:                                       m_rDevClassChar[nDevice] = '?'; break;
                    }
                }
                m_strPoseClasses += m_rDevClassChar[nDevice];
            }
        }

        if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
        {
            m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
            m_mat4HMDPose = glm::inverse(m_mat4HMDPose);//custom_transpose(m_mat4HMDPose);//glm::transpose(m_mat4HMDPose); //invert?
        }
    }

    glm::mat4 custom_transpose(const glm::mat4& m)
    {
        glm::mat4 r;

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                r[i][j] = m[j][i];
            }
        }

        return r;
    }


    glm::mat4 ConvertSteamVRMatrix(const vr::HmdMatrix34_t& matPose)
    {
        glm::mat4 matrixObj(
            matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
            matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
            matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
            matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
        );

        return matrixObj;
    }


    // Returns true if the action is active and its state is true
    bool GetDigitalActionState(vr::VRActionHandle_t action, vr::VRInputValueHandle_t* pDevicePath = nullptr)
    {
        vr::InputDigitalActionData_t actionData;
        vr::VRInput()->GetDigitalActionData(action, &actionData, sizeof(actionData), vr::k_ulInvalidInputValueHandle);
        if (pDevicePath)
        {
            *pDevicePath = vr::k_ulInvalidInputValueHandle;
            if (actionData.bActive)
            {
                vr::InputOriginInfo_t originInfo;
                if (vr::VRInputError_None == vr::VRInput()->GetOriginTrackedDeviceInfo(actionData.activeOrigin, &originInfo, sizeof(originInfo)))
                {
                    *pDevicePath = originInfo.devicePath;
                }
            }
        }
        return actionData.bActive && actionData.bState;
    }

    void ProcessVREvent(const vr::VREvent_t& event)
    {
        switch (event.eventType)
        {
            case vr::VREvent_TrackedDeviceDeactivated:
            {
                spdlog::info("Device {} detached", event.trackedDeviceIndex);
                break;
            }
            case vr::VREvent_TrackedDeviceUpdated:
            {
                spdlog::info("Device {} updated", event.trackedDeviceIndex);
                break;
            }
        }
    }

    std::string stringify(const glm::vec4& v) //glm::to_string unavailable through Magnum/GlmIntegration
    {
        std::string result;

        result += "{ ";
        for (int i = 0; i < 4; ++i)
        {
            result += std::to_string(v[i]);
            result += ", ";
        }
        result += " }";

        return result;
    }
    
    void PrintControllerAxes()
    {
        if (!m_pHMD->IsInputAvailable())
        {
            return;
        }

        for (int i : {0, 1})
        {
            if (!m_rHand[i].m_bShowController)
            {
                continue;
            }

            glm::vec4 pos = m_rHand[i].m_rmat4Pose * glm::vec4(0, 0, 0, 1);
            spdlog::info("Hand[{}] position: {}", i, stringify(pos));
        }
    }

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

        return glm::inverse(matrixObj); //glm::transpose(matrixObj); //invert?
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
    vr::VRActionSetHandle_t m_actionsetDemo = vr::k_ulInvalidActionSetHandle; 
    vr::VRActionHandle_t m_actionHideCubes = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_actionHideThisController = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_actionTriggerHaptic = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_actionAnalongInput = vr::k_ulInvalidActionHandle;

    vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
    int m_iValidPoseCount = 0;
    int m_iValidPoseCount_Last = -1;
    int m_iTrackedControllerCount = 0;
    int m_iTrackedControllerCount_Last = -1;

    std::string m_strPoseClasses;
    glm::mat4 m_rmat4DevicePose[vr::k_unMaxTrackedDeviceCount];
    glm::mat4 m_mat4HMDPose;
    char m_rDevClassChar[vr::k_unMaxTrackedDeviceCount]{ 0 };   // for each device, a character representing its class

    glm::vec2 m_vAnalogValue;
    glm::uvec2 m_nRenderSize;

    FramebufferDesc leftEyeDesc;
    FramebufferDesc rightEyeDesc;
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
            glfwTerminate();
            return -1;
        }

        while (!glfwWindowShouldClose(window)) 
        {
            app.update();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glfwTerminate();
    }

    return 0;
}