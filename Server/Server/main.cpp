#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>        // For sockaddr_in
#include <windows.h>         // For CRITICAL_SECTION, CONDITION_VARIABLE
#include <time.h>            // For timeval (if needed)
#endif

#include <WS2tcpip.h>

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <tchar.h>
#include <algorithm>
#include <atomic>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#define IMGUI_IMPL_GLFW 
#define IMGUI_IMPL_OPENGL3 

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>
#include <rfb/rfb.h>

#include "Constants.h"

GLFWwindow* _window;
rfbScreenInfoPtr _screen;

rfbSocket _udpSocket;
sockaddr_in _clientAddr;
socklen_t _clientAddrLen = sizeof(_clientAddr);

int _windowWidth = WINDOW_WIDTH;
int _windowHeight = WINDOW_HEIGHT;

struct ivec2
{
    int x = 0;
    int y = 0;
};
struct ivec2x2
{
    ivec2 v1;
    ivec2 v2;
};

static std::atomic<int> s_MouseX{ 0 };
static std::atomic<int> s_MouseY{ 0 };

std::thread _receivingThread;
std::thread _receivingThreadWindowSize;
static std::atomic<bool> s_isTRunning{ false };
static std::atomic<bool> s_isT2Running{ false };

std::mutex _frameBufferResizeMutex;

bool _windowResized = false;

void receiveMousePosFromServer(rfbSocket udpSocket, sockaddr_in clientAddr, socklen_t clientAddrLen);
void receiveWindowSizeFromServer(rfbSocket udpSocket, sockaddr_in clientAddr, socklen_t clientAddrLen);
void sendNewFBSizeToClient(int width, int height);

void processNewFBSize(int width, int height)
{
    // round width to a multiple of 4
    if (width % 4 != 0)
    {
        width = (width + 3) & ~3;
    }
    // round height to a multiple of 4
    if (height % 4 != 0)
    {
        height = (height + 3) & ~3;
    }

    _windowWidth = width;
    _windowHeight = height;
    glViewport(0, 0, width, height);

    size_t newFBSize = width * height * BYTES_PER_PIXEL;

    char* buffer = (char*)malloc(newFBSize);
    if (buffer)
    {
        memset(buffer, 0, newFBSize);
    }

    // assign new buffer before freeing the old buffer.
    char* oldBuffer = _screen->frameBuffer;
    _screen->frameBuffer = buffer;
    rfbNewFramebuffer(_screen, buffer, width, height, 8, 3, BYTES_PER_PIXEL);
    // Notify the client before scaling the framebuffer
    _windowResized = true;
    free(oldBuffer);
    _screen->serverFormat.redShift = 16;
    _screen->serverFormat.greenShift = 8;
    _screen->serverFormat.blueShift = 0;
    _screen->width = width;
    _screen->height = height;
}

#pragma region GLFW Callbacks

void GLFWFrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
    processNewFBSize(width, height);
    
    sendNewFBSizeToClient(width, height);
    //rfbMarkRectAsModified(_screen, 0, 0, width, height);
}

void GLFWCursorMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
    // check if its focused, only set the mouse pos value https://www.glfw.org/docs/3.3/window_guide.html#window_attribs
    if (glfwGetWindowAttrib(window, GLFW_FOCUSED))
    {
        s_MouseX.store((int)xpos, std::memory_order_relaxed);
        s_MouseY.store((int)ypos, std::memory_order_relaxed);
    }
}

#pragma endregion

#pragma region ImGui Init and Terminate


