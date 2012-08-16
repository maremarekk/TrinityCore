/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2006-2010 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
SDName: trial_of_the_crusader
SD%Complete: ??%
SDComment: based on /dev/rsa
SDCategory: Crusader Coliseum
EndScriptData */

// Known bugs:
//    - They should be floating but they aren't respecting the floor =(
//    - Hardcoded bullets spawner

#include "ScriptPCH.h"
#include "trial_of_the_crusader.h"

enum Yells
{
    SAY_AGGRO           = -1649040,
    SAY_DEATH           = -1649041,
    SAY_BERSERK         = -1649042,
    EMOTE_SHIELD        = -1649043,
    SAY_SHIELD          = -1649044,
    SAY_KILL1           = -1649045,
    SAY_KILL2           = -1649046,
    EMOTE_LIGHT_VORTEX  = -1649047,
    SAY_LIGHT_VORTEX    = -1649048,
    EMOTE_DARK_VORTEX   = -1649049,
    SAY_DARK_VORTEX     = -1649050,
};

enum Equipment
{
    EQUIP_MAIN_1         = 9423,
    EQUIP_MAIN_2         = 37377,
};

enum Summons
{
    NPC_BULLET_CONTROLLER        = 34743,

    NPC_BULLET_DARK              = 34628,
    NPC_BULLET_LIGHT             = 34630,
};

enum BossSpells
{
    SPELL_LIGHT_TWIN_SPIKE      = 66075,
    SPELL_LIGHT_SURGE           = 65766,
    SPELL_LIGHT_SHIELD          = 65858,
    SPELL_LIGHT_TWIN_PACT       = 65876,
    SPELL_LIGHT_VORTEX          = 66046,
    SPELL_LIGHT_TOUCH           = 67297,
    SPELL_LIGHT_ESSENCE         = 65686,
    SPELL_EMPOWERED_LIGHT       = 65748,
    SPELL_TWIN_EMPATHY_LIGHT    = 66133,
    SPELL_UNLEASHED_LIGHT       = 65795,

    SPELL_DARK_TWIN_SPIKE       = 66069,
    SPELL_DARK_SURGE            = 65768,
    SPELL_DARK_SHIELD           = 65874,
    SPELL_DARK_TWIN_PACT        = 65875,
    SPELL_DARK_VORTEX           = 66058,
    SPELL_DARK_TOUCH            = 67282,
    SPELL_DARK_ESSENCE          = 65684,
    SPELL_EMPOWERED_DARK        = 65724,
    SPELL_TWIN_EMPATHY_DARK     = 66132,
    SPELL_UNLEASHED_DARK        = 65808,

    SPELL_CONTROLLER_PERIODIC    = 66149,
    SPELL_POWER_TWINS           = 65879,
    SPELL_BERSERK               = 64238,
    SPELL_POWERING_UP           = 67590,
    SPELL_SURGE_OF_SPEED        = 65828,
};

enum Events
{
    EVENT_SPECIALABILITY        = 1,
    EVENT_SPIKE,
    EVENT_TOUCH,
    EVENT_BERSERK
};

#define SPELL_DARK_ESSENCE_HELPER RAID_MODE<uint32>(65684, 67176, 67177, 67178)
#define SPELL_LIGHT_ESSENCE_HELPER RAID_MODE<uint32>(65686, 67222, 67223, 67224)

#define SPELL_POWERING_UP_HELPER RAID_MODE<uint32>(67590, 67602, 67603, 67604)

#define SPELL_UNLEASHED_DARK_HELPER RAID_MODE<uint32>(65808, 67172, 67173, 67174)
#define SPELL_UNLEASHED_LIGHT_HELPER RAID_MODE<uint32>(65795, 67238, 67239, 67240)

enum Actions
{
    ACTION_VORTEX,
    ACTION_PACT,
};

/*######
## boss_twin_base
######*/

class OrbsDespawner : public BasicEvent
{
    public:
        explicit OrbsDespawner(Creature* creature) : _creature(creature)
        {
        }

        bool Execute(uint64 /*currTime*/, uint32 /*diff*/)
        {
            Trinity::CreatureWorker<OrbsDespawner> worker(_creature, *this);
            _creature->VisitNearbyGridObject(5000.0f, worker);
            return true;
        }

