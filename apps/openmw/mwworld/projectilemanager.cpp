#include "projectilemanager.hpp"

#include <OgreSceneManager.h>

#include <libs/openengine/bullet/physic.hpp>

#include "../mwworld/manualref.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"

#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"

#include "../mwmechanics/combat.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/spellcasting.hpp"

#include "../mwrender/effectmanager.hpp"

#include "../mwsound/sound.hpp"

namespace MWWorld
{

    ProjectileManager::ProjectileManager(Ogre::SceneManager* sceneMgr, OEngine::Physic::PhysicEngine &engine)
        : mPhysEngine(engine)
        , mSceneMgr(sceneMgr)
    {

    }

    void ProjectileManager::createModel(State &state, const std::string &model)
    {
        state.mObject = NifOgre::Loader::createObjects(state.mNode, model);
        for(size_t i = 0;i < state.mObject->mControllers.size();i++)
        {
            if(state.mObject->mControllers[i].getSource().isNull())
                state.mObject->mControllers[i].setSource(Ogre::SharedPtr<MWRender::EffectAnimationTime> (new MWRender::EffectAnimationTime()));
        }
    }

    void ProjectileManager::update(NifOgre::ObjectScenePtr object, float duration)
    {
        for(size_t i = 0; i < object->mControllers.size() ;i++)
        {
            MWRender::EffectAnimationTime* value = dynamic_cast<MWRender::EffectAnimationTime*>(object->mControllers[i].getSource().get());
            if (value)
                value->addTime(duration);

            object->mControllers[i].update();
        }
    }

    void ProjectileManager::launchMagicBolt(const std::string &model, const std::string &sound,
                                            const std::string &spellId, float speed, bool stack,
                                            const ESM::EffectList &effects, const Ptr &actor, const std::string &sourceName)
    {
        // Spawn at 0.75 * ActorHeight
        float height = mPhysEngine.getCharacter(actor.getRefData().getHandle())->getHalfExtents().z * 2 * 0.75;

        Ogre::Vector3 pos(actor.getRefData().getPosition().pos);
        pos.z += height;

        Ogre::Quaternion orient = Ogre::Quaternion(Ogre::Radian(actor.getRefData().getPosition().rot[2]), Ogre::Vector3::NEGATIVE_UNIT_Z) *
                Ogre::Quaternion(Ogre::Radian(actor.getRefData().getPosition().rot[0]), Ogre::Vector3::NEGATIVE_UNIT_X);

        MagicBoltState state;
        state.mSourceName = sourceName;
        state.mId = spellId;
        state.mActorId = actor.getClass().getCreatureStats(actor).getActorId();
        state.mSpeed = speed;
        state.mStack = stack;

        // Only interested in "on target" effects
        for (std::vector<ESM::ENAMstruct>::const_iterator iter (effects.mList.begin());
            iter!=effects.mList.end(); ++iter)
        {
            if (iter->mRange == ESM::RT_Target)
                state.mEffects.mList.push_back(*iter);
        }

        MWWorld::ManualRef ref(MWBase::Environment::get().getWorld()->getStore(), model);
        MWWorld::Ptr ptr = ref.getPtr();

        state.mNode = mSceneMgr->getRootSceneNode()->createChildSceneNode(pos, orient);
        createModel(state, ptr.getClass().getModel(ptr));

        MWBase::SoundManager *sndMgr = MWBase::Environment::get().getSoundManager();
        state.mSound = sndMgr->playManualSound3D(pos, sound, 1.0f, 1.0f, MWBase::SoundManager::Play_TypeSfx, MWBase::SoundManager::Play_Loop);

        mMagicBolts.push_back(state);
    }

    void ProjectileManager::launchProjectile(Ptr actor, Ptr projectile, const Ogre::Vector3 &pos,
                                             const Ogre::Quaternion &orient, Ptr bow, float speed)
    {
        ProjectileState state;
        state.mActorId = actor.getClass().getCreatureStats(actor).getActorId();
        state.mBowId = bow.getCellRef().mRefID;
        state.mVelocity = orient.yAxis() * speed;
        state.mProjectileId = projectile.getCellRef().mRefID;

        MWWorld::ManualRef ref(MWBase::Environment::get().getWorld()->getStore(), projectile.getCellRef().mRefID);
        MWWorld::Ptr ptr = ref.getPtr();

        state.mNode = mSceneMgr->getRootSceneNode()->createChildSceneNode(pos, orient);
        createModel(state, ptr.getClass().getModel(ptr));

        mProjectiles.push_back(state);
    }

    void ProjectileManager::update(float dt)
    {
        moveProjectiles(dt);
        moveMagicBolts(dt);
    }

