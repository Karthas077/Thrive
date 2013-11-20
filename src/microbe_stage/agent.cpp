#include "microbe_stage/agent.h"

#include "bullet/rigid_body_system.h"
#include "engine/component_factory.h"
#include "engine/engine.h"
#include "engine/entity_filter.h"
#include "engine/game_state.h"
#include "engine/serialization.h"
#include "engine/rng.h"
#include "game.h"
#include "ogre/scene_node_system.h"
#include "scripting/luabind.h"

#include <OgreEntity.h>
#include <OgreSceneManager.h>

using namespace thrive;

REGISTER_COMPONENT(AgentComponent)


luabind::scope
AgentComponent::luaBindings() {
    using namespace luabind;
    return class_<AgentComponent, Component>("AgentComponent")
        .enum_("ID") [
            value("TYPE_ID", AgentComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &AgentComponent::TYPE_NAME)
        ]
        .def(constructor<>())
        .def_readwrite("agentId", &AgentComponent::m_agentId)
        .def_readwrite("potency", &AgentComponent::m_potency)
        .def_readwrite("timeToLive", &AgentComponent::m_timeToLive)
        .def_readwrite("velocity", &AgentComponent::m_velocity)
    ;
}


void
AgentComponent::load(
    const StorageContainer& storage
) {
    Component::load(storage);
    m_agentId = storage.get<AgentId>("agentId", NULL_AGENT);
    m_potency = storage.get<float>("potency");
    m_timeToLive = storage.get<Milliseconds>("timeToLive");
    m_velocity = storage.get<Ogre::Vector3>("velocity");
}


StorageContainer
AgentComponent::storage() const {
    StorageContainer storage = Component::storage();
    storage.set<AgentId>("agentId", m_agentId);
    storage.set<float>("potency", m_potency);
    storage.set<Milliseconds>("timeToLive", m_timeToLive);
    storage.set<Ogre::Vector3>("velocity", m_velocity);
    return storage;
}

////////////////////////////////////////////////////////////////////////////////
// AgentEmitterComponent
////////////////////////////////////////////////////////////////////////////////

//Ignore C-style casts warnings to allow luabind construct with overloads.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
luabind::scope
AgentEmitterComponent::luaBindings() {
    using namespace luabind;
    return class_<AgentEmitterComponent, Component>("AgentEmitterComponent")
        .enum_("ID") [
            value("TYPE_ID", AgentEmitterComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &AgentEmitterComponent::TYPE_NAME)
        ]
        .def(constructor<>())
        .def("emitAgent", ( void(AgentEmitterComponent::*)(AgentId, int, bool, Ogre::Vector3) ) &AgentEmitterComponent::emitAgent) //Cast to desired overload
        .def_readwrite("agentId", &AgentEmitterComponent::m_agentId)
        .def_readwrite("emissionRadius", &AgentEmitterComponent::m_emissionRadius)
        .def_readwrite("maxInitialSpeed", &AgentEmitterComponent::m_maxInitialSpeed)
        .def_readwrite("minInitialSpeed", &AgentEmitterComponent::m_minInitialSpeed)
        .def_readwrite("minEmissionAngle", &AgentEmitterComponent::m_minEmissionAngle)
        .def_readwrite("maxEmissionAngle", &AgentEmitterComponent::m_maxEmissionAngle)
        .def_readwrite("meshName", &AgentEmitterComponent::m_meshName)
        .def_readwrite("particlesPerEmission", &AgentEmitterComponent::m_particlesPerEmission)
        .def_readwrite("particleLifetime", &AgentEmitterComponent::m_particleLifetime)
        .def_readwrite("particleScale", &AgentEmitterComponent::m_particleScale)
        .def_readwrite("potencyPerParticle", &AgentEmitterComponent::m_potencyPerParticle)
    ;
}
#pragma GCC diagnostic pop