        void operator()(Creature* creature) const
        {
            switch (creature->GetEntry())
            {
                case NPC_BULLET_DARK:
                case NPC_BULLET_LIGHT:
                    creature->DespawnOrUnsummon();
                    return;
                default:
                    return;
            }
        }

    private:
        Creature* _creature;
};

struct boss_twin_baseAI : public ScriptedAI
{
    boss_twin_baseAI(Creature* creature) : ScriptedAI(creature), Summons(me)
    {
        m_instance = (InstanceScript*)creature->GetInstanceScript();
    }

    InstanceScript* m_instance;
    SummonList Summons;

    AuraStateType  m_uiAuraState;

    uint32 m_uiWeapon;

    EventMap events;

    int32 m_uiVortexSay;
    int32 m_uiVortexEmote;
    uint32 m_uiSisterNpcId;
    uint32 m_uiMyEmphatySpellId;
    uint32 m_uiOtherEssenceSpellId;
    uint32 m_uiSurgeSpellId;
    uint32 m_uiVortexSpellId;
    uint32 m_uiShieldSpellId;
    uint32 m_uiTwinPactSpellId;
    uint32 m_uiSpikeSpellId;
    uint32 m_uiTouchSpellId;

    void Reset()
    {
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE);
        me->SetReactState(REACT_PASSIVE);
        me->ModifyAuraState(m_uiAuraState, true);
        /* Uncomment this once that they are flying above the ground
        me->AddUnitMovementFlag(MOVEMENTFLAG_LEVITATING);
        me->SetFlying(true); */
        events.Reset();
        Summons.DespawnAll();
    }

    void JustReachedHome()
    {
        if (m_instance)
            m_instance->SetData(TYPE_VALKIRIES, FAIL);

        Summons.DespawnAll();
        me->DespawnOrUnsummon();
    }

    void MovementInform(uint32 uiType, uint32 uiId)
    {
        if (uiType != POINT_MOTION_TYPE) return;

        switch (uiId)
        {
            case 1:
                m_instance->DoUseDoorOrButton(m_instance->GetData64(GO_MAIN_GATE_DOOR));
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE);
                me->SetReactState(REACT_AGGRESSIVE);
                break;
        }
    }

    void DamageTaken(Unit* attacker, uint32& damage) 
    {
        if (attacker == me)
            return;

        damage /= 2;
        //Hack, can cause some problems in damege reductions etc, needs test
        if (Creature* sister = GetSister())
        {
            // do nonkill damage only to alive
            if (sister->isAlive() && sister->GetHealth() > damage){
                sister->ModifyHealth(-int32(damage));
                sister->LowerPlayerDamageReq(damage);
            }
        }    
    }

    void KilledUnit(Unit* who)
    {
        if (who->GetTypeId() == TYPEID_PLAYER)
        {
            DoScriptText(urand(0, 1) ? SAY_KILL1 : SAY_KILL2, me);
            if (m_instance)
                m_instance->SetData(DATA_TRIBUTE_TO_IMMORTALITY_ELEGIBLE, 0);
        }
    }

    void JustSummoned(Creature* summoned)
    {
        Summons.Summon(summoned);
    }

    void SummonedCreatureDespawn(Creature* summoned)
    {
        switch (summoned->GetEntry())
        {
            case NPC_LIGHT_ESSENCE:
                m_instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_LIGHT_ESSENCE_HELPER);
                m_instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_POWERING_UP_HELPER);
                break;
            case NPC_DARK_ESSENCE:
                m_instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_DARK_ESSENCE_HELPER);
                m_instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_POWERING_UP_HELPER);
                break;
            case NPC_BULLET_CONTROLLER:
                me->m_Events.AddEvent(new OrbsDespawner(me), me->m_Events.CalculateTime(100));
                break;
        }
        Summons.Despawn(summoned);
    }

    void JustDied(Unit* killer)
    {
        DoScriptText(SAY_DEATH, me);
        if (m_instance)
        {
            if (Creature* pSister = GetSister())
            {
                if (pSister->isAlive())
                {
                    killer->Kill(pSister);
                }

                me->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                pSister->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
            }
            m_instance->SetData(TYPE_VALKIRIES, DONE);
        }
        Summons.DespawnAll();
    }

    // Called when sister pointer needed
    Creature* GetSister()
    {
        return Unit::GetCreature((*me), m_instance->GetData64(m_uiSisterNpcId));
    }

    void EnterCombat(Unit* /*who*/)
    {
        me->SetInCombatWithZone();
        if (m_instance)
        {
            if (Creature* pSister = GetSister())
            {
                //me->AddAura(m_uiMyEmphatySpellId, pSister);
                pSister->SetInCombatWithZone();
            }
            m_instance->SetData(TYPE_VALKIRIES, IN_PROGRESS);
        }

        events.ScheduleEvent(EVENT_SPIKE, 20*IN_MILLISECONDS);
        if (IsHeroic())
            events.ScheduleEvent(EVENT_TOUCH, urand(10, 15)*IN_MILLISECONDS);
        events.ScheduleEvent(EVENT_BERSERK, IsHeroic() ? 6*MINUTE*IN_MILLISECONDS : 10*MINUTE*IN_MILLISECONDS);

        DoScriptText(SAY_AGGRO, me);
        DoCast(me, m_uiSurgeSpellId);
    }

    void DoAction(const int32 action)
    {
        switch (action)
        {
            case ACTION_VORTEX:
                DoScriptText(m_uiVortexEmote, me);
                DoScriptText(m_uiVortexSay, me);
                DoCastAOE(m_uiVortexSpellId);
                events.ScheduleEvent(EVENT_SPECIALABILITY, 45*IN_MILLISECONDS);
                break;
            case ACTION_PACT:
                DoScriptText(EMOTE_SHIELD, me);
                DoScriptText(SAY_SHIELD, me);
                if (Creature* pSister = GetSister())
                    pSister->CastSpell(pSister, SPELL_POWER_TWINS, false);
                DoCast(me, m_uiShieldSpellId);
                DoCast(me, m_uiTwinPactSpellId);
                events.ScheduleEvent(EVENT_SPECIALABILITY, 45*IN_MILLISECONDS);
                break;
        }
    }

    void EnableDualWield(bool mode = true)
    {
        SetEquipmentSlots(false, m_uiWeapon, mode ? m_uiWeapon : int32(EQUIP_UNEQUIP), EQUIP_UNEQUIP);
        me->SetCanDualWield(mode);
        me->UpdateDamagePhysical(mode ? OFF_ATTACK : BASE_ATTACK);
    }

    void DoRandomSpecialAbility()
    {
        if (!m_instance)
            return;

        uint32 ability = m_instance->GetData(DATA_VALKYR_RANDOM_ABILITY);

        if (Creature* target = ((me->GetEntry() == NPC_LIGHTBANE) == bool(ability & (1 << 1))) ? me : GetSister())
            target->GetAI()->DoAction((ability & (1 << 0)) ? ACTION_PACT : ACTION_VORTEX);
    }

    void UpdateAI(const uint32 uiDiff)
    {
        if (!m_instance || !UpdateVictim())
            return;

        events.Update(uiDiff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SPECIALABILITY:
                    events.CancelEvent(EVENT_SPECIALABILITY);
                    DoRandomSpecialAbility();
                    break;
                case EVENT_SPIKE:
                    DoCastVictim(m_uiSpikeSpellId);
                    events.RescheduleEvent(EVENT_SPIKE, 20*IN_MILLISECONDS);
                    break;
                case EVENT_TOUCH:
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 200, true, m_uiOtherEssenceSpellId))
                        me->CastCustomSpell(m_uiTouchSpellId, SPELLVALUE_MAX_TARGETS, 1, target, false);
                    events.RescheduleEvent(EVENT_TOUCH, urand(10, 15)*IN_MILLISECONDS);
                    break;
                case EVENT_BERSERK:
                    DoCast(me, SPELL_BERSERK);
                    DoScriptText(SAY_BERSERK, me);
                    events.CancelEvent(EVENT_BERSERK);
            }
        }

        DoMeleeAttackIfReady();
    }
};