    void ProjectileManager::moveMagicBolts(float duration)
    {
        for (std::vector<MagicBoltState>::iterator it = mMagicBolts.begin(); it != mMagicBolts.end();)
        {
            Ogre::Quaternion orient = it->mNode->getOrientation();
            static float fTargetSpellMaxSpeed = MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>()
                        .find("fTargetSpellMaxSpeed")->getFloat();
            float speed = fTargetSpellMaxSpeed * it->mSpeed;

            Ogre::Vector3 direction = orient.yAxis();
            direction.normalise();
            Ogre::Vector3 pos(it->mNode->getPosition());
            Ogre::Vector3 newPos = pos + direction * duration * speed;

            it->mSound->setPosition(newPos);

            it->mNode->setPosition(newPos);

            update(it->mObject, duration);

            // Check for impact
            // TODO: use a proper btRigidBody / btGhostObject?
            btVector3 from(pos.x, pos.y, pos.z);
            btVector3 to(newPos.x, newPos.y, newPos.z);
            std::vector<std::pair<float, std::string> > collisions = mPhysEngine.rayTest2(from, to);
            bool hit=false;

            for (std::vector<std::pair<float, std::string> >::iterator cIt = collisions.begin(); cIt != collisions.end() && !hit; ++cIt)
            {
                MWWorld::Ptr obstacle = MWBase::Environment::get().getWorld()->searchPtrViaHandle(cIt->second);

                MWWorld::Ptr caster = MWBase::Environment::get().getWorld()->searchPtrViaActorId(it->mActorId);
                if (caster.isEmpty())
                    caster = obstacle;

                if (obstacle.isEmpty())
                {
                    // Terrain
                }
                else
                {
                    MWMechanics::CastSpell cast(caster, obstacle);
                    cast.mHitPosition = pos;
                    cast.mId = it->mId;
                    cast.mSourceName = it->mSourceName;
                    cast.mStack = it->mStack;
                    cast.inflict(obstacle, caster, it->mEffects, ESM::RT_Target, false, true);
                }

                hit = true;
            }
            if (hit)
            {
                MWWorld::Ptr caster = MWBase::Environment::get().getWorld()->searchPtrViaActorId(it->mActorId);
                MWBase::Environment::get().getWorld()->explodeSpell(pos, it->mEffects, caster, it->mId, it->mSourceName);

                MWBase::Environment::get().getSoundManager()->stopSound(it->mSound);

                mSceneMgr->destroySceneNode(it->mNode);

                it = mMagicBolts.erase(it);
                continue;
            }
            else
                ++it;
        }
    }

    void ProjectileManager::moveProjectiles(float duration)
    {
        for (std::vector<ProjectileState>::iterator it = mProjectiles.begin(); it != mProjectiles.end();)
        {
            // gravity constant - must be way lower than the gravity affecting actors, since we're not
            // simulating aerodynamics at all
            it->mVelocity -= Ogre::Vector3(0, 0, 627.2f * 0.1f) * duration;

            Ogre::Vector3 pos(it->mNode->getPosition());
            Ogre::Vector3 newPos = pos + it->mVelocity * duration;

            Ogre::Quaternion orient = Ogre::Vector3::UNIT_Y.getRotationTo(it->mVelocity);
            it->mNode->setOrientation(orient);
            it->mNode->setPosition(newPos);

            update(it->mObject, duration);

            // Check for impact
            // TODO: use a proper btRigidBody / btGhostObject?
            btVector3 from(pos.x, pos.y, pos.z);
            btVector3 to(newPos.x, newPos.y, newPos.z);
            std::vector<std::pair<float, std::string> > collisions = mPhysEngine.rayTest2(from, to);
            bool hit=false;

            for (std::vector<std::pair<float, std::string> >::iterator cIt = collisions.begin(); cIt != collisions.end() && !hit; ++cIt)
            {
                MWWorld::Ptr obstacle = MWBase::Environment::get().getWorld()->searchPtrViaHandle(cIt->second);

                MWWorld::Ptr caster = MWBase::Environment::get().getWorld()->searchPtrViaActorId(it->mActorId);

                // Arrow intersects with player immediately after shooting :/
                if (obstacle == caster)
                    continue;

                if (obstacle.isEmpty())
                {
                    // Terrain
                }
                else if (obstacle.getClass().isActor())
                {                    
                    MWWorld::ManualRef projectileRef(MWBase::Environment::get().getWorld()->getStore(), it->mProjectileId);

                    // Try to get a Ptr to the bow that was used. It might no longer exist.
                    MWWorld::Ptr bow = projectileRef.getPtr();
                    if (!caster.isEmpty())
                    {
                        MWWorld::InventoryStore& inv = caster.getClass().getInventoryStore(caster);
                        MWWorld::ContainerStoreIterator invIt = inv.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                        if (invIt != inv.end() && Misc::StringUtils::ciEqual(invIt->getCellRef().mRefID, it->mBowId))
                            bow = *invIt;
                    }

                    if (caster.isEmpty())
                        caster = obstacle;

                    MWMechanics::projectileHit(caster, obstacle, bow, projectileRef.getPtr(), pos + (newPos - pos) * cIt->first);
                }
                hit = true;
            }
            if (hit)
            {
                mSceneMgr->destroySceneNode(it->mNode);

                it = mProjectiles.erase(it);
                continue;
            }
            else
                ++it;
        }
    }

    void ProjectileManager::clear()
    {
        for (std::vector<ProjectileState>::iterator it = mProjectiles.begin(); it != mProjectiles.end(); ++it)
        {
            mSceneMgr->destroySceneNode(it->mNode);
        }
        mProjectiles.clear();
        for (std::vector<MagicBoltState>::iterator it = mMagicBolts.begin(); it != mMagicBolts.end(); ++it)
        {
            MWBase::Environment::get().getSoundManager()->stopSound(it->mSound);
            mSceneMgr->destroySceneNode(it->mNode);
        }
        mMagicBolts.clear();
    }

}
