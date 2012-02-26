 /*
Creado para Reinos Iberos.

�Vamos all�! �Siempre arriba, siempre arriba!
 */

#include "ScriptPCH.h"
#include "shadowfang_keep.h"
#include "LFGMgr.h"

/*######
## apothecary hummel
######*/

enum Hummel
{
    FACTION_HOSTIL                   = 14,
    NPC_CRAZED_APOTHECARY            = 36568,
    NPC_VIAL_BUNNY                   = 36530,

    SAY_AGGRO_1                      = -1033020,
    SAY_AGGRO_2                      = -1033021,
    SAY_AGGRO_3                      = -1033022,
    SAY_CALL_BAXTER                  = -1033023,
    SAY_CALL_FRYE                    = -1033024,
    SAY_SUMMON_ADDS                  = -1033025,
    SAY_DEATH                        = -1033026,

    SPELL_ALLURING_PERFUME           = 68589,
    SPELL_ALLURING_PERFUME_SPRAY     = 68607,
    SPELL_IRRESISTIBLE_COLOGNE       = 68946,
    SPELL_IRRESISTIBLE_COLOGNE_SPRAY = 68948,

    SPELL_TABLE_APPEAR               = 69216,
    SPELL_SUMMON_TABLE               = 69218,
    SPELL_CHAIN_REACTION             = 68821,
    SPELL_UNSTABLE_REACTION          = 68957,

    // frye
    SPELL_THROW_PERFUME              = 68799,
    SPELL_THROW_COLOGNE              = 68841,
    SPELL_ALLURING_PERFUME_SPILL     = 68798,
    SPELL_IRRESISTIBLE_COLOGNE_SPILL = 68614,
};

enum Action
{
    START_INTRO,
    START_FIGHT,
    APOTHECARY_DIED,
    SPAWN_CRAZED
};

enum Phase
{
    PHASE_NORMAL,
    PHASE_INTRO
};

static Position Loc[]=
{
    // spawn
    {-215.776443f, 2242.365479f, 79.769257f, 0.0f},
    {-169.500702f, 2219.286377f, 80.613045f, 0.0f},
    {-200.056641f, 2152.635010f, 79.763107f, 0.0f},
    {-238.448242f, 2165.165283f, 89.582985f, 0.0f},
    // moveto
    {-210.784164f, 2219.004150f, 79.761803f, 0.0f},
    {-198.453400f, 2208.410889f, 79.762474f, 0.0f},
    {-208.469910f, 2167.971924f, 79.764969f, 0.0f},
    {-228.251511f, 2187.282471f, 79.762840f, 0.0f}
};

#define GOSSIP_ITEM_START "Comienza la batalla."

void SetInCombat(Creature* self)
{
    self->AI()->DoZoneInCombat(self, 150.0f);

    if (!self->isInCombat())
        self->AI()->EnterEvadeMode();
}

class npc_apothecary_hummel : public CreatureScript
{
    public:
        npc_apothecary_hummel() : CreatureScript("npc_apothecary_hummel") { }