/*######
## boss_fjola
######*/

class boss_fjola : public CreatureScript
{
public:
    boss_fjola() : CreatureScript("boss_fjola") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_fjolaAI(creature);
    }

    struct boss_fjolaAI : public boss_twin_baseAI
    {
        boss_fjolaAI(Creature* creature) : boss_twin_baseAI(creature)
        {
            m_instance = (InstanceScript*)creature->GetInstanceScript();
        }

        InstanceScript* m_instance;

        void Reset()
        {
            SetEquipmentSlots(false, EQUIP_MAIN_1, EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
            m_uiWeapon = EQUIP_MAIN_1;
            m_uiAuraState = AURA_STATE_UNKNOWN22;
            m_uiVortexEmote = EMOTE_LIGHT_VORTEX;
            m_uiVortexSay = SAY_LIGHT_VORTEX;
            m_uiSisterNpcId = NPC_DARKBANE;
            m_uiMyEmphatySpellId = SPELL_TWIN_EMPATHY_DARK;
            m_uiOtherEssenceSpellId = SPELL_DARK_ESSENCE_HELPER;
            m_uiSurgeSpellId = SPELL_LIGHT_SURGE;
            m_uiVortexSpellId = SPELL_LIGHT_VORTEX;
            m_uiShieldSpellId = SPELL_LIGHT_SHIELD;
            m_uiTwinPactSpellId = SPELL_LIGHT_TWIN_PACT;
            m_uiTouchSpellId = SPELL_LIGHT_TOUCH;
            m_uiSpikeSpellId = SPELL_LIGHT_TWIN_SPIKE;
            boss_twin_baseAI::Reset();

            if (m_instance)
            {
                m_instance->DoStopTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT,  EVENT_START_TWINS_FIGHT);
            }
        }

        void EnterCombat(Unit* who)
        {
            if (m_instance)
            {
                m_instance->DoStartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT,  EVENT_START_TWINS_FIGHT);
            }

            me->SummonCreature(NPC_BULLET_CONTROLLER, ToCCommonLoc[1].GetPositionX(), ToCCommonLoc[1].GetPositionY(), ToCCommonLoc[1].GetPositionZ(), 0.0f, TEMPSUMMON_MANUAL_DESPAWN);
            boss_twin_baseAI::EnterCombat(who);
        }

        void EnterEvadeMode()
        {
            m_instance->DoUseDoorOrButton(m_instance->GetData64(GO_MAIN_GATE_DOOR));
            boss_twin_baseAI::EnterEvadeMode();
        }

        void JustReachedHome()
        {
            if (m_instance)
                m_instance->DoUseDoorOrButton(m_instance->GetData64(GO_MAIN_GATE_DOOR));

            boss_twin_baseAI::JustReachedHome();
        }
    };

};

