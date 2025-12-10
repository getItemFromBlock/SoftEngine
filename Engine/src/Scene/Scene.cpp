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

    m_cameraData.transform = std::make_unique<TransformComponent>();
    m_cameraData.transform->SetLocalPosition(Vec3f(2.0f, 2.0f, 2.0f));

    m_cameraData.transform->EOnUpdateModelMatrix += [this]()
    {
        float aspect = Engine::Get()->GetWindow()->GetAspectRatio();
        Mat4 view = Mat4::LookAtRH(m_cameraData.transform->GetLocalPosition(),
                                   m_cameraData.transform->GetLocalPosition() + m_cameraData.transform->GetForward(), 
                                   m_cameraData.transform->GetUp());
        Mat4 projection = Mat4::CreateProjectionMatrix(45.f, aspect, 0.1f, 10.0f); 
        projection[1][1] *= -1;

        m_cameraData.VP = projection * view;
    };
}

Scene::~Scene() = default;

void Scene::OnRender(RHIRenderer* renderer)
{
    for (const std::vector<std::shared_ptr<IComponent>>& componentList : m_components | std::views::values)
    {
        for (const std::shared_ptr<IComponent>& component : componentList)
        {
            component->OnRender(renderer);
        }
    }
}

void Scene::OnUpdate(float deltaTime)
{
    UpdateCamera(deltaTime);
    for (const std::vector<std::shared_ptr<IComponent>>& componentList : m_components | std::views::values)
    {
        for (const std::shared_ptr<IComponent>& component : componentList)
        {
            component->OnUpdate(deltaTime);
        }
    }
}

SafePtr<GameObject> Scene::CreateGameObject(GameObject* parent)
{
    std::shared_ptr object = std::make_shared<GameObject>(*this);
    m_gameObjects.emplace(object->GetUUID(), object);

    SetParent(object.get(), parent ? parent : (m_rootUUID != UUID_INVALID ? GetRootObject().get().get() : nullptr));

    object->AddComponent<TransformComponent>();
    return object;
}

SafePtr<GameObject> Scene::GetGameObject(Core::UUID uuid) const
{
    return m_gameObjects.at(uuid);
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
        RemoveChild(prevParent.get().get(), object);
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

void Scene::DestroyGameObject(GameObject* gameObject)
{
    auto it = m_gameObjects.find(gameObject->GetUUID());
    if (it == m_gameObjects.end())
        return;

    RemoveAllComponents(gameObject);

    m_gameObjects.erase(it);
}

void Scene::RemoveAllComponents(GameObject* gameObject)
{
    if (!gameObject)
        return;

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
    auto transform = m_cameraData.transform.get();
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
    
    const float speed = 10.f;
    const float freeLookSensitivity = 0.5f;
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

    if (m_cameraData.transform->GetUp().y < 0)
    {
        mouseX *= -1;
    }

    m_cameraData.transform->Rotate(Vec3f::Up(), -mouseX, Space::World);
    m_cameraData.transform->Rotate(Vec3f::Right(), -mouseY, Space::Local);

}