        bool OnGossipHello(Player* player, Creature* creature)
        {
            if (creature->isQuestGiver())
                player->PrepareQuestMenu(creature->GetGUID());

            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_START, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
        {
            if (action == GOSSIP_ACTION_INFO_DEF + 1)
                creature->AI()->DoAction(START_INTRO);

            player->CLOSE_GOSSIP_MENU();
            return true;
        }

        struct npc_apothecary_hummelAI : public ScriptedAI
        {
            npc_apothecary_hummelAI(Creature* creature) : ScriptedAI(creature), _summons(me)
            {
                _instance = creature->GetInstanceScript();
            }

            void Reset()
            {
                me->RestoreFaction();
                me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                _phase = PHASE_NORMAL;
                _step = 0;
                _deadCount = 0;
                _stepTimer = 1500;
                _aggroTimer = 5000;
                _sprayTimer = urand(4000, 7000);
                _chainReactionTimer = urand(10000, 25000);
                _firstCrazed = false;

                me->SetCorpseDelay(900); 
                _summons.DespawnAll();

                if (!_instance)
                    return;

                _instance->SetData(TYPE_CROWN, NOT_STARTED);

                if (Creature* baxter = ObjectAccessor::GetCreature(*me, _instance->GetData64(DATA_BAXTER)))
                {
                    if (baxter->isAlive())
                        baxter->AI()->EnterEvadeMode();
                    else
                        baxter->Respawn();
                }

                if (Creature* frye = ObjectAccessor::GetCreature(*me, _instance->GetData64(DATA_FRYE)))
                {
                    if (frye->isAlive())
                        frye->AI()->EnterEvadeMode();
                    else
                        frye->Respawn();
                }

                if (GameObject* door = _instance->instance->GetGameObject(_instance->GetData64(DATA_DOOR)))
                    _instance->HandleGameObject(NULL, true, door);
            }

            void DoAction(int32 const action)
            {
                switch (action)
                {
                    case START_INTRO:
                    {
                        if (Creature* baxter = ObjectAccessor::GetCreature(*me, _instance? _instance->GetData64(DATA_BAXTER) : 0))
                            baxter->AI()->DoAction(START_INTRO);
                        if (Creature* frye = ObjectAccessor::GetCreature(*me, _instance ? _instance->GetData64(DATA_FRYE) : 0))
                            frye->AI()->DoAction(START_INTRO);

                        _phase = PHASE_INTRO;
                        me->SetReactState(REACT_PASSIVE);
                        me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                        SetInCombat(me);
                        break;
                    }
                    case START_FIGHT:
                    {
                        _phase = PHASE_NORMAL;
                        me->setFaction(FACTION_HOSTIL);
                        me->SetReactState(REACT_AGGRESSIVE);
                        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                        SetInCombat(me);
                        _instance->SetData(TYPE_CROWN, IN_PROGRESS);
                        break;
                    }
                    case APOTHECARY_DIED:
                    {
                        ++_deadCount;
                        if (_deadCount > 2) // Cuando los tres est�n muertos, ser�n loteables
                        {
                            _summons.DespawnAll();
                            me->SetCorpseDelay(90); // delay
                            me->setDeathState(JUST_DIED); // relog delay
                            _instance->SetData(TYPE_CROWN, DONE);
                            me->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);

                            Map* map = me->GetMap();
                            if (map && map->IsDungeon())
                            {
                                Map::PlayerList const& players = map->GetPlayers();
                                if (!players.isEmpty())
                                    for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
                                        if (Player* player = i->getSource())
                                            if (player->GetDistance(me) < 120.0f)
                                                sLFGMgr->RewardDungeonDoneFor(288, player);
                            }
                        }
                        else
                        {
                            if (me->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE))
                                me->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                        }
                        break;
                    }
                    case SPAWN_CRAZED:
                    {
                        uint8 i = urand(0, 3);
                        if (Creature* crazed = me->SummonCreature(NPC_CRAZED_APOTHECARY, Loc[i], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 3*IN_MILLISECONDS))
                        {
                            crazed->setFaction(FACTION_HOSTIL);
                            crazed->SetReactState(REACT_PASSIVE);
                            crazed->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                            crazed->GetMotionMaster()->MovePoint(1, Loc[i + 4]);
                        }

                        if (!_firstCrazed)
                        {
                            DoScriptText(SAY_SUMMON_ADDS, me);
                            _firstCrazed = true;
                        }
                        break;
                    }
                }
            }

            void UpdateAI(uint32 const diff)
            {
                if (_phase == PHASE_INTRO)
                {
                    if (_stepTimer <= diff)
                    {
                        ++_step;
                        switch (_step)
                        {
                            case 1:
                            {
                                DoScriptText(SAY_AGGRO_1, me);
                                _stepTimer = 4000;
                                break;
                            }
                            case 2:
                            {
                                DoScriptText(SAY_AGGRO_2, me);
                                _stepTimer = 5500;
                                break;
                            }
                            case 3:
                            {
                                DoScriptText(SAY_AGGRO_3, me);
                                _stepTimer = 1000;
                                break;
                            }
                            case 4:
                            {
                                DoAction(START_FIGHT);
                                break;
                            }
                        }
                    }
                    else
                        _stepTimer -= diff;
                }
                else // NORMAL FASE
                {
                    if (!UpdateVictim())
                        return;

                    if (_aggroTimer <= diff)
                    {
                        SetInCombat(me);
                        _aggroTimer = 5000;
                    }
                    else
                        _aggroTimer -= diff;

                    if (me->HasUnitState(UNIT_STAT_CASTING))
                        return;

                    if (_chainReactionTimer <= diff)
                    {
                        DoCast(me, SPELL_TABLE_APPEAR, true);
                        DoCast(me, SPELL_SUMMON_TABLE, true);
                        DoCast(SPELL_CHAIN_REACTION);
                        _chainReactionTimer = urand(15000, 25000);
                    }
                    else
                        _chainReactionTimer -= diff;

                    if (_sprayTimer <= diff)
                    {
                        DoCastVictim(SPELL_ALLURING_PERFUME_SPRAY);
                        _sprayTimer = urand(8000, 15000);
                    }
                    else
                        _sprayTimer -= diff;

                    DoMeleeAttackIfReady();
                }
            }

            void JustSummoned(Creature* summon)
            {
                _summons.Summon(summon);
            }

            void JustDied(Unit* /*killer*/)
            {
                DoAction(APOTHECARY_DIED);
                DoScriptText(SAY_DEATH, me);
            }