/*######
## boss_eydis
######*/

class boss_eydis : public CreatureScript
{
public:
    boss_eydis() : CreatureScript("boss_eydis") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_eydisAI(creature);
    }

    struct boss_eydisAI : public boss_twin_baseAI
    {
        boss_eydisAI(Creature* creature) : boss_twin_baseAI(creature) {}

        void Reset()
        {
            SetEquipmentSlots(false, EQUIP_MAIN_2, EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
            m_uiWeapon = EQUIP_MAIN_2;
            m_uiAuraState = AURA_STATE_UNKNOWN19;
            m_uiVortexEmote = EMOTE_DARK_VORTEX;
            m_uiVortexSay = SAY_DARK_VORTEX;
            m_uiSisterNpcId = NPC_LIGHTBANE;
            m_uiMyEmphatySpellId = SPELL_TWIN_EMPATHY_LIGHT;
            m_uiOtherEssenceSpellId = SPELL_LIGHT_ESSENCE_HELPER;
            m_uiSurgeSpellId = SPELL_DARK_SURGE;
            m_uiVortexSpellId = SPELL_DARK_VORTEX;
            m_uiShieldSpellId = SPELL_DARK_SHIELD;
            m_uiTwinPactSpellId = SPELL_DARK_TWIN_PACT;
            m_uiTouchSpellId = SPELL_DARK_TOUCH;
            m_uiSpikeSpellId = SPELL_DARK_TWIN_SPIKE;
            boss_twin_baseAI::Reset();
        }

        void EnterCombat(Unit* who)
        {
            events.ScheduleEvent(EVENT_SPECIALABILITY, 45*IN_MILLISECONDS);
            boss_twin_baseAI::EnterCombat(who);
        }
    };

};

#define ESSENCE_REMOVE 0
#define ESSENCE_APPLY 1

