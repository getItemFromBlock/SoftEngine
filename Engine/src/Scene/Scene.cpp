#include "Scene.h"

#include <ranges>

#include "GameObject.h"
#include "Component/IComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Engine.h"
#include "Debug/Log.h"

Scene::Scene()
{
    SafePtr<GameObject> root = CreateGameObject();
    root->SetName("Root");
    m_rootUUID = root->GetUUID();

    m_editorCamera = std::make_unique<Camera>();
    m_editorCamera->GetTransform()->SetLocalPosition(Vec3f::Zero());

    m_editorCamera->GetTransform()->EOnUpdateModelMatrix += [this]()
    {
        float aspect = Engine::Get()->GetWindow()->GetAspectRatio();
        
        m_editorCamera->SetAspectRatio(aspect);
        
        m_editorCamera->UpdateFrustum();

        m_editorCameraData.frustum = m_editorCamera->GetFrustum();
        m_editorCameraData.VP = m_editorCamera->GetViewProjectionMatrix();
        m_editorCameraData.forward = m_editorCamera->GetTransform()->GetForward();
        m_editorCameraData.right = m_editorCamera->GetTransform()->GetRight();
        m_editorCameraData.up = m_editorCamera->GetTransform()->GetUp();
    };
}

Scene::~Scene()
{
    GameObject* root = GetRootObject().getPtr();
    DestroyGameObject(root);
}

void Scene::OnRender(RHIRenderer* renderer)
{
    std::scoped_lock lock(m_componentsMutex);
    
    for (const std::vector<std::shared_ptr<IComponent>>& componentList : m_components | std::views::values)
    {
        for (const std::shared_ptr<IComponent>& component : componentList)
        {
            if (component->IsEnable())
                component->OnRender(renderer);
        }
    }
    
    auto renderQueueManager = renderer->GetRenderQueueManager();
    renderQueueManager->SortAll();
    renderQueueManager->ExecuteAll(renderer);
    renderQueueManager->ClearAll();
}

void Scene::OnUpdate(float deltaTime)
{
    UpdateCamera(deltaTime);

    std::scoped_lock lock(m_componentsMutex);
    
    for (const std::vector<std::shared_ptr<IComponent>>& componentList : m_components | std::views::values)
    {
        for (const std::shared_ptr<IComponent>& component : componentList)
        {
            if (component->IsEnable())
                component->OnUpdate(deltaTime);
        }
    }
}

SafePtr<GameObject> Scene::CreateGameObject(GameObject* parent)
{
    std::shared_ptr object = std::make_shared<GameObject>(*this);
    object->SetName("GameObject");
    
    {
        std::scoped_lock lock(m_gameObjectsMutex);
        m_gameObjects.emplace(object->GetUUID(), object);
    }
    
    SetParent(object.get(), parent ? parent : (m_rootUUID != UUID_INVALID ? GetRootObject().getPtr() : nullptr));
    
    return object;
}

SafePtr<GameObject> Scene::GetGameObject(Core::UUID uuid) const
{
    std::scoped_lock lock(m_gameObjectsMutex);
    
    auto it = m_gameObjects.find(uuid);
    if (it != m_gameObjects.end())
        return it->second;
    return {};
}

SafePtr<GameObject> Scene::GetRootObject() const
{
    if (m_rootUUID != UUID_INVALID)
        return GetGameObject(m_rootUUID);
    return {};
}

void Scene::SetParent(GameObject* object, GameObject* parent)
{
    Core::UUID objectUuid = object->GetUUID();
    if (object->m_parentUUID != UUID_INVALID)
    {
        SafePtr<GameObject> prevParent = GetGameObject(object->m_parentUUID);
        if (prevParent)
            RemoveChild(prevParent.getPtr(), object);
    }

    if (parent)
        object->m_parentUUID = parent->GetUUID();
    else
        object->m_parentUUID = UUID_INVALID;

    if (parent)
    {
        parent->m_childrenUUID.insert(objectUuid);
    }
}

void Scene::RemoveChild(GameObject* object, GameObject* child)
{
    auto it = object->m_childrenUUID.find(child->GetUUID());
    ASSERT(it != object->m_childrenUUID.end())
    child->m_parentUUID = UUID_INVALID;
    object->m_childrenUUID.erase(it);
}