void InitImGui(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void TerminateImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

#pragma endregion

void SetupWSADLL()
{
    WSADATA wsaData;
    int wsaError;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaError = WSAStartup(wVersionRequested, &wsaData);
    if (wsaError != 0)
    {
        std::cout << "The Winsock dll not found :(\n";
        return;
    }
    else
    {
        std::cout << "The Winsock dll found :)\n";
        std::cout << "Status: " << wsaData.szSystemStatus << "\n\n";
    }
}

void sendNewFBSizeToClient(int width, int height)
{
    ivec2 fbSize;
    fbSize.x = width; fbSize.y = height;
    socklen_t clientAddrLen = sizeof(_clientAddr);
    int bytesSent = sendto(_udpSocket, reinterpret_cast<const char*>(&fbSize), sizeof(ivec2), 0, (SOCKADDR*)&_clientAddr, (int)clientAddrLen);
    if (bytesSent == SOCKET_ERROR)
    {
        std::cerr << "Failed to send mouse pos to the server!!! :" <<WSAGetLastError() << std::endl;
    }
}

void HandleKBD(rfbBool down, rfbKeySym key, rfbClientPtr client)
{
    ImGuiIO& io = ImGui::GetIO();
    if (down)
    {
        if (key == VK_BACK)
            io.AddKeyEvent(ImGuiKey_Backspace, true);
        io.AddInputCharacter(key);
    }
    else
    {
        if (key == VK_BACK)
            io.AddKeyEvent(ImGuiKey_Backspace, false);
    }
}

void HandleMouse(int buttonMask, int x, int y, struct _rfbClientRec* client)
{
    std::cout << "Mouse moved. x: " << x << " ,y: " << y << " Button: " << buttonMask << std::endl;

    // ImGUI imput handling
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(x, y);
    if (buttonMask == 1 * BUTTON_MASK_HASH_PRESS)
        io.MouseDown[0] = true;
    else if (buttonMask == 2 * BUTTON_MASK_HASH_PRESS)
        io.MouseDown[1] = true;

    if (buttonMask == 1 * BUTTON_MASK_HASH_RELEASE)
        io.MouseDown[0] = false;
    else if (buttonMask == 2 * BUTTON_MASK_HASH_RELEASE)
        io.MouseDown[1] = false;
}

//#pragma region UDP - Deprecated
//// DATA TOO LARGE
//void SendDataOnUDPSock(char* data, size_t size)
//{
//    int bytesSent = sendto(_udpSocket, data, size, 0, (SOCKADDR*)&_clientAddr, _clientAddrLen);
//    if (bytesSent < 0)
//    {
//        std::cerr << "Failed to send FB to client: " << WSAGetLastError() << std::endl;
//    }
//}
//
//// SENDING IN CHUNKS BECAUSE DATA IS TOO LARGE
//void SendDataOnUDPSockInChunk(char* data, size_t size)
//{
//    // save some memory
//#pragma pack(push, 1)
//    struct ChunkHeader
//    {
//        uint32_t frameId;
//        uint32_t totalChunks;
//        uint32_t chunkIndex;
//        uint16_t payloadSize;
//    };
//#pragma pack(pop)
//
//    const size_t ChunkHeaderSize = sizeof(ChunkHeader);
//    const size_t ChunkSize = MAX_UDP_PAYLOAD - ChunkHeaderSize;
//
//    size_t totalRequiredChunks = (size + ChunkSize - 1) / ChunkSize;
//
//    uint32_t frameId = 1;
//
//    for (size_t index = 0; index < totalRequiredChunks; index++)
//    {
//        size_t offset = index * ChunkSize;
//        size_t actualChunkSize = std::min(ChunkSize, size - offset);
//
//        ChunkHeader chunkHeader;
//        chunkHeader.frameId = frameId;
//        chunkHeader.totalChunks = totalRequiredChunks;
//        chunkHeader.chunkIndex = index;
//        chunkHeader.payloadSize = actualChunkSize;
//
//        std::vector<char> packetBuffer(ChunkHeaderSize + actualChunkSize);
//        memcpy(packetBuffer.data(), &chunkHeader, ChunkHeaderSize);
//        memcpy(packetBuffer.data() + ChunkHeaderSize, data + offset, actualChunkSize);
//        
//        // send away
//        int bytesSent = sendto(_udpSocket, data + offset, static_cast<int>(packetBuffer.size()), 0, (SOCKADDR*)&_clientAddr, _clientAddrLen);
//        if (bytesSent < 0)
//        {
//            std::cerr << "Failed to send FB packet to client: " << WSAGetLastError() << std::endl;
//            return;
//        }
//    }
//}
//
//#pragma endregion

#pragma region UDP 

void InitUDPSocket()
{
    _udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (_udpSocket == INVALID_SOCKET)
    {
        std::cerr << "Failed to initialize udp socket!!!" << std::endl;
        exit(1);
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    // https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#bind
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(UDP_PORT);

    if (bind(_udpSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "Socket bind failed: " << WSAGetLastError() << std::endl;
        std::cout << "Cleanup result: " << WSACleanup() << std::endl;
        exit(1);
    }
    _clientAddr.sin_family = AF_INET;
    _clientAddr.sin_port = htons(CLIENT_UDP_PORT);
    InetPton(AF_INET, _T("192.168.2.17"), &_clientAddr.sin_addr.S_un);

    if (connect(_udpSocket, (SOCKADDR*)&_clientAddr, _clientAddrLen) == SOCKET_ERROR)
    {
        std::cerr << "Connection failed!! " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(1);
    }

    // NON BLOCKING
    u_long arg = 1;
    ioctlsocket(_udpSocket, FIONBIO, &arg);
}

void startReceivingThreads()
{
    if (s_isTRunning)
        return;
    s_isTRunning = true;
    _receivingThread = std::thread(receiveMousePosFromServer, _udpSocket, _clientAddr, _clientAddrLen);
    _receivingThread.detach();

    if (s_isT2Running)
        return;
    s_isT2Running = true;
    _receivingThreadWindowSize = std::thread(receiveWindowSizeFromServer, _udpSocket, _clientAddr, _clientAddrLen);
    _receivingThreadWindowSize.detach();
}

void receiveMousePosFromServer(rfbSocket udpSocket, sockaddr_in clientAddr, socklen_t clientAddrLen)
{
    ivec2 latestPos; latestPos.x = 0; latestPos.y = 0;
    auto last = std::chrono::steady_clock::now();

    while (s_isTRunning)
    {
        ivec2 mousePos;

        int bytesRecvd = recvfrom(udpSocket, reinterpret_cast<char*>(&mousePos), sizeof(mousePos), 0, (SOCKADDR*)&clientAddr, &clientAddrLen);
        int error = WSAGetLastError();
        if (bytesRecvd == SOCKET_ERROR)
        {
            if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }
        else if (bytesRecvd == sizeof(ivec2))
        {
            latestPos.x = mousePos.x; latestPos.y = mousePos.y;
        }
        
        auto now = std::chrono::steady_clock::now();
        // only on 10 ms intervals update atomic pos, to reduce write intervals
        if (now - last >= std::chrono::milliseconds(10))
        {
            s_MouseX.store(mousePos.x, std::memory_order_relaxed);
            s_MouseY.store(mousePos.y, std::memory_order_relaxed);
            std::cout << "Mouse moved. x: " << mousePos.x << " ,y: " << mousePos.y << std::endl;
            last = now;
        }
    }
}

void receiveWindowSizeFromServer(rfbSocket udpSocket, sockaddr_in clientAddr, socklen_t clientAddrLen)
{
    ivec2x2 size;
    
    while (s_isT2Running)
    {
        int bytesRecvd = recvfrom(udpSocket, reinterpret_cast<char*>(&size), sizeof(size), 0, (SOCKADDR*)&clientAddr, &clientAddrLen);
        if (bytesRecvd == SOCKET_ERROR)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        else if (bytesRecvd == sizeof(ivec2x2))
        {
            glfwSetWindowSize(_window, size.v2.x, size.v2.y);
        }
    }
}

void stopReceivingThreads()
{
    if (!s_isTRunning)
        return;
    s_isTRunning = false;

    if (_receivingThread.joinable())
    {
        _receivingThread.join();
    }

    if (!s_isT2Running)
        return;
    s_isT2Running = false;

    if (_receivingThreadWindowSize.joinable())
    {
        _receivingThreadWindowSize.join();
    }
}

#pragma endregion

int main()
{
    SetupWSADLL();

    /* Initialize the library */
    if (!glfwInit())
        return -1;


    // Create a windowed mode window and its OpenGL context
    _window = glfwCreateWindow(_windowWidth, _windowHeight, "VNC Server", NULL, NULL);
    if (!_window)
    {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(_window);

    if (!gladLoadGL())
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwSetFramebufferSizeCallback(_window, GLFWFrameBufferResizeCallback);
    glfwSetCursorPosCallback(_window, GLFWCursorMoveCallback);

    // ImGui stuff
    InitImGui(_window);
    float slider = 0.5f;
    char textBuffer[128] = "default text";


    // VNC SERVER
    _screen = rfbGetScreen(nullptr, nullptr, _windowWidth, _windowHeight, 8, 3, BYTES_PER_PIXEL);
    size_t fbSize = _windowWidth * _windowHeight * BYTES_PER_PIXEL;
    _screen->frameBuffer = (char*)malloc(fbSize);

    std::memset(_screen->frameBuffer, 0, fbSize);
    char* prevFB = new char[fbSize];
    memset(prevFB, 0, fbSize);
    size_t prevFBSize = fbSize;

    _screen->desktopName = strdup("VNC Server with ImGUI");
    _screen->serverFormat.redShift = 16;
    _screen->serverFormat.greenShift = 8;
    _screen->serverFormat.blueShift = 0;
    _screen->port = PORT;

    // UDP
    InitUDPSocket();
    startReceivingThreads();

    // Events
    _screen->kbdAddEvent = HandleKBD;
    _screen->ptrAddEvent = HandleMouse;
    // WHAT NAME IS THAT 'aa' ?!?!?!
    

    rfbInitServer(_screen);

    // https://brooker.co.za/blog/2024/05/09/nagle.html
    // optimize for low latency
    if (_screen->listenSock != INVALID_SOCKET)
    {
        int opt = 1;
        int res = setsockopt(_screen->listenSock, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
        if (res == 0)
        {
            std::cout << "TCP_NODELAY set successfully\n";
        }
        else
        {
            std::cerr << "TCP_NODELAP cannot be set: " << WSAGetLastError() << std::endl;
        }
    }

    auto last = std::chrono::steady_clock::now();

    glClearColor(0.5f, 1.0f, 1.0f, 1.0f);
    // Loop until the user closes the window 
    while (!glfwWindowShouldClose(_window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        // Start a new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::SliderFloat("Test Slider", &slider, 0.0f, 3.0f);
        ImGui::InputText("Some Input", textBuffer, IM_ARRAYSIZE(textBuffer));
        //ImGui::InputText("Input text", inputTextBuffer, 10);

        // IO
        ImGuiIO& io = ImGui::GetIO();
        int mouseX = s_MouseX.load(std::memory_order_relaxed);
        int mouseY = s_MouseY.load(std::memory_order_relaxed);
        io.MousePos = ImVec2(mouseX, mouseY);

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, _windowWidth, _windowHeight, GL_RGBA, GL_UNSIGNED_BYTE, _screen->frameBuffer);

        glfwSwapBuffers(_window);
        glfwPollEvents();

        int minX = _windowWidth, minY = _windowHeight, maxX = 0, maxY = 0;
        bool isChanged = false;

        if (_windowResized)
        {
            auto now = std::chrono::steady_clock::now();
            // only on 10 ms intervals update atomic pos, to reduce write intervals
            if (now - last >= std::chrono::milliseconds(10))
            {
                _windowResized = false;
                free(prevFB);
                fbSize = _windowWidth * _windowHeight * BYTES_PER_PIXEL;
                prevFBSize = fbSize;
                prevFB = new char[fbSize];
                memset(prevFB, 0, fbSize);
                now = last;
            }
            else
            {
                continue;
            }
        }

        for (int y = 0; y < _windowHeight; y += BLOCK_CHECKING_SIZE)
        {
            for (int x = 0; x < _windowWidth; x += BLOCK_CHECKING_SIZE)
            {
                int endX = (x + BLOCK_CHECKING_SIZE > _windowWidth) ? _windowWidth : (x + BLOCK_CHECKING_SIZE);
                int endY = (y + BLOCK_CHECKING_SIZE > _windowHeight) ? _windowHeight : (y + BLOCK_CHECKING_SIZE);

                bool dirtyBlock = false;
                int minX = endX, minY = endY, maxX = x, maxY = y;
                for (int j = y; j < endY; j++)
                {
                    int rowIndex = (j * _windowWidth + x) * BYTES_PER_PIXEL;
                    int rowLength = (endX - x) * BYTES_PER_PIXEL;
                    if (memcmp(_screen->frameBuffer + rowIndex, prevFB + rowIndex, rowLength) != 0)
                    {
                        dirtyBlock = true;
                        isChanged = true;
                        break;
                    }
                }
                if (dirtyBlock)
                    rfbMarkRectAsModified(_screen, x, y, endX, endY);
            }
        }

        // only modify if changed
        if (isChanged && !_windowResized)
        {
            // new FB
            if (_screen->width == _windowWidth && _screen->height == _windowHeight)
            {
                fbSize = _windowWidth * _windowHeight * BYTES_PER_PIXEL;
                if (prevFBSize == fbSize)
                    memcpy(prevFB, _screen->frameBuffer, fbSize);
            }
        }

        if (_screen && _screen->frameBuffer)
        {
            rfbProcessEvents(_screen, 10000);
        }
    }

    delete[] prevFB;
    rfbScreenCleanup(_screen);
    stopReceivingThreads();
    closesocket(_udpSocket);
    TerminateImGui();
    glfwTerminate();
    WSACleanup();
    return 0;
}


/*
#ifdef _WIN32
#include <winsock2.h>        // For sockaddr_in
#include <windows.h>         // For CRITICAL_SECTION, CONDITION_VARIABLE
#include <time.h>            // For timeval (if needed)
#endif


#include <iostream>
#include <cstring>
#include <vector>

#include <rfb/rfb.h>


#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Constants.h"


// https://libvnc.github.io/doc/html/libvncserver_doc.html
#pragma region vnc server

std::vector<char> framebuffer(_windowWidth* _windowHeight * 4);

rfbScreenInfoPtr _server = nullptr;

void HandleKBD(rfbBool down, rfbKeySym key, rfbClientPtr client)
{
    if (down && key == VK_ESCAPE)
    {
        rfbShutdownServer(_server, TRUE);
    }
}

void InitVNCServer(rfbScreenInfoPtr& server)
{
    server = rfbGetScreen(nullptr, nullptr, _windowWidth, _windowHeight, 8, 3, 4);
    server->kbdAddEvent = HandleKBD;
    server->frameBuffer = framebuffer.data();
    if (!server->frameBuffer)
    {
        std::cerr << "Cannot read buffer!\n";
        exit(1);
    }
    server->desktopName = "ImGui Remote";
    server->alwaysShared = TRUE;
    server->serverFormat.trueColour = TRUE;
    server->serverFormat.bitsPerPixel = 32;
    server->serverFormat.depth = 24;
    server->serverFormat.redMax = 255;
    server->serverFormat.greenMax = 255;
    server->serverFormat.blueMax = 255;
    server->serverFormat.redShift = 0;
    server->serverFormat.greenShift = 8;
    server->serverFormat.blueShift = 16;

    rfbInitServer(server);
    std::cout << "VNC Server started\n";
    //rfbRunEventLoop(server, 40000, TRUE);
}

void SendFramebufferToVNC(rfbScreenInfoPtr server)
{
    std::vector<char> flipped(_windowWidth * _windowHeight * 4);

    // Copy framebuffer data
    for (int y = 0; y < _windowHeight; y++)
    {
        std::memcpy(
            flipped.data() + (_windowHeight - 1 - y) * _windowWidth * 4,
            framebuffer.data() + y * _windowWidth * 4,
            _windowWidth * 4
        );
    }
    std::memcpy(server->frameBuffer, flipped.data(), flipped.size());
    //std::memcpy(server->frameBuffer, pixels, _windowWidth * _windowHeight * 4);

    rfbMarkRectAsModified(server, 0, 0, _windowWidth, _windowHeight);
}

void HandleVNCInput(rfbClientPtr cl, int x, int y, int buttonMask)
{
    if (buttonMask & 1) { ImGui::GetIO().MouseDown[0] = true; }
    if (buttonMask & 2) { ImGui::GetIO().MouseDown[1] = true; }
    if (buttonMask & 4) { ImGui::GetIO().MouseDown[2] = true; }

    ImGui::GetIO().MousePos = ImVec2(x, y);
}

#pragma endregion

#pragma region ImGui Init and Terminate

void InitImGui(GLFWwindow* window)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");
}

void TerminateImGui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

#pragma endregion

void CaptureFrame(std::vector<char>& pixels)
{
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, _windowWidth, _windowHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}


int main(void)
{

    GLFWwindow* window;

    // Initialize the library 
    if (!glfwInit())
        return -1;

    
    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(_windowWidth, _windowHeight, "VNC Server", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    if (!gladLoadGL())
    {
		std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

	InitImGui(window);
    float slider = 0.5f;

	//InitVNCServer(_server);
    
    // VNC SERVER
    rfbScreenInfoPtr _screen = rfbGetScreen(nullptr, nullptr, _windowWidth, _windowHeight, 8, 3, BYTE_PER_PIXEL);
    size_t fbSize = _windowWidth * _windowHeight * BYTE_PER_PIXEL;
    _screen->frameBuffer = (char*)malloc(fbSize);
    
    std::memset(_screen->frameBuffer, 0, fbSize);
    _screen->desktopName = strdup("VNC Server with ImGUI");
    _screen->serverFormat.redShift = 16;
    _screen->serverFormat.greenShift = 8;
    _screen->serverFormat.blueShift = 0;
    _screen->port = PORT;
    rfbInitServer(_screen);


    
    Loop until the user closes the window
    while (!glfwWindowShouldClose(window))
    {
        // Render here 
        glClear(GL_COLOR_BUFFER_BIT);

        // Start a new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::SliderFloat("Test Slider", &slider, 0.0f, 3.0f);
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		//CaptureFrame(framebuffer);
		//SendFramebufferToVNC(_server);

        // Process VNC events

        // Swap front and back buffers 
        glfwSwapBuffers(window);

        // Poll for and process events 
        glfwPollEvents();

        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, _windowWidth, _windowHeight, GL_RGBA, GL_UNSIGNED_BYTE, _screen->frameBuffer);

        rfbMarkRectAsModified(_screen, 0, 0, _windowWidth, _windowHeight);


        rfbProcessEvents(_screen, 10000);
    }

	//rfbShutdownServer(_server, TRUE);
    rfbScreenCleanup(_screen);
    //delete[] _screen->frameBuffer;
    
	TerminateImGui();

    glfwTerminate();


    return 0;
}

*/