void
AgentEmitterComponent::emitAgent(
    Ogre::Vector3 emittorPosition
) {
    this->emitAgent(this->m_agentId, this->m_potencyPerParticle, false, emittorPosition);
}


void
AgentEmitterComponent::emitAgent(
    AgentId agentId,
    int amount,
    bool useAbsolutePosition,      // If true, the emissionPosition parameter will an absolute position for the emission
    Ogre::Vector3 emissionPosition
) {
    Ogre::Vector3 emissionOffset(0,0,0);

    Ogre::Degree emissionAngle{static_cast<Ogre::Real>(Game::instance().engine().rng().getDouble(
        this->m_minEmissionAngle.valueDegrees(),
        this->m_maxEmissionAngle.valueDegrees()
    ))};
    Ogre::Real emissionSpeed = Game::instance().engine().rng().getDouble(
        this->m_minInitialSpeed,
        this->m_maxInitialSpeed
    );
    Ogre::Vector3 emissionVelocity(
        emissionSpeed * Ogre::Math::Sin(emissionAngle),
        emissionSpeed * Ogre::Math::Cos(emissionAngle),
        0.0
    );
    if (!useAbsolutePosition)
    {
        emissionOffset = Ogre::Vector3(
            this->m_emissionRadius * Ogre::Math::Sin(emissionAngle),
            this->m_emissionRadius * Ogre::Math::Cos(emissionAngle),
            0.0
        );
    }
    EntityId agentEntityId = Game::instance().engine().currentGameState()->entityManager().generateNewId();
    // Scene Node
    auto agentSceneNodeComponent = make_unique<OgreSceneNodeComponent>();
    agentSceneNodeComponent->m_transform.scale = this->m_particleScale;
    agentSceneNodeComponent->m_meshName = this->m_meshName;
    // Collision Hull
    auto agentRigidBodyComponent = make_unique<RigidBodyComponent>(
        btBroadphaseProxy::SensorTrigger,
        btBroadphaseProxy::AllFilter & (~ btBroadphaseProxy::SensorTrigger)
    );
    agentRigidBodyComponent->m_properties.shape = std::make_shared<SphereShape>(0.01);
    agentRigidBodyComponent->m_properties.hasContactResponse = false;
    agentRigidBodyComponent->m_properties.kinematic = true;
    agentRigidBodyComponent->m_dynamicProperties.position = emissionPosition + emissionOffset;
    // Agent Component
    auto agentComponent = make_unique<AgentComponent>();
    agentComponent->m_timeToLive = this->m_particleLifetime;
    agentComponent->m_velocity = emissionVelocity;
    agentComponent->m_agentId = agentId;
    agentComponent->m_potency = amount;
    // Build component list
    std::list<std::unique_ptr<Component>> components;
    components.emplace_back(std::move(agentSceneNodeComponent));
    components.emplace_back(std::move(agentComponent));
    components.emplace_back(std::move(agentRigidBodyComponent));
    for (auto& component : components) {
        Game::instance().engine().currentGameState()->entityManager().addComponent(
            agentEntityId,
            std::move(component)
        );
    }
}



void
AgentEmitterComponent::load(
    const StorageContainer& storage
) {
    Component::load(storage);
    m_agentId = storage.get<AgentId>("agentId", NULL_AGENT);
    m_emissionRadius = storage.get<Ogre::Real>("emissionRadius", 0.0);
    m_maxInitialSpeed = storage.get<Ogre::Real>("maxInitialSpeed", 0.0);
    m_minInitialSpeed = storage.get<Ogre::Real>("minInitialSpeed", 0.0);
    m_maxEmissionAngle = storage.get<Ogre::Degree>("maxEmissionAngle");
    m_minEmissionAngle = storage.get<Ogre::Degree>("minEmissionAngle");
    m_meshName = storage.get<Ogre::String>("meshName");
    m_particlesPerEmission = storage.get<uint16_t>("particlesPerEmission");
    m_particleLifetime = storage.get<Milliseconds>("particleLifetime");
    m_particleScale = storage.get<Ogre::Vector3>("particleScale");
    m_potencyPerParticle = storage.get<float>("potencyPerParticle");
}