std::vector<SafePtr<IComponent>> Scene::GetComponents(const GameObject* gameObject)
{
    std::vector<SafePtr<IComponent>> result;

    if (!gameObject)
        return result;

    std::scoped_lock lock(m_componentsMutex);

    for (auto& componentList : m_components | std::views::values)
    {
        for (auto& component : componentList)
        {
            if (component->GetGameObject() == gameObject)
            {
                result.emplace_back(component);
            }
        }
    }

    return result;
}

void Scene::DestroyGameObject(GameObject* gameObject)
{
    std::scoped_lock lock(m_gameObjectsMutex);
    
    auto it = m_gameObjects.find(gameObject->GetUUID());
    if (it == m_gameObjects.end())
        return;
    
    for (auto& childUUID : gameObject->m_childrenUUID)
    {
        GameObject* object = GetGameObject(childUUID).getPtr();
        DestroyGameObject(object);
    }

    RemoveAllComponents(gameObject);

    m_gameObjects.erase(it);
}

void Scene::RemoveComponent(Core::UUID compId)
{
    for (auto& componentList : m_components | std::views::values)
    {
        auto removeIt = std::ranges::remove_if(componentList,
           [compId](const std::shared_ptr<IComponent>& component)
           {
               if (component->GetUUID() == compId)
               {
                   component->OnDestroy();
                   return true;
               }
               return false;
           }
        ).begin();
    }
}

void Scene::RemoveAllComponents(GameObject* gameObject)
{
    if (!gameObject)
        return;

    std::scoped_lock lock(m_componentsMutex);
    
    for (auto& componentList : m_components | std::views::values)
    {
        auto removeIt = std::ranges::remove_if(componentList,
           [gameObject](const std::shared_ptr<IComponent>& component)
           {
               if (component->GetGameObject() == gameObject)
               {
                   component->OnDestroy();
                   return true;
               }
               return false;
           }
        ).begin();

        componentList.erase(removeIt, componentList.end());
    }
}

void Scene::UpdateCamera(float deltaTime) const
{
    static Vec2f startClickPos;
    static Vec2f prevMousePos = Vec2f::Zero();
    auto transform = m_editorCamera->GetTransform();
    transform->OnUpdate(deltaTime);
    
    auto position = transform->GetLocalPosition();
    Window* window = Engine::Get()->GetWindow();
    Input& input = window->GetInput();
    
    static bool isLooking = false;
    
    auto stopLooking = [&]()
    {
        isLooking = false;
        prevMousePos = window->GetMouseCursorPosition();
        window->SetMouseCursorMode(CursorMode::Normal);
        window->SetMouseCursorPosition(startClickPos);
    };
    
    if (isLooking && input.IsMouseButtonReleased(MouseButton::BUTTON_2))
    {
        stopLooking();
    }
    
    if (input.IsMouseButtonPressed(MouseButton::BUTTON_2))
    {
        isLooking = true;
        window->SetMouseCursorMode(CursorMode::Hidden);
        startClickPos = window->GetMouseCursorPosition();
        prevMousePos = startClickPos;
    }
    
    if (!isLooking)
        return;

    constexpr float speed = 10.f;
    constexpr float freeLookSensitivity = 0.5f;
    if (input.IsKeyDown(Key::W))
    {
        position += transform->GetForward() * speed * deltaTime;
    }
    if (input.IsKeyDown(Key::S))
    {
        position -= transform->GetForward() * speed * deltaTime;
    }
    if (input.IsKeyDown(Key::A))
    {
        position -= transform->GetRight() * speed * deltaTime;
    }
    if (input.IsKeyDown(Key::D))
    {
        position += transform->GetRight() * speed * deltaTime;
    }
    if (input.IsKeyDown(Key::Q))
    {
        position += transform->GetUp() * speed * deltaTime;
    }
    if (input.IsKeyDown(Key::E))
    {
        position -= transform->GetUp() * speed * deltaTime;
    }

    transform->SetLocalPosition(position);
    
    const Vec2f mousePos = window->GetMouseCursorPosition();
    const Vec2f delta = mousePos - prevMousePos;

    float mouseX = delta.x * freeLookSensitivity;
    float mouseY = delta.y * freeLookSensitivity;

    prevMousePos = mousePos;

    if (transform->GetUp().y < 0)
    {
        mouseX *= -1;
    }

    transform->Rotate(Vec3f::Up(), -mouseX, Space::World);
    transform->Rotate(Vec3f::Right(), -mouseY, Space::Local);
}