class mob_essence_of_twin : public CreatureScript
{
    public:
        mob_essence_of_twin() : CreatureScript("mob_essence_of_twin") { }

        struct mob_essence_of_twinAI : public ScriptedAI
        {
            mob_essence_of_twinAI(Creature* creature) : ScriptedAI(creature) { }

            uint32 GetData(uint32 data)
            {
                uint32 spellReturned = 0;
                switch (me->GetEntry())
                {
                    case NPC_LIGHT_ESSENCE:
                        spellReturned = data == ESSENCE_REMOVE? SPELL_DARK_ESSENCE_HELPER : SPELL_LIGHT_ESSENCE_HELPER;
                        break;
                    case NPC_DARK_ESSENCE:
                        spellReturned = data == ESSENCE_REMOVE? SPELL_LIGHT_ESSENCE_HELPER : SPELL_DARK_ESSENCE_HELPER;
                        break;
                    default:
                        break;
                }

                return spellReturned;
            }
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new mob_essence_of_twinAI(creature);
        };

        bool OnGossipHello(Player* player, Creature* creature)
        {
            player->RemoveAurasDueToSpell(creature->GetAI()->GetData(ESSENCE_REMOVE));
            player->CastSpell(player, creature->GetAI()->GetData(ESSENCE_APPLY), true);
            player->CLOSE_GOSSIP_MENU();
            return true;
        }
};

struct mob_unleashed_ballAI : public ScriptedAI
{
    mob_unleashed_ballAI(Creature* creature) : ScriptedAI(creature)
    {
        m_instance = (InstanceScript*)creature->GetInstanceScript();
    }

    InstanceScript* m_instance;
    uint32 m_uiRangeCheckTimer;

    void MoveToNextPoint()
    {
        float x0 = ToCCommonLoc[1].GetPositionX(), y0 = ToCCommonLoc[1].GetPositionY(), r = 47.0f;
        float y = y0;
        float x = frand(x0 - r, x0 + r);
        float sq = pow(r, 2) - pow(x - x0, 2);
        float rt = sqrtf(fabs(sq));
        if (urand(0, 1))
            y = y0 + rt;
        else
            y = y0 - rt;
        me->GetMotionMaster()->MovePoint(0, x, y, me->GetPositionZ());
    }

    void Reset()
    {
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
        me->SetReactState(REACT_PASSIVE);
        me->AddUnitMovementFlag(MOVEMENTFLAG_LEVITATING);
        me->SetFlying(true);
        me->SetSpeed(MOVE_WALK,0.7,false);
        me->SetSpeed(MOVE_RUN,0.7,false);
        me->SetSpeed(MOVE_FLIGHT,0.7,false);
        SetCombatMovement(false);
        MoveToNextPoint();
        m_uiRangeCheckTimer = IN_MILLISECONDS;
    }

    void MovementInform(uint32 uiType, uint32 uiId)
    {
        if (uiType != POINT_MOTION_TYPE) return;

        switch (uiId)
        {
            case 0:
                if (urand(0, 3) == 0)
                    MoveToNextPoint();
                else
                    me->DisappearAndDie();
                break;
        }
    }
};

class mob_unleashed_dark : public CreatureScript
{
public:
    mob_unleashed_dark() : CreatureScript("mob_unleashed_dark") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_unleashed_darkAI(creature);
    }

    struct mob_unleashed_darkAI : public mob_unleashed_ballAI
    {
        mob_unleashed_darkAI(Creature* creature) : mob_unleashed_ballAI(creature) {}

        void UpdateAI(const uint32 uiDiff)
        {
            if (m_uiRangeCheckTimer < uiDiff)
            {
                if (Unit* target = me->SelectNearestPlayer(2.2f))
                    if (/*target->GetTypeId() == TYPEID_PLAYER && */ target->isAlive())
                    {
                        if (target->HasAura(SPELL_DARK_ESSENCE_HELPER))
                        {
                            DoCast(SPELL_UNLEASHED_DARK_HELPER);
                            me->GetMotionMaster()->MoveIdle();
                            me->DespawnOrUnsummon(1000);
                        }
                        else 
                        {
                            DoCast(target,SPELL_UNLEASHED_DARK);
                            me->GetMotionMaster()->MoveIdle();
                            me->DespawnOrUnsummon(1000);
                        }
                    }
                m_uiRangeCheckTimer = IN_MILLISECONDS;
            }
            else m_uiRangeCheckTimer -= uiDiff;
        }

        void SpellHitTarget(Unit* who, SpellInfo const* spell)
        {
            if (spell->Id == SPELL_UNLEASHED_DARK_HELPER)
            {
                if (who->HasAura(SPELL_DARK_ESSENCE_HELPER))
                    who->CastCustomSpell(SPELL_POWERING_UP, SPELLVALUE_AURA_STACK, urand(6, 9), who, true);
            }
        }
    };

};