StorageContainer
AgentEmitterComponent::storage() const {
    StorageContainer storage = Component::storage();
    storage.set<AgentId>("agentId", m_agentId);
    storage.set<Ogre::Real>("emissionRadius", m_emissionRadius);
    storage.set<Ogre::Real>("maxInitialSpeed", m_maxInitialSpeed);
    storage.set<Ogre::Real>("minInitialSpeed", m_minInitialSpeed);
    storage.set<Ogre::Degree>("maxEmissionAngle", m_maxEmissionAngle);
    storage.set<Ogre::Degree>("minEmissionAngle", m_minEmissionAngle);
    storage.set<Ogre::String>("meshName", m_meshName);
    storage.set<uint16_t>("particlesPerEmission", m_particlesPerEmission);
    storage.set<Milliseconds>("particleLifetime", m_particleLifetime);
    storage.set<Ogre::Vector3>("particleScale", m_particleScale);
    storage.set<float>("potencyPerParticle", m_potencyPerParticle);
    return storage;
}

REGISTER_COMPONENT(AgentEmitterComponent)


////////////////////////////////////////////////////////////////////////////////
// TimedAgentEmitterComponent
////////////////////////////////////////////////////////////////////////////////

luabind::scope
TimedAgentEmitterComponent::luaBindings() {
    using namespace luabind;
    return class_<TimedAgentEmitterComponent, Component>("TimedAgentEmitterComponent")
        .enum_("ID") [
            value("TYPE_ID", TimedAgentEmitterComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &TimedAgentEmitterComponent::TYPE_NAME)
        ]
        .def(constructor<>())
        .def_readwrite("emitInterval", &TimedAgentEmitterComponent::m_emitInterval)
    ;
}


void
TimedAgentEmitterComponent::load(
    const StorageContainer& storage
) {
    Component::load(storage);
    m_emitInterval = storage.get<Milliseconds>("emitInterval", 1000);
    m_timeSinceLastEmission = storage.get<Milliseconds>("timeSinceLastEmission");
}


StorageContainer
TimedAgentEmitterComponent::storage() const {
    StorageContainer storage = Component::storage();
    storage.set<Milliseconds>("emitInterval", m_emitInterval);
    storage.set<Milliseconds>("timeSinceLastEmission", m_timeSinceLastEmission);
    return storage;
}

REGISTER_COMPONENT(TimedAgentEmitterComponent)

////////////////////////////////////////////////////////////////////////////////
// AgentAbsorberComponent
////////////////////////////////////////////////////////////////////////////////

luabind::scope
AgentAbsorberComponent::luaBindings() {
    using namespace luabind;
    return class_<AgentAbsorberComponent, Component>("AgentAbsorberComponent")
        .enum_("ID") [
            value("TYPE_ID", AgentAbsorberComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &AgentAbsorberComponent::TYPE_NAME)
        ]
        .def(constructor<>())
        .def("absorbedAgentAmount", &AgentAbsorberComponent::absorbedAgentAmount)
        .def("setAbsorbedAgentAmount", &AgentAbsorberComponent::setAbsorbedAgentAmount)
        .def("setCanAbsorbAgent", &AgentAbsorberComponent::setCanAbsorbAgent)
    ;
}


float
AgentAbsorberComponent::absorbedAgentAmount(
    AgentId id
) const {
    const auto& iter = m_absorbedAgents.find(id);
    if (iter != m_absorbedAgents.cend()) {
        return iter->second;
    }
    else {
        return 0.0f;
    }
}


bool
AgentAbsorberComponent::canAbsorbAgent(
    AgentId id
) const {
    return m_canAbsorbAgent.find(id) != m_canAbsorbAgent.end();
}


