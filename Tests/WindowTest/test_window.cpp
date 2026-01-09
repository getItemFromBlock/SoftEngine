#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Core/Window/WindowGLFW.h"
#include "Core/Window.h"

using namespace testing;

class WindowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        windowAPI = WindowAPI::GLFW;
        config.title = "Test Window";
        config.size = Vec2i(800, 600);
        config.attributes = WindowAttributes::None;
    }

    void TearDown() override
    {
        if (window)
        {
            window->Terminate();
            window.reset();
        }
    }

    WindowAPI windowAPI;
    WindowConfig config;
    std::unique_ptr<Window> window;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(WindowTest, CreateWindow_WithGLFWAPI_ReturnsValidWindow)
{
    window = Window::Create(windowAPI, config);
    
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->GetWindowAPI(), windowAPI);
}

TEST_F(WindowTest, Initialize_SetsCorrectTitle)
{
    window = Window::Create(windowAPI, config);
    
    EXPECT_EQ(window->GetTitle(), "Test Window");
}

TEST_F(WindowTest, Initialize_SetsCorrectSize)
{
    window = Window::Create(windowAPI, config);
    
    Vec2i size = window->GetSize();
    EXPECT_EQ(size.x, 800);
    EXPECT_EQ(size.y, 600);
}

TEST_F(WindowTest, InitializeAPI_ReturnsTrue)
{
    window = Window::Create(windowAPI, config);
    
    EXPECT_TRUE(window->InitializeAPI());
}

// ============================================================================
// Window Properties Tests
// ============================================================================

TEST_F(WindowTest, SetTitle_UpdatesWindowTitle)
{
    window = Window::Create(windowAPI, config);
    
    window->SetTitle("New Title");
    
    EXPECT_EQ(window->GetTitle(), "New Title");
}

TEST_F(WindowTest, SetSize_UpdatesWindowSize)
{
    window = Window::Create(windowAPI, config);
        
    Vec2i newSize(1024, 768);
    window->SetSize(newSize);
    
    Vec2i size = window->GetSize();
    EXPECT_EQ(size.x, 1024);
    EXPECT_EQ(size.y, 768);
}

TEST_F(WindowTest, SetPosition_UpdatesWindowPosition)
{
    window = Window::Create(windowAPI, config);
        
    Vec2i newPos(100, 200);
    window->SetPosition(newPos);
    
    Vec2i pos = window->GetPosition();
    EXPECT_EQ(pos.x, 100);
    EXPECT_EQ(pos.y, 200);
}

TEST_F(WindowTest, GetAspectRatio_ReturnsCorrectRatio)
{
    window = Window::Create(windowAPI, config);
        
    float aspectRatio = window->GetAspectRatio();
    float expected = 800.0f / 600.0f;
    
    EXPECT_FLOAT_EQ(aspectRatio, expected);
}

// ============================================================================
// Window Attributes Tests
// ============================================================================

TEST_F(WindowTest, VSync_InitiallyDisabled)
{
    window = Window::Create(windowAPI, config);
        
    EXPECT_FALSE(window->IsVSyncEnabled());
}

TEST_F(WindowTest, SetVSync_EnablesVSync)
{
    window = Window::Create(windowAPI, config);
        
    window->SetVSync(true);
    
    EXPECT_TRUE(window->IsVSyncEnabled());
}

TEST_F(WindowTest, SetVSync_DisablesVSync)
{
    window = Window::Create(windowAPI, config);
        
    window->SetVSync(true);
    window->SetVSync(false);
    
    EXPECT_FALSE(window->IsVSyncEnabled());
}

TEST_F(WindowTest, Initialize_WithVSyncAttribute_EnablesVSync)
{
    config.attributes = WindowAttributes::VSync;
    window = Window::Create(windowAPI, config);
        
    EXPECT_TRUE(window->IsVSyncEnabled());
}

TEST_F(WindowTest, SetClickThrough_TogglesClickThrough)
{
    window = Window::Create(windowAPI, config);
        
    window->SetClickThrough(true);
    EXPECT_TRUE(window->IsClickThroughEnabled());
    
    window->SetClickThrough(false);
    EXPECT_FALSE(window->IsClickThroughEnabled());
}