class mob_unleashed_light : public CreatureScript
{
public:
    mob_unleashed_light() : CreatureScript("mob_unleashed_light") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_unleashed_lightAI(creature);
    }

    struct mob_unleashed_lightAI : public mob_unleashed_ballAI
    {
        mob_unleashed_lightAI(Creature* creature) : mob_unleashed_ballAI(creature) {}

        void UpdateAI(const uint32 uiDiff)
        {
            if (m_uiRangeCheckTimer < uiDiff)
            {
                if (Unit* target = me->SelectNearestPlayer(2.2f))
                    if (/*target->GetTypeId() == TYPEID_PLAYER && */ target->isAlive())
                    {
                        if (target->HasAura(SPELL_LIGHT_ESSENCE_HELPER))
                        {
                            DoCast(SPELL_UNLEASHED_LIGHT_HELPER);
                            me->GetMotionMaster()->MoveIdle();
                            me->DespawnOrUnsummon(1000);
                        }
                        else
                        {
                            DoCast(target,SPELL_UNLEASHED_LIGHT);
                            me->GetMotionMaster()->MoveIdle();
                            me->DespawnOrUnsummon(1000);
                        }
                    }
                m_uiRangeCheckTimer = IN_MILLISECONDS;
            }
            else m_uiRangeCheckTimer -= uiDiff;
        }

        void SpellHitTarget(Unit* who, SpellInfo const* spell)
        {
            if (spell->Id == SPELL_UNLEASHED_LIGHT_HELPER)
            {
                if (who->HasAura(SPELL_LIGHT_ESSENCE_HELPER))
                    who->CastCustomSpell(SPELL_POWERING_UP, SPELLVALUE_AURA_STACK, urand(6, 9), who, true);
            }
        }
    };

};

class mob_bullet_controller : public CreatureScript
{
public:
    mob_bullet_controller() : CreatureScript("mob_bullet_controller") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new mob_bullet_controllerAI(creature);
    }

    struct mob_bullet_controllerAI : public Scripted_NoMovementAI
    {
        mob_bullet_controllerAI(Creature* creature) : Scripted_NoMovementAI(creature)
        {
            Reset();
        }

        void Reset()
        {
            DoCastAOE(SPELL_CONTROLLER_PERIODIC);
            burstTimer = urand(25, 30) * IN_MILLISECONDS;
        }

        void UpdateAI(const uint32 diff)
        {
            UpdateVictim();

            if (burstTimer <= diff)
            {

                if (me->GetMap()->IsHeroic())
                {
                    me->CastCustomSpell(66152, SPELLVALUE_MAX_TARGETS, urand(10, 12), me, true);
                    me->CastCustomSpell(66153, SPELLVALUE_MAX_TARGETS, urand(10, 12), me, true);
                }
                else
                {
                    me->CastCustomSpell(66152, SPELLVALUE_MAX_TARGETS, urand(8, 10), me, true);
                    me->CastCustomSpell(66153, SPELLVALUE_MAX_TARGETS, urand(8, 10), me, true);
                }


                burstTimer = urand(35, 40) * IN_MILLISECONDS;
            }
            else
                burstTimer -= diff;

        }

        uint32 burstTimer;
    };
};

class spell_powering_up : public SpellScriptLoader
{
    public:
        spell_powering_up() : SpellScriptLoader("spell_powering_up") { }