void
AgentAbsorberComponent::load(
    const StorageContainer& storage
) {
    Component::load(storage);
    StorageList agents = storage.get<StorageList>("agents");
    for (const StorageContainer& container : agents) {
        AgentId agentId = container.get<AgentId>("agentId");
        float amount = container.get<float>("amount");
        m_absorbedAgents[agentId] = amount;
        m_canAbsorbAgent.insert(agentId);
    }
}


void
AgentAbsorberComponent::setAbsorbedAgentAmount(
    AgentId id,
    float amount
) {
    m_absorbedAgents[id] = amount;
}


void
AgentAbsorberComponent::setCanAbsorbAgent(
    AgentId id,
    bool canAbsorb
) {
    if (canAbsorb) {
        m_canAbsorbAgent.insert(id);
    }
    else {
        m_canAbsorbAgent.erase(id);
    }
}


StorageContainer
AgentAbsorberComponent::storage() const {
    StorageContainer storage = Component::storage();
    StorageList agents;
    agents.reserve(m_canAbsorbAgent.size());
    for (AgentId agentId : m_canAbsorbAgent) {
        StorageContainer container;
        container.set<AgentId>("agentId", agentId);
        container.set<float>("amount", this->absorbedAgentAmount(agentId));
        agents.append(container);
    }
    storage.set<StorageList>("agents", agents);
    return storage;
}

REGISTER_COMPONENT(AgentAbsorberComponent)

////////////////////////////////////////////////////////////////////////////////
// AgentLifetimeSystem
////////////////////////////////////////////////////////////////////////////////

luabind::scope
AgentLifetimeSystem::luaBindings() {
    using namespace luabind;
    return class_<AgentLifetimeSystem, System>("AgentLifetimeSystem")
        .def(constructor<>())
    ;
}


struct AgentLifetimeSystem::Implementation {

    EntityFilter<
        AgentComponent
    > m_entities;
};


AgentLifetimeSystem::AgentLifetimeSystem()
  : m_impl(new Implementation())
{
}


AgentLifetimeSystem::~AgentLifetimeSystem() {}


void
AgentLifetimeSystem::init(
    GameState* gameState
) {
    System::init(gameState);
    m_impl->m_entities.setEntityManager(&gameState->entityManager());
}


void
AgentLifetimeSystem::shutdown() {
    m_impl->m_entities.setEntityManager(nullptr);
    System::shutdown();
}


