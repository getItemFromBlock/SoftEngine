#include "Scene.h"

#include <ranges>

#include "GameObject.h"
#include "Component/IComponent.h"
#include "Component/TransformComponent.h"
#include "Debug/Log.h"


Scene::Scene()
{
    SafePtr<GameObject> root = CreateGameObject();
    root->SetName("Root");
    m_rootUUID = root->GetUUID();
}

Scene::~Scene()
{
}

void Scene::OnRender()
{
    for (const std::vector<std::shared_ptr<IComponent>>& componentList : m_components | std::views::values)
    {
        for (const std::shared_ptr<IComponent>& component : componentList)
        {
            component->OnRender();
        }
    }
}

void Scene::OnUpdate(float deltaTime) const
{
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

SafePtr<GameObject> Scene::GetGameObject(UUID uuid) const
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
    UUID objectUuid = object->GetUUID();
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