        class spell_powering_up_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_powering_up_AuraScript);

            void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Unit* target = GetUnitOwner())
                {
                    if (target->GetAuraCount(GetId()) > 99)
                        {
                            if (target->GetDummyAuraEffect(SPELLFAMILY_GENERIC, 2206, EFFECT_1))
                                target->CastSpell(target, SPELL_EMPOWERED_DARK, true);

                            if (target->GetDummyAuraEffect(SPELLFAMILY_GENERIC, 2845, EFFECT_1))
                                target->CastSpell(target, SPELL_EMPOWERED_LIGHT, true);

                            target->RemoveAurasDueToSpell(GetId());
                        }
                }
            }

            void Register()
            {
                OnEffectApply += AuraEffectApplyFn(spell_powering_up_AuraScript::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
                OnEffectApply += AuraEffectApplyFn(spell_powering_up_AuraScript::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAPPLY);
                OnEffectApply += AuraEffectApplyFn(spell_powering_up_AuraScript::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_CHANGE_AMOUNT);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_powering_up_AuraScript();
        }

        class spell_powering_up_SpellScript : public SpellScript
        {
            public:
                PrepareSpellScript(spell_powering_up_SpellScript)

            uint32 spellId;

            bool Validate(SpellEntry const* /*spellEntry*/)
            {
                spellId = sSpellMgr->GetSpellIdForDifficulty(SPELL_SURGE_OF_SPEED, GetCaster());
                if (!sSpellMgr->GetSpellInfo(spellId))
                    return false;
                return true;
            }

            void HandleScriptEffect(SpellEffIndex /*effIndex*/)
            {
                if (Unit* target = GetTargetUnit())
                    if (urand(0, 99) < 15)
                        target->CastSpell(target, spellId, true);
            }

            void Register()
            {
                OnEffectHitTarget += SpellEffectFn(spell_powering_up_SpellScript::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_powering_up_SpellScript();
        }
};

class spell_valkyr_essences : public SpellScriptLoader
{
    public:
        spell_valkyr_essences() : SpellScriptLoader("spell_valkyr_essences") { }

        class spell_valkyr_essences_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_valkyr_essences_AuraScript);

            uint32 spellId;

            bool Load()
            {
                spellId = sSpellMgr->GetSpellIdForDifficulty(SPELL_SURGE_OF_SPEED, GetCaster());
                if (!sSpellMgr->GetSpellInfo(spellId))
                    return false;
                return true;
            }

            void Absorb(AuraEffect* /*aurEff*/, DamageInfo & /*dmgInfo*/, uint32 & /*absorbAmount*/)
            {
                if (urand(0, 99) < 5)
                    GetTarget()->CastSpell(GetTarget(), spellId, true);
            }

            void Register()
            {
                OnEffectAbsorb += AuraEffectAbsorbFn(spell_valkyr_essences_AuraScript::Absorb, EFFECT_0);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_valkyr_essences_AuraScript();
        }
};

class spell_power_of_the_twins : public SpellScriptLoader
{
    public:
        spell_power_of_the_twins() : SpellScriptLoader("spell_power_of_the_twins") { }

        class spell_power_of_the_twins_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_power_of_the_twins_AuraScript);

            bool Load()
            {
                return GetCaster()->GetTypeId() == TYPEID_UNIT;
            }

            void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (InstanceScript* instance = GetCaster()->GetInstanceScript())
                {
                    if (Creature* Valk = ObjectAccessor::GetCreature(*GetCaster(), instance->GetData64(GetCaster()->GetEntry())))
                        CAST_AI(boss_twin_baseAI, Valk->AI())->EnableDualWield(true);
                }
            }

            void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (InstanceScript* instance = GetCaster()->GetInstanceScript())
                {
                    if (Creature* Valk = ObjectAccessor::GetCreature(*GetCaster(), instance->GetData64(GetCaster()->GetEntry())))
                        CAST_AI(boss_twin_baseAI, Valk->AI())->EnableDualWield(false);
                }
            }

            void Register()
            {
                AfterEffectApply += AuraEffectApplyFn(spell_power_of_the_twins_AuraScript::HandleEffectApply, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_REAL);
                AfterEffectRemove += AuraEffectRemoveFn(spell_power_of_the_twins_AuraScript::HandleEffectRemove, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);

            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_power_of_the_twins_AuraScript();
        }
};

void AddSC_boss_twin_valkyr()
{
    new boss_fjola();
    new boss_eydis();
    new mob_unleashed_light();
    new mob_unleashed_dark();
    new mob_essence_of_twin();
    new mob_bullet_controller();
    new spell_powering_up();
    new spell_valkyr_essences();
    new spell_power_of_the_twins();
}