void
AgentLifetimeSystem::update(int milliseconds) {
    for (auto& value : m_impl->m_entities) {
        AgentComponent* agentComponent = std::get<0>(value.second);
        agentComponent->m_timeToLive -= milliseconds;
        if (agentComponent->m_timeToLive <= 0) {
            this->entityManager()->removeEntity(value.first);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// AgentMovementSystem
////////////////////////////////////////////////////////////////////////////////

luabind::scope
AgentMovementSystem::luaBindings() {
    using namespace luabind;
    return class_<AgentMovementSystem, System>("AgentMovementSystem")
        .def(constructor<>())
    ;
}


struct AgentMovementSystem::Implementation {

    EntityFilter<
        AgentComponent,
        RigidBodyComponent
    > m_entities;
};


AgentMovementSystem::AgentMovementSystem()
  : m_impl(new Implementation())
{
}


AgentMovementSystem::~AgentMovementSystem() {}


void
AgentMovementSystem::init(
    GameState* gameState
) {
    System::init(gameState);
    m_impl->m_entities.setEntityManager(&gameState->entityManager());
}


void
AgentMovementSystem::shutdown() {
    m_impl->m_entities.setEntityManager(nullptr);
    System::shutdown();
}


void
AgentMovementSystem::update(int milliseconds) {
    for (auto& value : m_impl->m_entities) {
        AgentComponent* agentComponent = std::get<0>(value.second);
        RigidBodyComponent* rigidBodyComponent = std::get<1>(value.second);
        Ogre::Vector3 delta = agentComponent->m_velocity * float(milliseconds) / 1000.0f;
        rigidBodyComponent->m_dynamicProperties.position += delta;
    }
}


////////////////////////////////////////////////////////////////////////////////
// AgentEmitterSystem
////////////////////////////////////////////////////////////////////////////////

luabind::scope
AgentEmitterSystem::luaBindings() {
    using namespace luabind;
    return class_<AgentEmitterSystem, System>("AgentEmitterSystem")
        .def(constructor<>())
    ;
}


struct AgentEmitterSystem::Implementation {

    EntityFilter<
        AgentEmitterComponent,
        TimedAgentEmitterComponent,
        OgreSceneNodeComponent
    > m_entities;

    Ogre::SceneManager* m_sceneManager = nullptr;
};


AgentEmitterSystem::AgentEmitterSystem()
  : m_impl(new Implementation())
{
}


AgentEmitterSystem::~AgentEmitterSystem() {}


void
AgentEmitterSystem::init(
    GameState* gameState
) {
    System::init(gameState);
    m_impl->m_entities.setEntityManager(&gameState->entityManager());
    m_impl->m_sceneManager = gameState->sceneManager();
}


void
AgentEmitterSystem::shutdown() {
    m_impl->m_entities.setEntityManager(nullptr);
    m_impl->m_sceneManager = nullptr;
    System::shutdown();
}


void
AgentEmitterSystem::update(int milliseconds) {
    for (auto& value : m_impl->m_entities) {
        AgentEmitterComponent* emitterComponent = std::get<0>(value.second);
        TimedAgentEmitterComponent* timedEmitterComponent = std::get<1>(value.second);
        OgreSceneNodeComponent* sceneNodeComponent = std::get<2>(value.second);
        timedEmitterComponent->m_timeSinceLastEmission += milliseconds;
        while (
            timedEmitterComponent->m_emitInterval > 0 and
            timedEmitterComponent->m_timeSinceLastEmission >= timedEmitterComponent->m_emitInterval
        ) {
            timedEmitterComponent->m_timeSinceLastEmission -= timedEmitterComponent->m_emitInterval;
            for (unsigned int i = 0; i < emitterComponent->m_particlesPerEmission; ++i) {
                 emitterComponent->emitAgent(sceneNodeComponent->m_transform.position);
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// AgentAbsorberSystem
////////////////////////////////////////////////////////////////////////////////

luabind::scope
AgentAbsorberSystem::luaBindings() {
    using namespace luabind;
    return class_<AgentAbsorberSystem, System>("AgentAbsorberSystem")
        .def(constructor<>())
    ;
}


struct AgentAbsorberSystem::Implementation {

    EntityFilter<
        AgentAbsorberComponent
    > m_absorbers;

    EntityFilter<
        AgentComponent
    > m_agents;

    btDiscreteDynamicsWorld* m_world = nullptr;

};


AgentAbsorberSystem::AgentAbsorberSystem()
  : m_impl(new Implementation())
{
}


AgentAbsorberSystem::~AgentAbsorberSystem() {}


void
AgentAbsorberSystem::init(
    GameState* gameState
) {
    System::init(gameState);
    m_impl->m_absorbers.setEntityManager(&gameState->entityManager());
    m_impl->m_agents.setEntityManager(&gameState->entityManager());
    m_impl->m_world = gameState->physicsWorld();
}


void
AgentAbsorberSystem::shutdown() {
    m_impl->m_absorbers.setEntityManager(nullptr);
    m_impl->m_agents.setEntityManager(nullptr);
    m_impl->m_world = nullptr;
    System::shutdown();
}


void
AgentAbsorberSystem::update(int) {
    for (const auto& entry : m_impl->m_absorbers) {
        AgentAbsorberComponent* absorber = std::get<0>(entry.second);
        absorber->m_absorbedAgents.clear();
    }
    auto dispatcher = m_impl->m_world->getDispatcher();
    int numManifolds = dispatcher->getNumManifolds();
    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold* contactManifold = dispatcher->getManifoldByIndexInternal(i);
        auto objectA = static_cast<const btCollisionObject*>(contactManifold->getBody0());
        auto objectB = static_cast<const btCollisionObject*>(contactManifold->getBody1());
        EntityId entityA = reinterpret_cast<size_t>(objectA->getUserPointer());
        EntityId entityB = reinterpret_cast<size_t>(objectB->getUserPointer());
        AgentAbsorberComponent* absorber = nullptr;
        AgentComponent* agent = nullptr;
        if (
            m_impl->m_agents.containsEntity(entityA) and
            m_impl->m_absorbers.containsEntity(entityB)
        ) {
            agent = std::get<0>(
                m_impl->m_agents.entities().at(entityA)
            );
            absorber = std::get<0>(
                m_impl->m_absorbers.entities().at(entityB)
            );
        }
        else if (
            m_impl->m_absorbers.containsEntity(entityA) and
            m_impl->m_agents.containsEntity(entityB)
        ) {
            absorber = std::get<0>(
                m_impl->m_absorbers.entities().at(entityA)
            );
            agent = std::get<0>(
                m_impl->m_agents.entities().at(entityB)
            );
        }
        if (agent and absorber and absorber->canAbsorbAgent(agent->m_agentId) and agent->m_timeToLive > 0) {
            absorber->m_absorbedAgents[agent->m_agentId] += agent->m_potency;
            agent->m_timeToLive = 0;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// AgentRegistry
////////////////////////////////////////////////////////////////////////////////

luabind::scope
AgentRegistry::luaBindings() {
    using namespace luabind;
    return class_<AgentRegistry>("AgentRegistry")
        .scope
        [
            def("registerAgentType", &AgentRegistry::registerAgentType),
            def("getAgentDisplayName", &AgentRegistry::getAgentDisplayName),
            def("getAgentInternalName", &AgentRegistry::getAgentInternalName),
            def("getAgentId", &AgentRegistry::getAgentId)
        ]
    ;
}


namespace {
    struct AgentRegistryEntry
    {
        std::string internalName;
        std::string displayName;
    };
}

static std::vector<AgentRegistryEntry>&
agentRegistry() {
    static std::vector<AgentRegistryEntry> agentRegistry;
    return agentRegistry;
}
static std::unordered_map<std::string, AgentId>&
agentRegistryMap() {
    static std::unordered_map<std::string, AgentId> agentRegistryMap;
    return agentRegistryMap;
}

AgentId
AgentRegistry::registerAgentType(
    const std::string& internalName,
    const std::string& displayName
) {
    if (agentRegistryMap().count(internalName) == 0)
    {
        AgentRegistryEntry entry;
        entry.internalName = internalName;
        entry.displayName = displayName;
        agentRegistry().push_back(entry);
        agentRegistryMap().emplace(std::string(internalName), agentRegistry().size());
        return agentRegistry().size();
    }
    else
    {
        throw std::invalid_argument("Duplicate internalName not allowed.");
    }
}

std::string
AgentRegistry::getAgentDisplayName(
    AgentId id
) {
    if (static_cast<std::size_t>(id) > agentRegistry().size())
        throw std::out_of_range("Index of agent does not exist.");
    return agentRegistry()[id-1].displayName;
}

std::string
AgentRegistry::getAgentInternalName(
    AgentId id
) {
    if (static_cast<std::size_t>(id) > agentRegistry().size())
        throw std::out_of_range("Index of agent does not exist.");
    return agentRegistry()[id-1].internalName;
}

AgentId
AgentRegistry::getAgentId(
    const std::string& internalName
) {
    AgentId agentId;
    try
    {
        agentId = agentRegistryMap().at(internalName);
    }
    catch(std::out_of_range&)
    {
        throw std::out_of_range("Internal name of agent does not exist.");
    }
    return agentId;
}