        private:
            InstanceScript* _instance;
            SummonList _summons;
            uint8 _deadCount;
            uint8 _phase;
            uint8 _step;
            uint32 _aggroTimer;
            uint32 _stepTimer;
            uint32 _sprayTimer;
            uint32 _chainReactionTimer;
            bool _firstCrazed;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_apothecary_hummelAI(creature);
        }
};

/*######
## apothecary baxter
######*/

class npc_apothecary_baxter : public CreatureScript
{
    public:
        npc_apothecary_baxter() : CreatureScript("npc_apothecary_baxter") { }

        struct npc_apothecary_baxterAI : public ScriptedAI
        {
            npc_apothecary_baxterAI(Creature* creature) : ScriptedAI(creature)
            {
                _instance = creature->GetInstanceScript();
            }

            void Reset()
            {
                me->RestoreFaction();
                _aggroTimer = 5000;
                _waitTimer = 20000;
                _sprayTimer = urand(4000, 7000);
                _chainReactionTimer = urand (10000, 25000);
                _phase = PHASE_NORMAL;

                if (Creature* hummel = ObjectAccessor::GetCreature(*me, _instance ? _instance->GetData64(DATA_HUMMEL) : 0))
                {
                    if (hummel->isAlive())
                        hummel->AI()->EnterEvadeMode();
                    else
                        hummel->Respawn();
                }
            }

            void DoAction(int32 const action)
            {
                switch (action)
                {
                    case START_INTRO:
                    {
                        _phase = PHASE_INTRO;
                        break;
                    }
                    case START_FIGHT:
                    {
                        if (Creature* hummel = ObjectAccessor::GetCreature(*me, _instance ? _instance->GetData64(DATA_HUMMEL) : 0))
                            DoScriptText(SAY_CALL_BAXTER, hummel);

                        _phase = PHASE_NORMAL;
                        me->setFaction(FACTION_HOSTIL);
                        SetInCombat(me);
                        break;
                    }
                }
            }

            void UpdateAI(uint32 const diff)
            {
                if (_phase == PHASE_INTRO)
                {
                    if (_waitTimer <= diff)
                    {
                        DoAction(START_FIGHT);
                    }
                    else
                        _waitTimer -= diff;
                }
                else // NORMAL FASE
                {
                    if (!UpdateVictim())
                        return;

                    if (_aggroTimer <= diff)
                    {
                        SetInCombat(me);
                        _aggroTimer = 5000;
                    }
                    else
                        _aggroTimer -= diff;

                    if (me->HasUnitState(UNIT_STATE_CASTING))
                        return;

                    if (_chainReactionTimer <= diff)
                    {
                        DoCast(me, SPELL_TABLE_APPEAR, true);
                        DoCast(me, SPELL_SUMMON_TABLE, true);
                        DoCast(SPELL_CHAIN_REACTION);
                        _chainReactionTimer = urand(15000, 25000);
                    }
                    else
                        _chainReactionTimer -= diff;

                    if (_sprayTimer <= diff)
                    {
                        DoCastVictim(SPELL_IRRESISTIBLE_COLOGNE_SPRAY);
                        _sprayTimer = urand(8000, 15000);
                    }
                    else
                        _sprayTimer -= diff;

                    DoMeleeAttackIfReady();
                }
            }

            void JustDied(Unit* /*killer*/)
            {
                if (Creature* hummel = ObjectAccessor::GetCreature(*me, _instance ? _instance->GetData64(DATA_HUMMEL) : 0))
                    hummel->AI()->DoAction(APOTHECARY_DIED);
            }

        private:
            InstanceScript* _instance;
            uint32 _chainReactionTimer;
            uint32 _sprayTimer;
            uint32 _aggroTimer;
            uint32 _waitTimer;
            uint8 _phase;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_apothecary_baxterAI(creature);
        }
};

/*######
## apothecary frye
######*/

class npc_apothecary_frye : public CreatureScript
{
    public:
        npc_apothecary_frye() : CreatureScript("npc_apothecary_frye") { }

        struct npc_apothecary_fryeAI : public ScriptedAI
        {
            npc_apothecary_fryeAI(Creature* creature) : ScriptedAI(creature)
            {
                _instance = creature->GetInstanceScript();
            }

            void Reset()
            {
                me->RestoreFaction();
                _aggroTimer = 5000;
                _waitTimer = 28000;
                _throwTimer = urand(2000, 4000);
                _targetSwitchTimer = urand(1000, 2000);
                _phase = PHASE_NORMAL;

                if (Creature* hummel = ObjectAccessor::GetCreature(*me, _instance ? _instance->GetData64(DATA_HUMMEL) : 0))
                {
                    if (hummel->isAlive())
                        hummel->AI()->EnterEvadeMode();
                    else
                        hummel->Respawn();
                }
            }