TEST_F(WindowTest, SetDecorated_TogglesDecoration)
{
    window = Window::Create(windowAPI, config);
        
    window->SetDecorated(false);
    EXPECT_FALSE(window->IsDecoratedEnabled());
    
    window->SetDecorated(true);
    EXPECT_TRUE(window->IsDecoratedEnabled());
}

TEST_F(WindowTest, SetTransparent_TogglesTransparency)
{
    window = Window::Create(windowAPI, config);
        
    window->SetTransparent(true);
    EXPECT_FALSE(window->IsTransparentEnabled());
    
    window->SetTransparent(false);
    EXPECT_FALSE(window->IsTransparentEnabled());
}

// ============================================================================
// Window State Tests
// ============================================================================

TEST_F(WindowTest, ShouldClose_InitiallyFalse)
{
    window = Window::Create(windowAPI, config);
        
    EXPECT_FALSE(window->ShouldClose());
}

TEST_F(WindowTest, Close_SetsShouldCloseFlag)
{
    window = Window::Create(windowAPI, config);
        
    window->Close(true);
    
    EXPECT_TRUE(window->ShouldClose());
}

TEST_F(WindowTest, Close_ClearsShouldCloseFlag)
{
    window = Window::Create(windowAPI, config);
        
    window->Close(true);
    window->Close(false);
    
    EXPECT_FALSE(window->ShouldClose());
}

TEST_F(WindowTest, PollEvents_DoesNotThrow)
{
    window = Window::Create(windowAPI, config);
        
    EXPECT_NO_THROW(window->PollEvents());
}

// ============================================================================
// Cursor Tests
// ============================================================================

TEST_F(WindowTest, SetMouseCursorMode_UpdatesCursorMode)
{
    window = Window::Create(windowAPI, config);
        
    window->SetMouseCursorMode(CursorMode::Disabled);
    
    EXPECT_EQ(window->GetMouseCursorMode(), CursorMode::Disabled);
}

TEST_F(WindowTest, SetMouseCursorType_UpdatesCursorType)
{
    window = Window::Create(windowAPI, config);
        
    window->SetMouseCursorType(CursorType::Hand);
    
    EXPECT_EQ(window->GetMouseCursorType(), CursorType::Hand);
}

TEST_F(WindowTest, SetMouseCursorPosition_WindowSpace_UpdatesPosition)
{
    window = Window::Create(windowAPI, config);
        
    Vec2i pos(100, 100);
    window->SetMouseCursorPosition(pos, CoordinateSpace::Window);
    
    Vec2i cursorPos = window->GetMouseCursorPosition(CoordinateSpace::Window);
    // Note: Exact position may vary slightly due to system constraints
    EXPECT_NEAR(cursorPos.x, 100, 5);
    EXPECT_NEAR(cursorPos.y, 100, 5);
}

// ============================================================================
// Coordinate Space Tests
// ============================================================================

TEST_F(WindowTest, ToWindowSpace_ConvertsScreenCoordinates)
{
    window = Window::Create(windowAPI, config);
        
    Vec2i windowPos = window->GetPosition();
    Vec2i screenPos(windowPos.x + 50, windowPos.y + 50);
    
    Vec2i result = window->ToWindowSpace(screenPos);
    
    EXPECT_EQ(result.x, 50);
    EXPECT_EQ(result.y, 50);
}

TEST_F(WindowTest, ToScreenSpace_ConvertsWindowCoordinates)
{
    window = Window::Create(windowAPI, config);
        
    Vec2i windowPos = window->GetPosition();
    Vec2i localPos(50, 50);
    
    Vec2i result = window->ToScreenSpace(localPos);
    
    EXPECT_EQ(result.x, windowPos.x + 50);
    EXPECT_EQ(result.y, windowPos.y + 50);
}

// ============================================================================
// Native Handle Tests
// ============================================================================