            void DoAction(int32 const action)
            {
                switch (action)
                {
                    case START_INTRO:
                    {
                        _phase = PHASE_INTRO;
                        break;
                    }
                    case START_FIGHT:
                    {
                        if (Creature* hummel = ObjectAccessor::GetCreature(*me, _instance ? _instance->GetData64(DATA_HUMMEL) : 0))
                            DoScriptText(SAY_CALL_FRYE, hummel);

                        _phase = PHASE_NORMAL;
                        me->setFaction(FACTION_HOSTIL);
                        SetInCombat(me);
                        break;
                    }
                }
            }

            void SummonBunny(Unit* target, bool perfume)
            {
                if (!target)
                    return;

                if (Creature* bunny = me->SummonCreature(NPC_VIAL_BUNNY, *target, TEMPSUMMON_TIMED_DESPAWN, 25*IN_MILLISECONDS))
                {
                    bunny->setFaction(FACTION_HOSTIL);
                    bunny->SetReactState(REACT_PASSIVE);
                    bunny->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NON_ATTACKABLE |UNIT_FLAG_NOT_SELECTABLE);
                    bunny->CastSpell(bunny, perfume ? SPELL_ALLURING_PERFUME_SPILL : SPELL_IRRESISTIBLE_COLOGNE_SPILL, true, NULL, NULL, me->GetGUID());
                }
            }

            void SpellHitTarget(Unit* target, SpellInfo const* spell)
            {
                switch (spell->Id)
                {
                    case SPELL_THROW_PERFUME:
                        SummonBunny(target, true);
                        break;
                    case SPELL_THROW_COLOGNE:
                        SummonBunny(target, false);
                        break;
                }
            }

            void UpdateAI(uint32 const diff)
            {
                if (_phase == PHASE_INTRO)
                {
                    if (_waitTimer <= diff)
                    {
                        DoAction(START_FIGHT);
                    }
                    else
                        _waitTimer -= diff;
                }
                else // NORMAL FASE
                {
                    if (!UpdateVictim())
                        return;

                    if (_throwTimer <= diff)
                    {
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                            DoCast(target, urand(0, 1) ? SPELL_THROW_PERFUME : SPELL_THROW_COLOGNE);
                        _throwTimer = urand(5000, 7500);
                    }
                    else
                        _throwTimer -= diff;

                    if (_targetSwitchTimer <= diff)
                    {
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 1, 100, true))
                        {
                            me->getThreatManager().modifyThreatPercent(me->getVictim(), -100);
                            me->AddThreat(target, 9999999.9f);
                        }
                        _targetSwitchTimer = urand(5000, 10000);
                    }
                    else
                        _targetSwitchTimer -= diff;

                    if (_aggroTimer <= diff)
                    {
                        SetInCombat(me);
                        _aggroTimer = 5000;
                    }
                    else
                        _aggroTimer -= diff;

                    DoMeleeAttackIfReady();
                }
            }

            void JustDied(Unit* /*killer*/)
            {
                if (Creature* hummel = ObjectAccessor::GetCreature(*me, _instance ? _instance->GetData64(DATA_HUMMEL) : 0))
                    hummel->AI()->DoAction(APOTHECARY_DIED);
            }

        private:
            InstanceScript* _instance;
            uint32 _targetSwitchTimer;
            uint32 _throwTimer;
            uint32 _aggroTimer;
            uint32 _waitTimer;
            uint8 _phase;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_apothecary_fryeAI(creature);
        }
};

/*######
## npc_crazed_apothecary
######*/

class npc_crazed_apothecary : public CreatureScript
{
    public:
        npc_crazed_apothecary() : CreatureScript("npc_crazed_apothecary") { }

        struct npc_crazed_apothecaryAI : public ScriptedAI
        {
            npc_crazed_apothecaryAI(Creature* creature) : ScriptedAI(creature) { }

            void MovementInform(uint32 type, uint32 id)
            {
                if (type != POINT_MOTION_TYPE)
                    return;

                DoZoneInCombat(me, 150.0f);

                if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 150.0f, true))
                {
                    _explodeTimer = urand (2500, 5000);
                    me->GetMotionMaster()->MoveFollow(target, 0.0f, float(2*M_PI*rand_norm()));
                }
                else
                    me->DespawnOrUnsummon();
            }

            void UpdateAI(uint32 const diff)
            {
                if (!UpdateVictim())
                    return;

                if (_explodeTimer <= diff)
                {
                    DoCast(me, SPELL_UNSTABLE_REACTION);
                }
                else
                    _explodeTimer -= diff;
            }

        private:
            uint32 _explodeTimer;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_crazed_apothecaryAI(creature);
        }
};

void AddSC_boss_apothecary_trio()
{
    new npc_apothecary_hummel();
    new npc_apothecary_baxter();
    new npc_apothecary_frye();
    new npc_crazed_apothecary();
}