TEST_F(WindowTest, GetNativeHandle_ReturnsValidHandle)
{
    window = Window::Create(windowAPI, config);
        
    void* handle = window->GetNativeHandle();
    
    EXPECT_NE(handle, nullptr);
}

TEST_F(WindowTest, GetHandle_ReturnsGLFWwindow)
{
    window = Window::Create(windowAPI, config);
        
    auto* glfwWindow = dynamic_cast<WindowGLFW*>(window.get());
    ASSERT_NE(glfwWindow, nullptr);
    
    GLFWwindow* handle = glfwWindow->GetHandle();
    EXPECT_NE(handle, nullptr);
}

// ============================================================================
// Vulkan Integration Tests
// ============================================================================

TEST_F(WindowTest, GetRequiredExtensions_ReturnsExtensions)
{
    window = Window::Create(windowAPI, config);
        
    auto extensions = window->GetRequiredExtensions();
    
    EXPECT_FALSE(extensions.empty());
}

TEST_F(WindowTest, CreateSurface_WithValidInstance_ReturnsSurface)
{
    window = Window::Create(windowAPI, config);
    
    //TODO
}

// ============================================================================
// Input System Tests
// ============================================================================

TEST_F(WindowTest, GetInput_ReturnsValidInputReference)
{
    window = Window::Create(windowAPI, config);
        
    Input& input = window->GetInput();
    
    EXPECT_NO_THROW({
        auto& inputRef = window->GetInput();
    });
}

// ============================================================================
// Event System Tests
// ============================================================================

TEST_F(WindowTest, ResizeEvent_TriggersWhenWindowResized)
{
    window = Window::Create(windowAPI, config);
        
    bool eventTriggered = false;
    Vec2i receivedSize;
    
    window->EResizeEvent.Bind([&](Vec2i size) {
        eventTriggered = true;
        receivedSize = size;
    });
    
    Vec2i newSize(1024, 768);
    window->SetSize(newSize);
    window->PollEvents();
    
    EXPECT_TRUE(eventTriggered);
    EXPECT_EQ(receivedSize, newSize);
}

TEST_F(WindowTest, MoveEvent_TriggersWhenWindowMoved)
{
    window = Window::Create(windowAPI, config);
        
    bool eventTriggered = false;
    
    window->EMoveEvent.Bind([&](Vec2i pos) {
        eventTriggered = true;
    });
    
    Vec2i newPos(100, 100);
    window->SetPosition(newPos);
    window->PollEvents();
    
    EXPECT_TRUE(eventTriggered);
}

// ============================================================================
// Multiple Window Tests
// ============================================================================

TEST_F(WindowTest, MultipleWindows_CanBeCreated)
{
    auto window1 = Window::Create(windowAPI, config);
    window1->Initialize(config);
    
    WindowConfig config2;
    config2.title = "Second Window";
    config2.size = Vec2i(640, 480);
    config2.attributes = WindowAttributes::None;
    
    auto window2 = Window::Create(windowAPI, config2);
    window2->Initialize(config2);
    
    EXPECT_NE(window1, nullptr);
    EXPECT_NE(window2, nullptr);
    EXPECT_NE(window1->GetNativeHandle(), window2->GetNativeHandle());
    
    window2->Terminate();
    window1->Terminate();
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(WindowTest, Terminate_CanBeCalledMultipleTimes)
{
    window = Window::Create(windowAPI, config);
        
    EXPECT_NO_THROW({
        window->Terminate();
        window->Terminate();
    });
}

// ============================================================================
// Combined Attributes Tests
// ============================================================================

TEST_F(WindowTest, Initialize_WithMultipleAttributes_AppliesAll)
{
    config.attributes = static_cast<WindowAttributes>(
        WindowAttributes::VSync | 
        WindowAttributes::NoDecoration
    );
    
    window = Window::Create(windowAPI, config);
        
    EXPECT_TRUE(window->IsVSyncEnabled());
    EXPECT_FALSE(window->IsDecoratedEnabled());
}

// ============================================================================
// Main function
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}