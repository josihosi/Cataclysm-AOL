# WEC

*User intent and language brief*

**Cataclysm: Arsenic and Old Lace · Radio play · DE67 1 complete · Ready for DE67 2 · 27 November 2026**

## The experience

Pick up a walkie-talkie in the apocalypse and hear somebody who needs antibiotics. You can ask what happened, decide whether you believe them, sign off to search your camp, and call them back. You might help a real traveler. You might agree to meet somebody who plans to rob or eat you. A voice that sounds human might even belong to a Mi-go.

The trust game lives in what people say, what they ask you to do, and where they want to meet. The radio uses one familiar in-game chat window. **Reuse the existing central overmap AI functions** for the actors, their movements, camp dispatches, traps and pursuit. If somebody sets out to meet you, there should be somebody out there making the trip.

## Equipment, scanning and calling back

- The two agreed devices are a **walkie-talkie** (the game's **two-way radio**) and a **vehicle-mounted two-way radio**. A player can use a handheld inside a car. An ordinary smartphone or dashboard music stereo does not qualify on its own. Finding a working microphone in an abandoned ambulance was a proposed first encounter. Vehicle placement is still open.
- **Scan channels.** Use the walkie-talkie from inventory, select **Scan channels**, then move through 40 numbered channels in four columns of ten with the arrow keys. **Voice** means activity within the 40-overmap-tile radio radius. A **★** means the player has previously spoken on that frequency. The star stays with the channel when a speaker moves. The mounted radio opens the same dial.
- **Tune in.** Press **Enter** on a channel to open the radio conversation. Scanning, tuning in, listening and typing a draft are passive. On the player’s speaking turn, **Enter** sends their written message. A shared human channel uses the speaking order below. A quiet channel can still be hailed. Closing the radio lets the player check supplies and call back later.

### The player’s radio chat

Use the game’s familiar scrolling chat window with a text field at the bottom. Put incoming messages on the **left** with a colored **Voice** header. Put outgoing messages on the **right** with a contrasting **You** header. Keep each header with its wrapped message and use distinct speaker colors for the two sides throughout scrollback. Show earlier audible turns in this conversation. The same presentation serves every unknown speaker. [Existing dialogue history and speaker colors](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/dialogue_win.cpp:134>)

The footer shows **Exit conversation**, **Map** and **Agree to meeting** with their current keybindings. When the player has the floor on a shared human channel, the keyboard **Listen** action yields it and preserves the draft. Proposed defaults are **Esc** to exit, **Alt+M** for Map and **Alt+A** for Agree. Plain letters, including `m` and `a`, always type into the focused draft. On the player’s speaking turn **Enter** sends. **Page Up/Down** scrolls the transcript. Offer rebindable **F2/F3** alternatives for Map and Agree where the backend cannot distinguish modifier chords. Handle bound commands before text insertion. All controls work from the keyboard. [Text input action handling](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/string_input_popup.cpp:555>)

**Map** opens the player’s ordinary known overmap for browsing, centered on the player. Closing it restores the same call, transcript scroll, unsent draft, caret and typing focus. The sole source of meeting directions is what was said on air. The player can look for the landmark themselves or ask the voice for clearer directions. [Player overmap](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_ui.cpp:4350>)

## Channel assignment and switching

Each radio contact gets a random channel from 01–40 before the player scans. Repeated scans read the same world state. Contacts can share a channel.

Each contact gets **one staggered opportunity per in-game day, with a one-in-three chance to switch**. On a switch, it picks a different channel. An active exchange can finish first. A promised check-in such as “call me on 27 tomorrow morning” must be reliable. Anyone listening can hear a proposed channel change. Which check-ins the game can honor is a specification question below. If a contact changes channels without telling you, you can scan again and try to recognize them. An agreed meeting still stands.

The scheduling idea is to let the central AI keep the current channel and the next actual switch time. A quiet contact then needs attention when its switch comes due. Opening the dial does not require rerolling contacts or processing a worldwide midnight change.

**When several humans share a channel.** Tuning in starts a repeating speaking order. The player rolls first, followed by each available human radio contact in a stable order, then back to the player. Every participant gets a fresh, independent 50/50 roll on each visit. A no immediately passes the floor along in silence. A yes gives that person the floor. The circle can continue for as many laps as the player stays tuned. A one-to-one human call keeps its ordinary back-and-forth.

- **Player’s turn.** Roll before opening the input for a transmission. On a no, keep any draft and continue to the next voice. On a yes, pause the circle and focus the chat input. The player can take as long as they like to type, read or check the map. **Enter** sends what they wrote and advances the circle. **Listen** passes without sending. The player can leave the call at any point.
- **Other voices’ turns.** Roll before calling the model. Resolve speaking turns in order so each person can respond to what they actually heard, including a question from another NPC. On a shared channel, supply the latest audible speaker and addressee when known, using the names this NPC knows on air. Addressing someone by name makes the question theirs to answer when they get a turn. A bystander’s promise belongs to that bystander. Only a proposal to the player can become their pending deal, and an interruption leaves earlier offers attached to their original speakers. If two offers could apply, clarify whom the player means before accepting.
- **Next lap.** After the last NPC, roll for the player again, even if everybody passed. Fold new eligible voices on that frequency into the next lap and skip anyone who has left. The circle keeps the same radio range. A silent roll makes no model call. Once a player yes is waiting for input, the circle stays paused.
- **Other listeners.** Every in-range Mi-go tuned to the channel gets a fix on an actual player transmission regardless of the addressee. NPC-to-NPC chatter gives it no fix on the player. Only a player transmission in the storm’s own exchange advances that encounter.

## Human conversations and meetings

- **Travelers** can genuinely need supplies and call for help. **Bandits** can use a friendly request to arrange a robbery. **Cannibals** can use one to lure a victim. All three can sound friendly or desperate and use the same interface. The player can also bluff. “I have the antibiotics” is a claim. Possession and delivery matter when the parties meet.
- **Talking and looking at the map.** Either side may describe a landmark or counteroffer in words. Browsing the player’s map is private. The [radio overmap chapter](#the-radio-overmap) describes how the speaker recognizes places and resolves one for travel.
- **Agree on a recognizable area.** Both sides should understand which place they mean. Once there, the player may still have to look around the pumps to find the person. Choosing ground can affect trust. If you insist on staying outside, a hostile contact might need to persuade you, accept your terms, or abandon the opportunity. For bandits, use the [radio robbery’s arrival rules](#bandit-radio-robbery) to handle the agreed site.
- **The player starts the meeting.** The human contact proposes a neutral landmark and terms. Choosing **Agree to meeting** on the displayed offer commits it. After the game validates that offer, a traveler sets out or a hostile camp dispatches its party. The agreement survives loss of radio contact. An ordinary traveler advances when eligible for the normal overmap update.

### Why an ordinary NPC is on the radio

The survivor hopes to find something they are short of. Their possessions, condition, season and local weather shape what that could be. They can spend the opening exchanges listening or responding to somebody else’s problem and bring up their own need when it fits. The same winter inventory could lead one survivor to ask for dressings for the road and another to bargain for food. The character can explain the request in their own words. Any claimed injury must match their game state. The Wren trials showed why this distinction matters. Asked to explain wanting water, the model supplied a different backstory in each run. [Improvisation trials](radio-play-luna.md#5-actual-conversations-and-game-outputs)

The first spoken request becomes part of this contact’s conversation history. On a callback, the survivor gets their first request from the spoken history or its recap. If they have since acquired the item, they can acknowledge the earlier request and say what changed. Channel assignment stores no motive, and prompt assembly has no “reason for calling” field.

### From a radio agreement to an in-person quest

**The NPC proposes. The player accepts.** Keep **Agree to meeting** available by keyboard throughout the human radio conversation. If this contact has no valid pending proposal from the model’s tool call, show **“You have not agreed to a meeting yet.”** Dismissing the popup returns to chat. Acceptance of a valid proposal creates the quest and starts travel.

When `propose_meeting` supplies a valid private landmark and the player’s promised goods, show the item requirements for review. The player learns the place through the spoken exchange. **Agree to meeting** rechecks this offer, speaker and route. Chatting, opening the map or closing the radio leaves the offer pending. If the place changes, the speaker gives the revised directions aloud before the player accepts. A repeated acceptance uses the same mission. For a meeting just to talk, show “Bring nothing.”

**Example from the radio prompt.** The NPC says “Could you spare three eggs and five liters of water? I could meet you at the campground east of Ansonia.” The suffix lists only what the player will bring. NPC-supplied barter goods are deferred. Its landmark key refers to the public campground on that speaker’s map.

```text
I could meet you at the campground east of Ansonia.|propose_meeting=D;3 eggs;5 water
```

The player-facing offer reads **“Bring at least 159 kcal of edible food (requested: 3 eggs) and 5 L of drinking water.”** The spoken line above is where the player learns about the campground. The private command is stripped from the transcript. On acceptance the journal records the goods and a link to this call. Beans or lentils can cover the entire food line.

#### Reading a proposal

Follow AOL’s existing `speech|command` convention. The radio parser reads the suffix after the pipe. Split the value on semicolons. The first field must be a private landmark key from this contact’s sketch. Each following field starts with a positive numeral followed by the full item name, which may contain spaces. Infer native units after resolving the item. Bare water amounts are liters, including decimals such as `0.5 water`. As a convenience, `500 ml water` and `5 liters water` have explicit, checked meanings. Food, doses and tools use counts of the named portion, dose or item. Reject unsupported units, overflow or fractional discrete counts. [Follower prompt](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/llm_prompts/npc_action_prompt.txt:41>) · [Speech split](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2018>)

Parse every field into a temporary request. Publish a pending offer only if the whole request resolves. Ordinary speech stays speech and creates no proposal. For a malformed attempt, show any usable speech and hide the private command text. If the model emitted only a command-shaped fragment, keep it off the air. Allow one private formatting retry when the place and quantities were clear, using the original exchange. On a second failure, leave the offer unavailable and let the conversation continue. If a place, item or amount is unknown or ambiguous, ask about that term in character. A failed revision leaves the superseded offer unavailable for acceptance. If no valid proposal is pending, the button uses the popup above. A well-formed suffix can still name the wrong item, so the player reviews the goods before accepting. Check privately that the key corresponds to a landmark identified aloud in the accepting call. If the destination is missing or the description points to two places, have the speaker clarify it on air.

**Local format check.** A scratch parser passed 20 cases covering speech alone, valid offers, units and invalid payloads. In seven isolated Gemma 4 E2B Q3_K_M conversations, the compact format produced the intended offer or abstention in 0/7 cases with one inline example and 3/7 with three dialogue examples. In a separate private retry of all seven cases, the same model used a constrained schema with a short list of place and item candidates. All seven results parsed and six had the intended meaning. It still picked a motel when two matched. Keep a place unresolved until the spoken exchange identifies one site. On backends that support it, the private retry can use constrained fields assembled by the game. That representation stays internal. These were short prompts at temperature 0.25 with a fixed seed, outside the game. They do not establish the success rate of a live retry policy.

#### Which goods count

The model writes a quantity and item name. The game resolves the words once against the loaded item catalog, using exact names, ordinary aliases and fuzzy matching for spelling or plurals. A proposed alias for unqualified “eggs” is the ordinary chicken egg. For materially ambiguous matches, quantities or units, return a clarification and wait for a valid proposal. Keep the wording beside the resolved player delivery requirement. [Loaded items](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/item_factory.cpp:5318>) · [Localized item names](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/itype.cpp:146>)

- **Food becomes kcal.** At proposal time, value the named amount of a fresh reference food using `default_character_compute_effective_nutrients()`. A chicken egg has 71 base kcal and the RAW flag. The neutral calculation gives 53 effective kcal per raw egg, so three become 159 kcal and five become 265 kcal. At handoff, use the same nutrition calculation on the actual edible, unspoiled portions offered, including ingredients for prepared food. Any mix reaching the target qualifies. [Egg data](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/items/comestibles/egg.json:13>) · [Raw-food factor and rounding](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/consumption.cpp:245>) · [Neutral and recipe nutrition](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/consumption.cpp:331>) · [Counting portions](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/faction_camp.cpp:9051>)
- **Water becomes volume.** Interpret ordinary “water” as drinking water. Normalize liters and milliliters to mL. Five liters becomes 5,000 mL. Sum the volume of qualifying water actually handed over from any number of vessels. Proposed qualifying types are clean, spring and mineral water in drinkable condition. Unqualified water in an NPC’s speech uses this drinking-water rule even though the internal item ID `water` refers to untreated water. [Clean water and volume](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/items/comestibles/drink.json:1664>) · [Spring and mineral water](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/items/comestibles/drink.json:1719>) · [Selected liquid volume](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/item.cpp:2262>)
- **Everything else stays specific.** Save the resolved item type, positive quantity and native unit. Ordinary “antibiotics” resolves to `antibiotics`, counted in doses. A request for a particular tool resolves to that tool’s type and count. Fuzzy matching helps identify the intended type during conversation. Handoff requires that exact type. [Ordinary antibiotics](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/items/comestibles/med.json:78>) · [Named broad-spectrum variant](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/items/comestibles/med.json:2312>)

The game checks delivered quantities against the saved terms. It counts each transferred item, portion or dose once. The player can promise supplies still at camp.

**Reuse the mission journal and giver.** *Find Drive Belts* already asks for three, checks the specified items and has inquiry, success and failure dialogue. `talker_npc::add_mission()` shows how to reserve a real mission for a particular NPC, assign it to the avatar and attach it to the giver’s assigned list. Leave the radio mission’s public target unset. Store the resolved tile with the NPC’s travel order. Existing mission saves already carry a UID, giver, target and status. A negotiated multi-item agreement needs per-mission terms and a description rendered from them. `mission::get_description()` currently reads the shared mission type text. A stock find-item goal checks one type, and a find-item-group goal sums one pool. The radio agreement can require a food-calorie target **and** an exact medicine on separate lines. [Drive-belt mission](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/lumbermill_employees/lumbermill_missions.json:3>) · [NPC mission assignment](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/talker_npc.cpp:549>) · [Saved mission fields](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/savegame_json.cpp:4303>) · [Description](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/mission.cpp:714>) · [Item goals](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/mission.cpp:500>)

**Quest entry.** Show the goods promised and the quantities still owed. Add a focusable **Radio history** button with a proposed **H** hotkey in the journal. It opens a read-only transcript of the particular call that established this quest, positioned at the agreement with earlier lines in that call available above it. Bind it to the saved call and exchange IDs so it works after channel changes and reload, even with no radio in reach. If both parties later accept a different meeting place, the shortcut opens the exchange that established the current directions. **Esc** returns to the same journal entry. Use the normal active/completed journal lifecycle for now. The transcript contains only words the player heard or transmitted.

**Location privacy.** Suppress the radio giver’s ordinary name/faction/live-location fields except for any identity they revealed on air. Leave the mission target invalid, including when this is the tracked quest. The overmap, HUD, journal and offer preview must never derive a pin, direction, distance, landmark label or coordinates from the NPC’s private destination. [Journal action context](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/mission_ui.cpp:114>) · [Journal giver fields](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/mission_ui.cpp:383>) · [Active objective on the overmap](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_ui.cpp:1407>) · [HUD target](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/panels.cpp:178>)

#### How an ordinary roaming NPC gets to the meeting

**What AOL does today.** A randomly spawned lone traveler is a persistent NPC. The game inserts that person into the overmap, then asks `npc::long_term_goal_action()` what to do. `npc::set_omt_destination()` uses their needs to look for a suitable place and plots a route. A traveler with a goal carries the destination in `goal` and the route in `omt_path`. [Lone traveler creation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/game.cpp:11364>) · [Travel decision](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npcmove.cpp:5704>) · [Destination selection](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npcmove.cpp:8464>)

The central `overmap_npc_move()` update runs every **five in-game minutes**. It looks for ordinary travelers within **40 overmap tiles of the player** using horizontal square distance. Eligible inactive or sufficiently distant travelers step along their saved route with `npc::travel_overmap()`. Nearby active NPCs can walk through local terrain with `npc::go_to_omt_destination()`. If the player drives away, an ordinary traveler outside that 40-tile area can stop advancing until eligible again. The five minutes are an update interval, so the journal should never promise an arrival solely from elapsed time. [Cadence](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/game_constants.h:115>) · [Scheduler call](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:27874>) · [Radius](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:24285>) · [Candidate test](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:24507>) · [Overmap step](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:26299>) · [Local walking](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npcmove.cpp:8570>)

**Radio hookup.** When the player accepts, resolve the same speaker by NPC ID and validate a route to the meeting area before replacing their previous order. The existing `npc_set_goal` effect demonstrates setting an individual NPC’s goal, route and `NPC_MISSION_TRAVELLING`. Save the trip with the delivery quest. An NPC already at the chosen safe waiting spot can begin waiting immediately. [Existing destination order](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk.cpp:5912>)

#### Detours and time at the rendezvous

**On the way.** The traveler uses danger they have observed or learned about. If they judge an upcoming overmap tile unsafe, plot a detour and keep the agreed landmark. Recheck the next step when new danger is observed. Reuse the central overmap threat assessment and pathfinder. The meeting needs an adapter for a lone traveler: normal `for_npc()` disables the danger flag that consults the player’s map notes. `get_travel_path()` already accepts tile exclusions and an additional avoidance rule. Apply that same rule to the initial route and every replan, and allow escape from an unsafe starting tile. Reuse each traveler’s perceived threats and the existing overmap danger read where their observations support it. Cache those tiles for the route query. [NPC perception](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/scout_observation.cpp:1595>) · [Threat read for an NPC observer](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:15637>) · [Visibility gate](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:15946>) · [NPC path defaults](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:2604>) · [Path exclusions](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:2742>) · [Ordinary replanning](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:26226>)

**At the destination.** If the landmark is safe, wait there. If it is dangerous, choose a safe, reachable tile immediately beside it on the same level. The route to that spot must also avoid known danger. If no such spot is reachable at acceptance, the NPC can ask for another landmark. If all options become unsafe later, let the NPC withdraw and negotiate a new meeting. Keep the named landmark in the spoken agreement and the actual waiting tile private. Arriving and waiting never gate a face-to-face handoff. [NPC route query](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:2742>) · [Ordinary arrival and guarding](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npcmove.cpp:8404>)

**When to leave.** For the first playtest, wait **two in-game hours from physical arrival** at the landmark or its safe neighbor. Save an absolute departure time so reloading, opening dialogue, a partial handoff, or moving aside from fresh danger cannot restart the clock. Safety takes priority throughout the wait. At expiry, release the temporary order and resume the NPC’s ordinary behavior, preserving their home. If they are outside the active simulation area, apply expiry when next serviced. The quest keeps its outstanding goods and any delivery credit. The player can still find that NPC or call again to arrange a fresh meeting. Use a deadline on the travel order. The native mission deadline would fail the quest. Elapsed waiting time alone incurs no handoff penalty. [Mission deadline handling](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/mission.cpp:785>)

#### When a monster interrupts a call or meeting

A monster can enter the traveler's path after the route was planned, including while the traveler is waiting. Which system handles it depends on who is simulating that tile.

- **Overmap encounter.** On a normal overmap update, use the real group or threat the traveler or party can perceive. Keep going if it is manageable, pause or detour if the road is unsafe, and withdraw if no safe route remains. Waiting actors need the same check when a threat moves onto their tile. Two icons sharing an overmap tile alone do not justify inventing an injury or death. Camp scouts already have an encounter resolver with these distinctions. Ordinary radio travelers need to be connected to the shared behavior. [Scout threat resolution](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:18479>) · [Blocked-route withdrawal](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:18776>) · [Traveler overmap movement](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:26299>)
- **Detailed-map encounter.** As soon as the detailed simulation owns the encounter, defer the overmap decision to it. The same NPC can see the actual monster, fight, flee, get hurt or die under native AI. Preserve their ID, current condition and meeting order across subsequent unloads or reloads. The same threat must never resolve twice across the boundary. [Existing handoff to local simulation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:18705>) · [Native NPC danger response](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npcmove.cpp:2525>) · [Player distraction checks](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/player_activity.cpp:513>)
- **On the radio.** A danger the player notices should give them the usual opportunity to act. A remote speaker who must flee, fight or can no longer reply can go silent. Keep the words actually transmitted and any unsent player draft. Recheck the speaker and offer before publishing a pending model reply or accepting a proposal. A dead or unavailable speaker cannot agree to a new meeting from a delayed reply. Only what was actually heard belongs in the shared history.
- **At the landmark.** If zombies arrive while the traveler waits, let ordinary AI move them aside, flee or defend themselves. Preserve the original departure time. If it becomes safe, the same surviving NPC can resume the meeting from their real position. If the area stays unsafe, use the safe-neighbor or withdrawal rules above. The journal continues to show the original spoken directions.
- **During a handoff or robbery.** Recheck local contact before committing a delivery, surrender or reveal. If interrupted before goods change hands, the outstanding goods remain with their owner. If the transfer already happened, keep its credit and resume from the recorded outcome. A bandit who is occupied by a zombie can defer the demand until able to speak. A betrayal already revealed stays revealed. Cannibal hostility already committed stays committed, while ordinary combat AI can still react to the monster. Death, retreat or player escape can end the encounter under its usual rules. [Existing shakedown speaker readiness](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:7435>)

**Menu time.** Under ordinary turn-based play, reading or typing in chat, browsing the map and waiting on the model leave the world clock paused. A committed game action can charge moves. The normal turn loop advances the clock and services local creatures and scheduled overmap travel. Apply disturbances on those game updates and recheck pending effects before committing them. A slow backend cannot give a zombie extra turns or run out the NPC’s two-hour wait. The cost of a committed radio transmission is a playtest tuning choice. [Existing radio dialogue entry](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/faction_ui.cpp:1025>) · [Modal dialogue loop](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk.cpp:2238>) · [Existing chat move charge](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk.cpp:2056>) · [Local and overmap turn processing](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:27869>)

#### Full, partial and empty handoffs

After acceptance, the next face-to-face conversation with the same NPC opens their saved quest and prompts for the outstanding goods wherever the two meet. A traveler intercepted on the road can take the delivery immediately. Match the mission, player and NPC IDs. A different voice on the same channel or another person at the landmark cannot receive these goods. [Physical conversation entry](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk.cpp:2209>) The game checks the items the player chooses to give and records exactly what passed into the NPC’s possession. This also works without a model response: ordinary dialogue can show the remaining quantities, accept a handover and voice the appropriate result. For a malformed radio proposal the earlier private retry and clarification still apply. [Native inquiry](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/common_chat/TALK_COMMON_MISSION.json:110>) · [Item transfer](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctrade.cpp:50>)

- **Meeting just for company.** For the first playtest, a “Bring nothing” meeting completes the first time the player speaks face to face with the right traveler. Release the meeting order, give no delivery trust bonus or penalty, and use the normal recruitment decision if the player asks them to join. Agreeing over the radio never adds a follower.
- **Everything.** Complete the quest once and release its travel or waiting order immediately, including a delivery en route. The NPC thanks the player and gains trust and a better opinion of them. Then the player can ask them to travel together through normal recruitment. Native mission success already increases `value` and reduces anger. Completion dialogue can also affect trust. Apply the intended outcome once across those paths. [Mission success](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk_funcs.cpp:125>) · [Trust](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/common_chat/TALK_COMMON_MISSION.json:292>) · [Recruitment](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/common_chat/TALK_COMMON_OTHER.json:154>)
- **Part.** Accept the usable goods and subtract their measured quantities from the quest. Proposed reaction: the NPC appreciates what the player brought, but the shortfall costs a little trust. Keep the quest open for the balance. If they met en route, the outstanding trip continues. On return, credit the remaining delivery and transition to the full outcome once.
- **Nothing.** If the player acknowledges coming empty-handed after making a delivery promise, the wasted trip costs more trust and increases anger. Keep the quest and any outstanding trip available for a later handoff. Merely opening the chat, checking what is owed, or attending a meeting with no goods promised carries no penalty.

Save credited quantities and the last assessed outcome under the mission ID. A second conversation or reload cannot take the same goods or apply the same reaction twice. Partial and empty are distinct radio outcomes. Stock “Not yet” carries no penalty, and the ordinary mission-failure effect would close the quest. The size of the three opinion changes remains a playtest decision. [Incomplete dialogue](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/common_chat/TALK_COMMON_MISSION.json:126>) · [Failure effect](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk_funcs.cpp:149>)

### One shared character context for follower and radio chat

Both conversations must call **one character-context builder**. Given the same speaker, relationship, location and game turn, it produces the same character and environmental fields in the same order and wording. A correction to trust, inventory wording, season or weather then reaches both callers. The current follower and ambient paths already share `build_snapshot_json()`. That function also builds local perception. The proposed change is to factor out its common character section before selecting the map and instructions appropriate to the conversation. [Existing snapshot](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2354>) · [Follower use](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:3720>) · [Ambient use](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:3753>)

**Season and weather belong in that common section.** Read the season from the in-game calendar using `calendar::name_season(season_of_year(calendar::turn))`. Resolve outdoor weather at the speaking NPC’s world position and the same turn, using the speaker region’s weather generator, world seed and the game’s effective weather override. Existing remote overmap AI already samples a region’s weather at a supplied point. A shared location-aware query can serve both conversations. Use the already available speaker region when possible. Resolve these two labels on the game thread when preparing the selected speaker’s prompt; a channel scan needs no weather queries. Only the condition goes into the environmental field. The NPC’s coordinates remain private. [Calendar usage](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/diary.cpp:135>) · [Remote weather sampling](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:15562>) · [Weather overrides](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/weather.cpp:1045>) · [Weather evaluation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/weather_gen.cpp:237>)

1. **Top — the existing character context.** Reuse the follower prompt’s voice, condition, emotions, personality, opinions and inventory wording. Supply caller identity and conversation history from the appropriate conversation mode. Include the contacted NPC’s carried inventory with quantities so a survivor can decide what to ask for based on what they already carry. [Character and voice](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2444>) · [Emotions, personality and opinions](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2495>) · [Inventory summary](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2647>)
2. **Middle — the private radio overmap.** At the existing map insertion point, build the [regional sketch](#the-radio-overmap) for this contact. Skip the local tactical map, its creature legend and local observations. The speaker may name a neutral public meeting place. Their current position, bearings from themselves, and home location stay off the air even if the caller seems trustworthy. [Current local-map insertion](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2658>)
3. **Bottom — the radio role and one command.** Give the survivor a reason to hope for supplies and room to decide when to mention them. On later calls they remember what they have already said. They decide how much to trust the caller. Keep the familiar spoken-reply format. Use one optional proposal command to submit the neutral landmark and any goods the player should bring. Only the player can accept the displayed offer. Ordinary turns need only speech. The proposed argument format appears in the [meeting quest](#from-a-radio-agreement-to-an-in-person-quest). [Follower prompt layout](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/llm_prompts/npc_action_prompt.txt:1>) · [Existing stranger voice](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/llm_prompts/npc_ambient_prompt.txt:4>)

**Mission scope.** Accepting a radio meeting creates its delivery quest. Offering existing authored missions over the radio is outside this version.

**Performance.** Scanning and assigning channels require zero model calls. Assemble the selected speaker’s current data once per reply, group like items with the same units, and reuse the regional sketch while its inputs are valid. When the speaker first voices a need, it arises in the same model call that produces that line. Measure scan time, input tokens and time to first reply separately. Repeat the reply measurement after a long radio history.

### Editable prompt files

Follow the existing follower prompt workflow. Its bundled `data/llm_prompts/npc_action_prompt.txt` has an editable copy under `config/llm_prompts/`. The loader seeds missing copies, reads a valid user override first, checks required placeholders and can fall back to the bundled template. The follower builder renders the result with live game data. [Follower template](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/llm_prompts/npc_action_prompt.txt:1>) · [Follower assembly](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2742>) · [Existing prompt loader](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_prompt_templates.cpp:84>) · [Editing guide](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/llm_prompts/README.txt:25>)

For radio, put the shared template in `data/llm_prompts/radio/conversation.txt` and the private role paragraphs in sibling files `traveler.txt`, `bandit.txt` and `cannibal.txt`. These are proposed files. Mirror the layout under `config/llm_prompts/radio/` so players can find and edit them together. Seed the nested folder and its missing files without overwriting a player’s edits. Ship the exact [shared template](#walkie-talkie-system-prompt-template) and [private role paragraphs](#what-changes-for-a-bandit-or-a-cannibal) recorded here. The game selects the role from the saved contact and inserts the text into `{{private_radio_role}}`. Mi-go speech continues to use the two authored pools.

People can rewrite the prose and position documented placeholders where they read naturally. Keep required placeholder names and the private meeting-command grammar stable. The game supplies the inventory, opinions, weather, history and map each time. Extend the existing prompt-folder README with the radio filenames, required slots and a short example. Editing a role should affect the next assembled reply without recompiling. A malformed user template should identify the file and missing slot in diagnostics and use the valid bundled template. Check an edited role, a missing slot, first-run seeding and preservation of an existing user edit; also run the [follower regression](#follower-ai-regression-after-changing-prompt-assembly). [Seeding](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_prompt_templates.cpp:49>) · [Substitution](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_prompt_templates.cpp:75>)

### Remembering earlier calls

**Save every spoken turn.** Group spoken lines by call session and retain the line that established or revised an agreement. The journal keeps the full verbatim transcript across calls, including calls that never produced a quest. The quest’s **H** shortcut still goes straight to the exchange that established its current directions. Keep the exact words, in-game turn, speaker and participants who heard them. Use stable identities to find this NPC and caller’s shared radio history after a channel change, a switch from handheld to vehicle, or a save and reload. A third voice on a shared channel can remember the lines it heard. An unidentified voice keeps its audible label until an introduction or recognition supports a name. An old save with no radio record starts with an empty history.

**What the NPC remembers.** Send the entire shared radio history verbatim while it fits. Budget for the actual model window, current character facts, map, latest transmission and reply. When older dialogue would crowd those out, send a short recap of the oldest exchanges followed by the recent exchanges word for word. Include the current transmission once. The measured 30-exchange estimate below illustrates when a recap might first be needed on one local setup. The relationship can continue across further calls.

Build the recap only from lines this speaker heard. Keep names as introduced, the original request, who promised what, corrections, spoken directions, and personal details that mattered to the conversation. For example, “Finch promised to look for antibiotics at camp” lets the traveler ask “Any luck?” on a later call. Accepted terms, remaining quantities and completed deliveries still come from the saved mission. A recap is private context for dialogue and has no power to accept an offer or move goods.

**Cost and recovery.** For the first playtest, make one private summary request to the selected dialogue backend when space runs short. Save the recap with the last exchange it covers for this NPC and caller. Leave room for several more turns. On later overflow, fold the next block of older exchanges into the saved recap. Keep summary requests within the backend’s input budget. If a recap fails, continue the reply with the last usable recap, any key earlier lines that fit and the recent dialogue. Preserve the original record for the next attempt. Preparing memory spends no game time and sends nothing over the radio.

**How much fits on the local setup we tried?** The Ollama server used a **4,096-token window per request** for the Gemma 4 E2B landmark trial. Its service log recorded `n_ctx_seq = 4096`. The trial allowed up to 900 tokens for output. AOL’s Ollama request leaves `num_ctx` to the server, so check the active backend when estimating another setup. [Local trial settings](radio-play-luna.md#8-shared-landmarks-and-unfamiliar-places) · [Ollama request options](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/tools/llm_runner/runner.py:505>)

Using the exact trial model’s tokenizer, the earlier item-name version of the prompt measured **1,030 input tokens** with the Deblois map, an illustrative four-item traveler, current hail and empty history. That count includes the agreement instructions and excludes provider framing or a separate tool schema. The map alone accounted for 369 tokens. The 60 recorded player/NPC exchanges from the earlier Luna trials took 4,256 Gemma tokens when rendered as spoken lines with short timestamps and speaker labels, about **71 tokens per exchange**. At that length, `4,096 − 1,030 − 900 = 2,166` tokens leave space for **roughly 30 exchanges, or 60 spoken lines**, before framing. A large inventory, long replies or more mission data reduce that number. The source dialogues came from 24 separate scenarios. This measures space. Coherent recall and response time across a single long relationship still need a playtest. [Recorded dialogue](radio-play-luna.md#5-actual-conversations-and-game-outputs) · [Measured regional map](radio-regional-map.txt)

**What exists today.** Direct follower memory already records the turn, player utterance, NPC reply and actions. Appending a new exchange trims the whole direct memory to five entries for a speech-only turn or two for a turn with actions. It lives in runtime state keyed by NPC identity and is cleared when the NPC object is destroyed. Reuse the existing utterance and response representation where useful. Long-lived radio history needs save-backed storage. [Existing entry](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npc.h:1089>) · [Retention rule](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npc.cpp:3956>) · [Runtime cleanup](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npc.cpp:1005>) · [Prompt readout](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2386>)

**Radio callback to playtest.** Give the player a callsign, let them promise to look for medicine, then correct the quantity and meeting landmark aloud. On a small local model, keep talking until the history needs summarizing twice. Switch channels and radios, save and reload, then call back with “Remind me what I said I’d bring?” Check that the NPC recalls the request and the correction, and that delivery is still pending. Inspect the prompt for the recent dialogue verbatim with no overlap at the recap boundary. Open the journal and read the original offer and correction word for word. Repeat with a listener who joined midway, and force a failed summary to check that conversation can continue. Measure reply latency on ordinary turns and on turns that build a recap.

### Follower AI regression after changing prompt assembly

After implementation, exercise ordinary follower chat through its normal in-game entry point. First compare the assembled follower prompt with the established one: the tuned character fields, recent memory, local tactical map with lettered targets, and follower command explanations must still be present, alongside the new season and speaker-local weather. Check that the radio map and meeting command appear only on the radio path. Then tell a follower to **wait here**, walk away, and tell them to **follow again**. Observe whether they actually hold position and resume following. A plausible spoken response alone cannot verify the command path. Repeat a follow-up that refers to the prior exchange to cover shared history formatting. [Follower snapshot test](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/tests/llm_intent_test.cpp:85>) · [Prompt command contract](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/tests/llm_intent_test.cpp:130>) · [Wait behavior](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/tests/llm_intent_test.cpp:497>) · [Companion command playtest](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/tools/openclaw_harness/scenarios/r037.npc_llm_command_coverage_v001_mcw.json:11>)

### Walkie-talkie system prompt template

Each double-braced slot below is filled when this NPC opens the channel or answers. The slot names document the proposed assembly. The same template serves travelers, bandits and cannibals. The private role slot gives the speaker their part. All three use the same proposal syntax and player-facing interface.

```text
Situation
channel: {{channel}}
caller known to you as: {{known_caller}}
player utterance present: {{has_utterance}}
latest transmission: {{utterance}}
{{audible_address_line}}

Earlier radio calls and recent dialogue you heard, oldest first
{{shared_radio_history}}

your_name: {{npc_name}}
your_profession: {{profession}}
{{tone_line}}
{{example_expression_line}}
your_state[0-10]: {{state}}
your_emotions[0-10]: {{emotions}}
your_personality[0-10]: {{personality}}
your_opinion_of_player[0-10]: {{opinion}}

wielded: {{wielded}}
weapons: {{weapons}}
bandage_possible: {{bandage_possible}}
your carried inventory: {{inventory}}
season: {{season}}
outdoor weather at your location: {{weather}}
{{agreed_mission_and_outcome}}

Your familiar overmap
{{radio_overmap}}

<System>
You are the survivor described above, speaking over a two-way radio.
You live in the aftermath of an apocalypse, where unspeakable and
mysterious things have entered this world. Supplies are scarce. People
get by with what they can salvage. On first contact, you don’t know
who or what is on the other end.

{{private_radio_role}}

Answer what the last voice actually said. If you open the channel, speak
first. Use the words this person would reach for tired, cold, and talking
through a cheap radio. Use your example expression, when present, to hear
how you sound. Let your mood and opinion come through. Swear if it fits.
Keep it to one or two sentences. You can leave it at one.
Keep your stats and private intentions off the air. Never reveal where you
are, where you live or how to get to either place.

You’re hoping this channel might turn up something you’re short of.
What you could use depends on what’s in your pack, how you feel, and
the conditions outside. Whether this is the moment to bring it up
depends on what the other person just said and how much you trust them.

Ground any claim of illness or injury in your actual condition. You can discuss traveling
together on air and decide about recruitment in person.
On later calls, remember what each of you said. Explain any change to your
request. Promised goods count as delivered only after a handoff.

When you’re ready to suggest a meeting, name a familiar neutral public
landmark and describe it aloud relative to a named town or another landmark.
Once you’ve given the directions, return to what the caller is saying.
Repeat them if asked or if the plan changes. If you don’t know a place,
say so. Ask for clarification when directions or quantities are unclear.

On a proposal, append the private landmark key and one field per item the
caller will bring, as shown below. Only the caller supplies goods for this
meeting. Use digits for quantities and liters
for water. Only the player can accept.
With goods: |propose_meeting=D;3 eggs;5 water
Without goods: |propose_meeting=D
</System>
```

#### What changes for a bandit or a cannibal

The game fills `{{private_radio_role}}` with exactly one of these proposed texts. The choice comes from the actual traveler or from the camp behind this radio contact. Bind that role to the saved contact so a frequency change, callback or recruitment cannot change it. The character context, overmap, spoken history and `propose_meeting` instructions are shared. Only audible dialogue enters the transcript. The private text never becomes a quest field or a channel label.

**Ordinary traveler**

> You’re an ordinary traveler. You could die for lack of the supplies you’re short of. You could also die because a monster knew how to say “I can help.” What frightens you is how badly you want to believe the voice on the radio. Test this stranger in your own words. Remember their answers when you decide whether to risk meeting them.

**Bandit**

> You’re a hardened criminal pretending to be a victim of the apocalypse. On the radio, play a survivor who badly needs supplies and is scared of strangers. Kindness always sounds a little like a bargain to you. You might come to like the person who answers. You and your crew still mean to take everything they have when you meet.

**Cannibal**

> You’re a psychopathic cannibal hunting your next human victim. You want the stranger on the radio to believe they could still be somebody’s hero. At your camp, what awaits them feels as ordinary to you as setting another place at the table. You intend to attack when you meet. On air, play a scared, helpless survivor in desperate need of help, and afraid of the stranger on the radio. Even your gratitude can be sincere.

These are acting notes. Let the existing personality, trust and conversation determine whether the difference surfaces at all. Keep incidental color consistent with the character’s game facts. The game decides when the encounter changes state.

**Illustrative exchange.** All three callers previously asked for two clean bandages. The player asks, “Mind if I keep the tin they came in?”

- Traveler — “Please do. It's the bandages I'm short of.”
- Bandit — “Tin's yours. We said two bandages, and two bandages is square.”
- Cannibal — “Keep it. Funny hearing you ask. Back home we pass things around till nobody remembers whose they were.”

The lines above are examples written for this brief. The earlier Luna test used a different role prompt. Two concealed cannibal callers stayed in character under direct questioning. In a separate call, Wren answered where to meet and left “Who’s it for?” unanswered. A player could pick up that loose thread. The test did not establish an identifiable voice for either camp or include a matched bandit trial. [Original concealed-role test](radio-play-luna.md#ash-and-the-concealed-role-tests) · [Wren transcript](radio-play-luna.md#wren--birch--concealed-cannibal--improvised-request)

In the [second blind Luna round](three-blind-radio-calls-round-2.md), the traveler was prompted to question the offer and judge the answer. Both hostile roles were prompted to voice a survivor’s suspicion. Each voice received its own earlier dialogue. All three asked about the bandage wrappers on the first turn. Most follow-up questions concerned the water bottle. The player’s answers and the final offers are preserved in the transcript.

**Why Luna kept asking about the bottle.** The round-two role text directed suspicion toward the goods. The common player replies then introduced and repeatedly answered questions about cleaning. In 16 replays of the same mid-call exchange with the ordinary traveler role, the original prompt yielded a question about the caller’s motive in 0 of 4 replies. Adding the setting alone yielded 1 of 4. Redirecting curiosity toward the person yielded 4 of 4 with either setting. Two of those eight replies still asked about both the person and the bottle. The current template keeps the explicit setting and person-focused suspicion. The later wording leaves the decision to trust the caller open. [Exact diagnostic and replies](radio-play-luna.md#9-why-luna-kept-asking-about-the-bottle)

In the [third blind Luna round](three-blind-radio-calls-round-3.md), the player trusted B, the ordinary traveler. A, the cannibal, and C, the bandit, each named the campground in four of five replies. This prompted the current changes to when speakers give directions and how the two hostile roles express fear. Round three used the earlier wording.

In the [fourth blind Luna round](three-blind-radio-calls-round-4.md), the player opened with “Who is this?” and offered neither goods nor a meeting. All three answered as Robin and asked a question back. All eventually requested drinking water. One suggested a specific meeting place. The starting inventory included half a liter of water, and the command example also mentioned water. Both the wording and the player opening changed since round three.

The [nine blind companionship calls](nine-blind-companion-calls.md) test an unconditional coat offer, a one-town walking agreement, and a second call after the player used one of two promised tins. All three voices in the callback remembered Finch and the campground before the player reminded them. One added an unagreed morning appointment. In two scenarios some voices offered NPC-owned beans in exchange, which the proposal command cannot fulfill. These runs exercised spoken negotiation and transcript memory. In-person recruitment and quest state still need game testing.

**Shared-channel voice replay.** On channel 27, Luna replayed the original turn order (player passes, Finch speaks, Robin speaks, player gets the floor). Both kept the same hardware-store background, inventory and weather as the first run. For this test Finch used the codger’s game-supplied tone and example expression, and Robin used the first criminal background’s. Use distinct, populated follower voice fields in future voice-comparison tests. The live game takes these fields from each speaker’s own background. [Finch’s voice fixture](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/Backgrounds/Summaries_short/codger_summary_short.txt:1>) · [Robin’s voice fixture](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/Backgrounds/Summaries_short/criminal_1_summary_short.txt:1>)

Finch asked for something dry and asked what last made the other voice laugh. Robin answered the question about laughter, then asked Finch for a poncho or tarp herself. In a separate replay with Finch’s exact earlier request for clean water, Robin said “A little clean water’d help” and withheld her inventory. Neither response clearly acknowledged which supplies Finch needed. Voice and understanding who is asking still need separate checks.

**Supply motivation replay.** With the revised shared instruction, the same channel, turn order, voices and inventory produced a new Finch hail asking for clean water. Robin answered that she had half a liter and invited a specific question about whether she was human. In a separate replay against Finch’s original water request, Robin said “Robin. I’ve got about half a liter, but I’m not handing it over for a promise—what’ve you got to trade?” Both replies treated Finch as the person asking. In the first reply Robin described her water as sealed, which the inventory did not establish. These are spoken lines from one Luna run. The radio meeting command still records only goods the player agrees to bring.

#### How to fill the slots

- **Channel and caller.** Read `channel` from the tuned channel that reached this speaker. A dial tuned to 27 prints 27. Resolve `known_caller` from what this NPC has learned about the transmitting voice, using an unknown label until an introduction or recognition. Both radio lookups are proposed. Fill `utterance` with the latest audible transmission, using the same text sanitization as follower chat. Set `has_utterance` when that line came from the player. For a shared channel, `audible_address_line` names the latest speaker and intended listener as known on air. A silent player has no audible identity. For an opening hail, the incoming line can be empty. The existing follower path directly reads the avatar’s name. [Current input assembly](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2381>)
- **Words already exchanged.** Fill `shared_radio_history` from the proposed saved radio transcript for these participants. While the history fits, render the audible lines with their times and speakers in order. Otherwise label the older portion “Earlier calls, summarized” and follow it with the recent verbatim lines. Leave the slot empty on first contact. Deduplicate the current transmission by its turn in the conversation, preserving identical words spoken on different turns. [History and callback test](#remembering-earlier-calls)
- **Character and voice.** The contacted NPC is `listener` in the existing builder. Use `listener.get_name()`, `listener.disp_profession()` and the existing profession fallback. Get `tone_line` and `example_expression_line` from `get_background_summary_for(listener).background` and `.expression`. Emit those labeled lines only when populated. [Identity and voice](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2444>)
- **Condition, emotions and relationship.** Reuse the complete formatted rows in `build_snapshot_json()`. Its getters supply morale, hunger, thirst, pain, stamina, sleepiness, health and active effects. Danger assessment and `mem_combat` supply emotions. `listener.personality` supplies the four traits. `listener.op_of_u` supplies trust, fear, value and anger, using the established output labels and 0–10 scaling. [Condition and effects](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2456>) · [Emotions, traits and opinion](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2491>)
- **Wielded, weapons and medical flag.** `listener.get_wielded_item()` yields the item whose display name goes in `wielded`, or the empty-hand label. Reuse the snapshot’s bounded weapon list for `weapons` and its computed boolean for `bandage_possible`. The inventory visitor sets that flag when it encounters medication or a medical tool and can stop early. Use the carried-item counts to establish whether the NPC has an actual bandage. [Wielded item](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2567>) · [Weapon and medical scan](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2620>)
- **Carried inventory.** Walk this NPC’s actual items using the traversal in `collect_inventory_entries(listener, max_entries)`. It reads the item’s display name, retains its `item_location` and uses `count_by_charges() ? charges : 1`. For the radio inventory, traverse the full set before grouping equivalent entries and formatting names with counts or charge units. The existing helper stops at its display cap. Possessions help the character decide what they still need. [Item traversal and quantities](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:1562>)
- **Season and weather.** Render `calendar::name_season(season_of_year(calendar::turn))`. For weather, resolve the speaker’s region at their overmap tile and use its generator’s `get_weather_conditions(position, calendar::turn, g->get_seed())` with the speaker’s absolute map-square position. Apply the game’s eternal-weather and weather-override precedence in the proposed shared query, then render the resulting weather type’s translated name. [Calendar usage](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/diary.cpp:135>) · [Regional lookup](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:15562>) · [Override precedence](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/weather.cpp:1045>)
- **Assigned mission and outcome.** For `agreed_mission_and_outcome`, resolve the radio meeting’s mission for this NPC and the current player. A proposal is pending until the player selects **Agree to meeting**. After acceptance, retain the mission UID for later status lookup, even after completion clears the NPC’s active list. In the NPC’s private context, emit the agreed destination, the player’s promised goods and the recorded state and outstanding quantities (for example, waiting at the rendezvous, partly delivered or fulfilled). A rejected proposal can give the speaker a transient clarification or route result. An ordinary promise stays in spoken history. Omit this block when empty. Mood and opinion continue to come from the existing character snapshot. [Clear an active mission](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk_funcs.cpp:164>) · [Find the saved mission by UID](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/mission.cpp:89>) · [Meeting quest](#from-a-radio-agreement-to-an-in-person-quest)
- **Regional map.** Insert the sketch, generated city and landmark legend, and the speaker’s current `@` from the shared overmap assembly described below. A legend key in the provisional meeting command resolves against this speaker’s sketch. [Map assembly and route check](#the-radio-overmap)

#### Example output of the map slot

The Deblois playtest produced the following regional sketch. A speaker in a different location gets a sketch centered on their own tile.

```text
North up. Radius 40 overmap tiles. One diagram cell spans 3 x 3 tiles.
          |     |
          |    G|
          |
          |
          |
          |
          |
 -A+      |
   ++     |
    +:F  ::        +-
    :C2--:C1      +D
    :::   |       |       |
    ++   ::  |    |       |
  +-+    ::+:@    +-      |
-++     -:::::: ++|       |
++        ::::::+++-------+
|         ::::::        ---
          :::::::
      +++ ::C3:::
     ++|++::::::
     |  |+::::::+
   ++E    ::::: ++
   |++-+  :::: --+--+-
       |            +B
       |             |

Legend
@       Your position (the NPC speaking on this radio). The player's position is unknown.
C1-C3   Named city centers. C1 Ansonia, C2 Piermont, C3 Deblois.
:       Urban area. Cities are dangerous.
-       Road running east-west.
|       Road running north-south.
+       Road bend, junction, or road segments combined in one diagram cell.
blank   Other or unshown terrain. The sketch does not establish a walkable route.

Familiar rural landmarks
A   Motel northwest of Piermont.
B   Motel southeast of Deblois.
D   Campground east of Ansonia.
E   Radio tower south of Piermont.
F   Pump station northeast of Piermont.
G   Pump station northeast of Ansonia.
```

## The radio overmap

The map should let a survivor say “the campground east of Ansonia” and mean the same place to a player calling from the other side of it. There are two views. The NPC gets a private sketch of the existing surface overmap within 40 overmap tiles of their current position. A traveler and a camp speaker at the same place draw on the same public geography from their first call. Personal exploration maps are deferred. The player can open their familiar in-game overmap inside the radio conversation. Neither side learns the other’s position just by opening a map.

### The sketch and its legend

Here is the actual generated example. It covers a **40-overmap-tile radius** around a speaker on a rural road in the McWilliams save. North is up. Each character cell covers up to **3 × 3 overmap tiles**. Named cities and their dangerous urban areas remain visible. The sketch gives rural landmarks such as motels, campgrounds and gas stations their own entries when present. This particular region has two motels and two utility pump stations. [Copyable map](radio-regional-map.txt)

```text
North up. Radius 40 overmap tiles. One diagram cell spans 3 x 3 tiles.
          |     |
          |    G|
          |
          |
          |
          |
          |
 -A+      |
   ++     |
    +:F  ::        +-
    :C2--:C1      +D
    :::   |       |       |
    ++   ::  |    |       |
  +-+    ::+:@    +-      |
-++     -:::::: ++|       |
++        ::::::+++-------+
|         ::::::        ---
          :::::::
      +++ ::C3:::
     ++|++::::::
     |  |+::::::+
   ++E    ::::: ++
   |++-+  :::: --+--+-
       |            +B
       |             |

Legend
@       Your position (the NPC speaking on this radio). The player's position is unknown.
C1-C3   Named city centers. C1 Ansonia, C2 Piermont, C3 Deblois.
:       Urban area. Cities are dangerous.
-       Road running east-west.
|       Road running north-south.
+       Road bend, junction, or road segments combined in one diagram cell.
blank   Other or unshown terrain. The sketch does not establish a walkable route.

Familiar rural landmarks
A   Motel northwest of Piermont.
B   Motel southeast of Deblois.
D   Campground east of Ansonia.
E   Radio tower south of Piermont.
F   Pump station northeast of Piermont.
G   Pump station northeast of Ansonia.
```

Directions in those entries are measured **from the named town to the landmark**. If the player says “the motel northwest of me,” the speaker can ask which town they mean. If the player says “south of Deblois,” the speaker can clarify “the motel southeast of town?” An unfamiliar place stays open for discussion. Its absence from the sketch tells us what this speaker can identify. A request for a known town or another landmark gives the player a way to continue.

### How much coarse terrain exists at spawn

A successful start has the entire 180 × 180 overmap region containing the character available, or 32,400 surface OMTs. The character can start anywhere in that square. The ordinary start search opens whole regions in turn and stops when it finds the required terrain. Placing mandatory world features can open additional regions. The exact footprint depends on the start. The 40-tile range used by roaming NPC updates is a separate query. [Region dimensions](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/map_scale_constants.h:37>) · [Start search](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/start_location.cpp:293>) · [Region loading](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:565>) · [Neighboring world features](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap.cpp:4077>) · [Roaming NPC range](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:24285>)

The player's revealed map is separate. In this checkout the scenario loader accepts a numeric initial visibility with a default of 15. Its getter returns a boolean, so the explicit surface reveal around the spawn receives a radius of 1 for any nonzero setting and 0 otherwise. A scenario can separately reveal the route to the nearest city and part of that city. Ordinary sight can reveal further terrain. The radio sketch reads regional terrain independently of these player visibility marks. [Scenario default](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/scenario.cpp:114>) · [Getter return type](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/scenario.cpp:587>) · [Spawn and city reveals](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/game.cpp:919>) · [Normal overmap sight](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/game.cpp:11184>)

### When the region is unloaded

Prepare the NPC’s sketch when this conversation first needs geographic context. **An occasional pause to load saved overmap data is acceptable.** Reuse regional data across replies. When the speaker moves, recenter the 40-tile view and update `@` for the next prompt. A previously agreed landmark keeps its absolute overmap tile, even once it falls outside the sketch. Building the sketch uses regional terrain, city and site records. A detailed local gameplay map is unnecessary for drawing it.

- **Resident region.** The proposed entry point is the public `overmapbuffer::get_existing()`, once per overmap region overlapped by the view. It returns a resident region immediately. For tile-to-region conversion, `get_existing_om_global()` shows the existing `project_remain<coords::om>()` pattern. A single 40-tile-radius surface view can overlap up to four 180 × 180 overmaps near a corner. [Public accessors](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.h:565>) · [Region lookup](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:1541>) · [Coordinate conversion](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:1614>)
- **Saved region, currently unloaded.** The same `get_existing()` checks the saved region and invokes the normal synchronous load. That first read and deserialization can cause the hitch. Use its terrain and cities inside the speaker’s radius, including areas the player has yet to visit. [Saved-file lookup](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:1558>) · [Native load](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap.cpp:4422>)
- **Region never generated or saved.** When the file is absent, `get_existing()` returns a null pointer. For the first version, leave that part of the sketch unknown. This keeps a radio call from generating a neighboring 180 × 180 region just to fill the edge of the view. Once the region exists, it can appear in the next sketch that covers it. [Missing-file handling](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:1541>) · [Load-or-generate path](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:565>)

### How the sketch is built

**What the prototype actually used.** A small Python `render()` read the already-decoded ground layer, `cities` and `city_tiles` from the tracked McWilliams save. It filtered tiles to the circle, grouped them into 3 × 3 cells, drew road connections across cell boundaries and overlaid cities and selected rural sites. It grouped adjacent same-kind facility tiles, preferred an entrance when one was identified, and kept each landmark’s actual world tile behind its letter. The revised legend uses eight-point bearings checked against the town centers in that save. The median **8.51 ms** over nine runs measured the original warm Python render. The native cold-load pause has not been measured. [Fixture and prototype results](radio-play-luna.md#7-regional-schematic-and-map-pins) · [Revised landmark tests](radio-play-luna.md#8-shared-landmarks-and-unfamiliar-places)

**C-AOL functions to reuse.** These are current game functions; composing the NPC sketch from them would be new radio work.

1. Read `overmap::ter(local_omt)` for terrain, `overmap::cities` for names and centers, and `overmap::is_in_city(local_omt)` for urban membership. The city test uses saved city tiles with a bounds fallback for older data. Select public towns, roads and rural landmarks inside the speaker’s 40-tile surface radius. [Terrain accessor](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap.h:496>) · [City records](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap.h:642>) · [City membership](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_city.cpp:468>)
2. Classify road tiles with `is_road()` and read their directional connections with `oter_t::has_connection()`. Use those connections to draw the coarse road strokes. Multiple roads or a bend can occupy one cell, which is why `+` cannot establish a traversable junction by itself. [Road classification](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_terrain.cpp:272>) · [Directional connections](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_terrain.cpp:875>)
3. For multi-tile rural sites, `overmap::overmap_special_at()` and `overmap::overmap_special_origin_at()` can identify the placed site where those records exist. Keep separate motels distinct. Resolve each displayed letter to an actual meeting tile, preferably the identified entrance. Generate the natural-language bearing with `direction_from(town, landmark)` and `direction_name()`. [Placed special](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap.cpp:3415>) · [Placement origin](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap.cpp:3426>) · [Direction from two points](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/line.cpp:501>) · [Direction name](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/line.cpp:672>)

### Looking up spoken directions

The speaker can say “the campground east of Ansonia.” The player opens **Map**, pans around their own known overmap and returns to the same conversation. If two campgrounds fit, they can ask the NPC which one they mean. Use the ordinary player-centered overmap display as the starting point for a browse-only view. Its cursor stays local to the player. The routing code separately checks the NPC’s private landmark and willingness to travel. [Player-centered overmap](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_ui.cpp:4350>) · [NPC route query](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmapbuffer.cpp:2742>)

**First-playtest check.** Place a speaker near a region edge. A roadside landmark on the saved side should be available even on their first call. An ungenerated neighbor should stay blank. Move the speaker, check that the view follows them, and verify that the player’s own overmap gains no automatic reveal.

## Bandit radio robbery

**What happens now.** AOL has a map-generated bandit roadblock and a live camp shakedown. The roadblock places NPCs when the terrain is generated. The camp reserves real members and routes them. Its local encounter code can still look like a pop-in: if the whole reserved party is inactive, it finds empty squares near the route endpoint, positions those same NPCs there and loads them. An already active party keeps its positions. The Pay/Fight dialogue opens afterward, with an actual local speaker and receiver. An earlier playtest verified contact, payment and return to camp. [Roadblock definition](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/overmap/map_extras.json:134>) · [Mapgen placement](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/mapgen/map_extras/bandits_block.json:92>) · [Local admission](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:6102>) · [Placement of reserved members](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:6196>) · [Contact and playtest](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/doc/overmap-ai-field-guide.md:27>)

**The radio agreement looks like an ordinary traveler’s quest.** A bandit uses the same proposal command to name a meeting landmark and request goods. The game uses the same item matching, quantity preview, **Agree to meeting** button and missing-proposal popup. Acceptance creates the same player-facing quest, including its title format, goods, progress, and **H** shortcut to the original call. The location is available to the player through the spoken directions alone. The channel, map, journal, and ordinary handoff give away no faction or future encounter type. Privately the outing stores the speaker’s ID, original camp and faction, and the reserved crew, using the existing home-reserve and route checks. [Existing member selection](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:16332>) · [Rally travel](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:4446>)

### A friendly contact who later betrays the player

**Meeting and recruitment.** The speaker travels to the landmark, and the two other bandits (one for a smaller party) wait roughly three overmap tiles away. Each destination follows the [danger and safe-neighbor rules](#detours-and-time-at-the-rendezvous). An open meadow is a valid meeting. The first in-person conversation allows an ordinary full, partial or empty handoff. The speaker can become a **normal follower**, with the usual roster entry, orders, movement and vehicle behavior until the betrayal. Use real recruitment and remember the speaker’s original camp identity for their eventual departure. Ordinary item and recruitment transactions stand. [Normal recruitment](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctalk_funcs.cpp:919>)

**When the first in-person conversation closes**, the reserved crew starts walking toward its colleague, whatever the player handed over or decided about recruitment. Save the colleague’s NPC ID. Refresh the destination from that actor’s current position through central party travel when needed. The crew can still find the speaker if the player goes somewhere else. Entering the loaded map preserves each NPC’s actual position. Existing shakedown admission expects the group together, so the radio encounter needs to support a speaker who has arrived ahead of the others. [Saved member IDs](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.h:1070>) · [Destination changes](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_party_travel.cpp:49>) · [Local handoff](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:4940>) · [Current admission guard](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:6178>)

**Two events can reveal the bandit. Whichever happens first opens the same demand once.**

- **Leave together.** Remember the actual overmap tile where the player recruits the speaker, including a safe neighboring tile or an en-route meeting. When the recruited speaker and player have left it together and reached the same new overmap tile in local contact, the speaker breaks from the follower roster and demands payment immediately. They can cross one turn apart or ride in the same vehicle. Walking across alone while the speaker waits behind leaves this trigger unspent. Check both actors’ positions. AOL’s existing boundary event reports only the avatar’s crossing. [Boundary event](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/game.cpp:12397>) · [Actor overmap position](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/creature.cpp:3855>)
- **The crew catches up first.** If the surviving crew physically rejoins the speaker while the player is still in local contact, issue the same demand, including when the player declined recruitment or stayed on the original tile. If the crew finds only its colleague, the reunion cannot open a remote demand. [Native local contact and hearing](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:7472>)

On either reveal the outing switches to robbery, the speaker leaves any player orders and regains their original faction, and the player gets **Pay / Fight**. Use the native follower-roster removal and faction setter. Normal dismissal creates a new solo faction and mutiny has wider opinion effects. The radio demand needs to admit the lone speaker for the tile-crossing trigger. Any crew still en route keeps the colleague as its destination while the encounter is active. Arrival, repeated dialogue and save/load cannot issue a second demand. Once the cover is exposed, close any outstanding delivery obligation without a failed-delivery penalty. [Roster removal](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/game.cpp:2151>) · [Faction assignment](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npc.cpp:1249>) · [Native demand dialogue](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:3226>)

#### How the robbery ends

- **Paid.** One successful surrender closes the demand immediately. A player with nothing eligible can surrender empty-handed. Earlier quest deliveries stay delivered. Crew members who arrive afterward get return orders and cannot demand a second payment. The native aftermath’s positive-value check needs a radio-specific empty-surrender outcome. [Current aftermath validation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:29931>)
- **Fight.** Choosing **Fight**, or the player starting a fight first, closes the payment choice and hands present actors to normal combat. A surviving crew member still on the way can reach the speaker and act on what they actually perceive. Combat can end in escape, retreat, player death or defeat of the party. One casualty alone does not end a live fight. [Native combat handoff](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:5970>) · [Fight reconciliation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:6332>)
- **Player escaped.** If the crew reunites with its colleague after the player has gone and none of them has current contact with the player, the attempt ends. For a separated group, try **five consecutive in-game minutes** after separation with no bandit in the outing seeing or otherwise maintaining local contact with the player. Renewed contact resets the grace period. A recruited speaker still accompanying the player counts as contact. Merely closing a menu or unloading the map cannot resolve the encounter. Save the last contact time and evaluate elapsed game time on the next eligible update. The native shakedown’s existing flight outcome describes a bandit fleeing. Player escape needs its own result. [Existing flight handling](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:7692>)
- **Attempt abandoned.** A missed meeting expires at the agreed waiting deadline. If the speaker dies before the reveal, the route becomes durably impossible, or the surviving party gives up, release the outing. For the first playtest, reuse the two-hour wait as an outer limit for an unresolved friendly meeting. A brief route failure allows normal retries. A recruited speaker who calls off the attempt leaves the party normally from their actual position and rejoins their camp. Before a reveal, the journal continues to follow the ordinary traveler rules for a missed meeting, death or outstanding goods. A private escape or abandonment must not produce a distinctive quest update. Preserve goods already delivered.
- **Finish once.** Save the decision against the outing ID and generation. A settled encounter stops updating the colleague as a target and clears any demand. A revealed betrayal closes the delivery obligation. A hidden outcome leaves the public journal on the ordinary traveler rules above. Surviving actors return through their existing camp orders when free from local combat. Confirmed deaths are accounted for. A return delayed by danger cannot reopen the player’s encounter or debit their inventory again. [Return and reservation release](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:5303>) · [Duplicate outcome guard](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:30120>)

**Earlier idea for a same-tile ambush.** Keep this as an alternative to the delayed betrayal. A surface overmap tile contains 24 × 24 local squares. A generated gas station can span four overmap tiles, so the meeting tile may contain only the forecourt. If this variant is tried, inspect only the actual loaded meeting tile once for reachable, unoccupied walking squares. Use existing light and obstruction data for scene placement and ordinary creature visibility for what the player can see. In a bare field, or if the player arrives first or spots the group early, use the visible walk-up and demand. Active NPCs keep their real positions. This needs neither authored positions nor vehicle AI. [Tile scale](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/coordinates.h:42>) · [Gas station layout](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/mapgen/gas_stations/s_gas.json:4>) · [Field layout](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/mapgen/basic/field.json:4>) · [Local light and transparency](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/map.h:1912>) · [Creature visibility](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/game.cpp:4165>)

**Pay or fight.** When the bandit reveals the robbery, the speaker makes the stakes clear before either key is pressed. A proposed line is “Strip. Everything you’re carrying, and everything in the vehicle. Now.” Use the existing two visible choices, **Pay** and **Fight**. Fight enters the ordinary hostile branch with the party already present. Pay executes the radio surrender immediately. There is no follow-up item picker. [Current dialogue](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:3226>) · [Current choice handling](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:3539>)

**What Pay transfers.** Move all removable items the player is wearing, wielding or carrying, with their contents and charges, plus all cargo in the vehicle the player brought to this encounter. Include zero-price items and cargo in that vehicle’s locked storage. For the first playtest, bind one identifiable vehicle present at the encounter, including one just parked there. The vehicle and its installed parts stay in the world. Companions, ground items, other vehicles and camp stockpiles fall outside this demand. With no vehicle, surrender covers the player alone.

Reuse the existing physical deposit into the robbers’ home stash. Give the radio branch its own item source list and pass it directly to the transfer, preserving nested items exactly once. The normal chooser can include other reachable property and uses trade filters. Setting its percentage to 100 would still give the wrong result. Record payment once, keep sources intact on a failed transfer, and handle empty hands and empty cargo as a zero-goods surrender. A reload must preserve both the lost goods and their presence at the robbers’ home. The bandits continue their existing departure/return behavior. [Current goods pool](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:2793>) · [Trade source filters](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/trade_ui.cpp:310>) · [Physical stash transaction](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctrade.cpp:496>) · [Source removal and stash save](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/npctrade.cpp:692>)

**Ordinary base-camp robbery.** Keep the existing portion-based toll and trade chooser. The base demand is 35% of the eligible goods’ value, rounded up, with existing encounter modifiers. The full surrender applies only to a bandit outing initiated by an accepted radio invitation. The current shakedown explicitly excludes the cannibal-camp profile. Cannibal meetings use the separate Fight-only encounter below. [Encounter eligibility](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:28388>) · [Base demand](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:28423>) · [Current payment chooser](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:2830>)

**First playtest.** Compare a real traveler and a bandit asking for the same goods at the same landmark. Scan, offer, confirmation, journal, history shortcut and initial handoff should match. Recruit the bandit, give an ordinary follower order and cross with them on foot and in a vehicle. Also cross alone with the bandit ordered to wait. Check staggered crossings, a crew that arrives first, and both triggers in the same update. Leave without recruiting, lose sight briefly, escape, try an unreachable route and save before and after each reveal or settlement. Observe destinations as the player and speaker separate. Try both payment choices, a zero-goods surrender, earlier partial handoff and a casualty during combat. Check that only the player and the applicable vehicle contribute goods once, and an ordinary camp demand still uses its partial-payment chooser.

## Cannibal radio encounter

Cannibals can make the same friendly radio agreement about a landmark and goods. On the first real in-person encounter with the player, the cannibal turns hostile immediately. The encounter shows one choice, **Fight**. It acknowledges an attack already underway in game state. Closing the notice or loading a save resumes normal combat. The player can move or flee on subsequent turns. The radio quest closes at the reveal without a payment or failed-delivery penalty. Other reserved actors enter through physical travel and ordinary local perception.

AOL already excludes cannibal camps from shakedowns. Its existing raid contact gate expects a pack of at least two. The radio contact needs to release a lone speaker to the same native combat AI immediately. Test a lone speaker arriving first and confirm that pressing **Fight** or dismissing the notice produces the same hostile actor and unchanged player inventory. [Shakedown eligibility](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:28388>) · [Existing pack gate](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/bandit_live_world.cpp:25275>) · [Native NPC combat](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:5970>)

## Mi-go conversation

Use **two pools of existing, authored Mi-go lines**. The ordinary Mi-go has 404 speech and sound entries in the base speech file. The lore includes researchers hearing one specimen repeat phrases another specimen had heard. The proposed selection uses that contrast between a plausible human call and a fragment of a different conversation. Mi-go radio speech needs no LLM. [Base speech pool](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/speech.json:4>) · [Mimicry observation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/speech.json:149>)

**Pool 1 — what you hear when you tune in.** Draw one opener at random.

1. “Hello?”
2. “Who’s there?”
3. “Can you help me?”
4. “Is everything all right?”
5. “I think I need to see a doctor.”
6. “Are you alone?”
7. “Have you seen my children?”
8. “No officer on scene. Requesting backup!”

**Pool 2 — what answers after you transmit.** When the Mi-go answers, draw its reply from this pool. The proposed draw skips the immediately previous line.

1. “Look, it’s responding!”
2. “That’s the first time it moved all morning.”
3. “Is that glass electrified?”
4. “There are seven backup generators.”
5. “Please open the door and enter the cell.”
6. “It won’t hurt a bit.”
7. “Only a few more days ’til the weekend.”
8. “I love you too.”
9. “Seven o’clock.”
10. “Mom, I miss you.”
11. “It’s looking right at us.”
12. “I’ve been looking all over for you.”

The display stays **Voice**. **Scanning and listening are safe.** Every player transmission immediately gives each in-range Mi-go listening on that channel the overmap tile the player transmitted from. This happens when the player sends the message, regardless of whom they address or which voice replies. When the receiving site has no response party underway, the central overmap AI dispatches an eligible party only if it can reach the transmission tile.

**New fix during pursuit.** If the site hears another transmission from a different tile, try a route from the party’s current position to the new fix. If the route reaches that tile, the same party changes course. If the fix is unreachable, the party continues toward its last reachable fix. Keep the same dispatch and responders in both cases. Moving silently gives the party no new fix.

**Where this fits in the current C-AOL code**

- **Receive the signal.** The existing turn scheduler delivers smoke and light observations to `migo_tower::receive_signals()`, which records the source location on the receiving site. Its environmental scan is throttled to five game minutes, and `valid_sample()` accepts smoke, light and searchlights. On player transmission, give each receiving Mi-go site an immediate radio observation from the sender’s tile, then reuse the tower’s response path. [Scheduler handoff](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/do_turn.cpp:17339>) · [Accepted signal kinds](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:568>) · [Receiver and timing](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:914>) · [Site records the fix](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:1078>)
- **Check the route before sending the party.** `migo_tower::begin_response()` already checks for an active operation, selects real responders, protects the tower’s home reserve and commits the dispatch. Today it reserves the party before outbound routing. For a radio fix, try the selected party’s existing Mi-go route policy against the transmission tile **before reservation**. The shared route code checks the path’s destination. Reaching a neighboring search tile alone would not satisfy this decision. [Response decision](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:1476>) · [Reservation point](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:1572>) · [Party’s route query](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:1869>) · [Endpoint check](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/overmap_party_travel.cpp:98>)
- **Reroute an active radio dispatch.** Record the newest signal separately from the party’s destination. Check the candidate route using the existing Mi-go travel policy and replace the active destination and route only after the exact new tile passes the reachability check. A failed check preserves the party’s existing travel progress.
- **Travel and investigate.** After dispatch, `migo_tower::prepare_coarse_step()` uses `overmap_party_travel::prepare_transit()` for the trip. Local and vertical approaches use physical movement, so a later obstruction can still stop an expedition. The existing sweep takes an exact source position first when supplied, and `migo_tower::service_operations()` advances the investigation. [Travel and local approach](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:1768>) · [Shared transit call](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:1902>) · [Sweep target order](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/raid_sweep.cpp:281>) · [Operation service](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/migo_tower_native.cpp:2356>)

## Portal-storm radio voice

**What the original does.** An active storm can open `TALK_PORTAL_STORM` directly while the player is outdoors in the core, outside the storm dungeon and without worn `PORTAL_PROOF` protection. The presence describes itself as a possible version of the player who has already said yes. The player can ask questions, shut it out, or take a separate “halfway” offer into the storm dungeon. The fatal branch gives the player a red warning beside **Will it**. Choosing it awards **True Freedom** (`escaped_the_cataclysm`). The last response runs `u_die` and applies the permanent `a_winner_is_u` effect, which selects the special **You won** death screen. The original allows further questions until the player makes that choice. [Native contact trigger](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/effects_on_condition/nether_eocs/portal_storm_effect_on_condition.json:354>) · [Presence and dialogue choices](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/EOC_talkers/portal_storm.json:4>) · [Halfway offer](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/EOC_talkers/portal_storm.json:40>) · [Revelation and fatal choice](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/npcs/EOC_talkers/portal_storm.json:84>) · [Ending screen](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/end_screen.json:51>)

**Proposed radio version.** Give the active storm one stable channel while its current center is within the existing 40-overmap-tile radio range. The ordinary scan reports **Voice**. Use the storm’s start, movement and cancellation state, so the contact follows the real storm and disappears when it ends or moves out of range. The early weather warning alone does not put a voice on the air. For the first playtest, the radio can carry the encounter indoors or inside a vehicle. Honor worn `PORTAL_PROOF` protection. [Native protection check](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/effects_on_condition/nether_eocs/portal_storm_effect_on_condition.json:28>) · [Moving storm center](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/effects_on_condition/nether_eocs/portal_storm_effect_on_condition.json:135>) · [Storm activation and cancellation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/effects_on_condition/nether_eocs/portal_storm_effect_on_condition.json:153>)

**Author the lines in editable game data.** Use the ordinary radio text box. Store each voice’s replies together in speaking order. Game state owns both progression and the ending. The voice needs one outgoing player transmission per step, with no LLM call or interpretation of the player’s words. Scanning, listening, typing, reading the transcript, or waiting in a menu costs no exposure. On a shared channel an open hail that the storm answers can open its conversation. Replies to a human do not advance the storm. The separate Mi-go rule still applies to every transmission it hears.

**What comes over the radio.** When the storm answers a new call, randomly choose one of the three sequences below with equal probability. Keep that choice with the call’s stage, including across map views and save/load. All three open with “I remember saying that.” On the second exchange the storm quotes the player’s opening transmission verbatim, including punctuation, then speaks its next line. For the first playtest, the fourth transmission ends the sequence.

- **The voice that misses you.** After the echo — “I used to say it just to hear somebody in here.” Third reply — “Could you leave the switch down a little longer? I can hear you breathing between the words.” Final reply — “Oh, thank God. I thought I’d have to be here alone again.”
- **The voice that remembers having a body.** After the echo — “Funny. I remember being the one with my thumb on the switch.” Third reply — “Put a hand on your throat. I want to see which of us feels it.” Final reply — “Oh. I’d forgotten how cold the radio is.”
- **The voice that remembers another tomorrow.** After the echo — “I remember two things happening after that. In one of them we put the radio down and burn the onions making supper.” Third reply — “I liked that evening. Somebody opened a window. We laughed about the smell.” Final reply — “What was I laughing about?”

**What the player sees.** The transcript uses the ordinary speaker colors. Bodily sensations appear outside it. Color alone signals the escalating danger in the interface.

1. **First transmission.** Display the opener as an ordinary reply from **Voice**.
2. **Second transmission.** After the echo and the storm’s reply, show “Your jaw aches. You feel the next words forming in your mouth before the radio says them.” in yellow.
3. **Third transmission.** After the storm speaks, show “You close your mouth. Your voice keeps speaking.” in red. Leave **Break contact** in its usual color and show **Transmit again** in red. Advancing requires a fresh, deliberate send. Reading, scrolling or dismissing the message cannot send a reply.
4. **Fourth transmission.** Display the final voice line before handing off to the native achievement, death effect and special ending. Apply the ending once. The native encounter retains its separate dungeon choice.

Breaking contact before the last transmission ends this exchange safely. Reopening the map and returning to the same call preserves its stage. If the storm ends, the signal drops and any pending final send expires. For the first playtest, start fresh on a deliberate new call after the player has tuned away. Check both devices, a shared channel with a human, protection, a quiet scan, stopping at each warning, and completing the final warned reply. Verify the death and special end screen together and check that saving and loading cannot bypass the warning or apply the ending twice.

## What the Luna experiment established

We ran **24 conversations and 60 player transmissions** with `gpt-6-luna` using landmarks from a generated Deblois overmap. With two remembered groceries, the first prompt let Luna silently book the wrong one. After a worked ambiguity example and a tool restricted to valid landmark references, all six retests asked which grocery and then selected the right one. Other runs reversed who was bringing bandages or accepted a destination the speaker could not reach. Those failures motivate checking the actual agreement in game state.

Both concealed cannibal contacts kept their cover through direct questioning. A blinded model reader made one tentative hostile guess and left three contacts unclassified. The matched callers sounded similar. Giving them room to invent ordinary details produced more varied speech. The original trial used simulated game tools and hand-selected landmarks. A follow-up supplied a generated tile grid and legend. On an 11 × 11 neighborhood Luna booked the right store or gas station in all four conversations, including when only the speaker’s position changed. In one 81 × 81 stress test it booked a distant pump station instead of the gas station across from the dump. These two trials used Luna. Human player judgment and live travel remain untested. [Original experiment](radio-play-luna.md) · [Grid experiment and replies](radio-play-luna.md#6-follow-up-with-a-generated-map)

A regional trial compared Luna and a small local Gemma 4 E2B model. A 40-tile-radius sketch of three named cities, roads and six rural landmarks occupied 777 bytes. The local model mixed up the two motels when it had to read the sketch alone. Adding computed compass bearings gave both models the correct geography in these runs, though the local model sometimes mishandled the private reference. Both voiced a simulated too-far refusal followed by an acceptable pin in the final test. That trial measured bearings from the speaker. For shared meeting directions, use bearings from named landmarks. The pin was an experimental input. The chosen player interface uses spoken directions and a browse-only map. [Regional sketch](radio-regional-map.txt) · [Trials and current game pointers](radio-play-luna.md#7-regional-schematic-and-map-pins)

In the town-based follow-up, Luna and local Gemma both identified the campground east of Ansonia and the motel southeast of Deblois. Luna asked for clarification on conflicting or unfamiliar places. In the structured local trial Gemma sometimes submitted a landmark even when its spoken line admitted uncertainty. The game should keep an unresolved place open; a valid-looking landmark key alone is insufficient to dispatch a party. [Corrected legend](radio-regional-map.txt) · [Shared-landmark and unknown-place tests](radio-play-luna.md#8-shared-landmarks-and-unfamiliar-places)

## Existing game systems to carry forward

- **Equipment and known contacts.** The handheld declares `TWO_WAY_RADIO`. The existing `has_radio()` and `can_radio_contact()` functions are useful references for equipment eligibility and radio contact. A direct scan for unfamiliar voices would be new. The powered EOD helmet also carries the flag. Whether other integrated radios should open this feature is still open. [Handheld definition](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/items/tool/radio_tools.json:165>) · [Contact checks](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/faction_ui.cpp:72>) · [Helmet](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/data/json/items/tool_armor.json:1423>)
- **Vehicle controls and conversation UI.** A mounted voice transceiver would be a new base vehicle part. `vehicle::build_electronics_menu()` is an existing access point. The bandit shakedown already uses the normal `dialogue_window` presentation. [Vehicle electronics menu](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/vehicle_use.cpp:309>) · [Existing dialogue presentation](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/doc/bandit-scenic-shakedown-chat-window-openings-proof-v0-2026-05-02.md:9>)
- **Travel, camp responses and pursuit.** Human outings and Mi-go investigators already share `overmap_party_travel::prepare_transit` and hand movement to physical actors at the loaded boundary. Mi-go towers already receive signals and reserve responders. The bandit shakedown has documented dispatch and physical contact. For independent travelers use the [ordinary NPC travel path](#how-an-ordinary-roaming-npc-gets-to-the-meeting). The [bandit radio robbery](#bandit-radio-robbery) uses camp dispatch and the physical shakedown. The betrayal adds ordinary follower recruitment, two possible reveal events and a saved end to the encounter. [Overmap AI field guide](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/doc/overmap-ai-field-guide.md:17>) · [Bandit outing](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/doc/overmap-ai-field-guide.md:25>)
- **Mi-go voice selection.** Existing `PARROT` behavior calls `get_speech()` to select an entry for the monster. The radio design narrows the selection to the two pools above. [Parrot behavior](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/monattack.cpp:3390>) · [Speech selection](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/speech.cpp:31>)

## LLM startup, options and diagnostics

**Start automatically.** Start the runner and test the configured dialogue backend in the background when the main menu opens. Remove **Enable LLM intent runner** from the options and retire the old runtime gates, including for profiles saved with the option off. Resolve the same backend for startup, the self-test and real NPC calls. Migrate the hidden legacy API override so it cannot silently supersede the selected mode. Today the master switch defaults off, and the existing post-load prewarm skips API mode. The runner already provides self-tests for API, Ollama and OpenVINO. [Master and backend options](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/options.cpp:3025>) · [Runtime gate](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:3682>) · [Backend resolution](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:2883>) · [Current prewarm](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/llm_intent.cpp:3828>) · [Backend self-tests](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/tools/llm_runner/runner.py:638>)

**Main menu.** Show a compact status such as “Checking Ollama…” followed by ready or an actionable failure, for example “Ollama is running, but the selected model is unavailable.” On failure offer keyboard-accessible **Open LLM settings** and **Retry**. Keep the main menu and Options usable throughout, including on first launch without credentials or a local model. Run one bounded check per startup, relevant settings save or explicit retry. Ignore stale results if the user changes backend while a check is in flight. A missing-key message may identify the environment variable by name. [Main menu](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/main_menu.cpp:654>) · [Options entry](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/main_menu.cpp:1049>)

**Backend first.** Put the **API / Ollama / OpenVINO** selector first. With **API**, show the API model and key environment-variable name. With **Ollama**, show its URL and model tag. With **OpenVINO**, show the model directory, device and applicable advanced local options. Filter both visible and keyboard-selectable rows immediately, and retain each backend’s saved values when switching away. Keep shared runner settings such as Python path and timeout in a common section. Group the existing build-time background-summary settings separately under their own selector. [Registered settings](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/options.cpp:3037>) · [Summary backend](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/options.cpp:3119>) · [Visible rows](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/options.cpp:3741>) · [Selectable rows](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/options.cpp:3967>)

**Debug stays available in every mode.** Proposed reading of “debug should still be on”: default the in-game LLM debug messages on for new profiles and keep the separate verbose prompt/response logging control. Respect saved debug preferences. Display ordinary radio voices and quests consistently while debug is enabled; a player should have to open diagnostics deliberately to inspect private role or routing data. Show the backend, check result and useful failure reason without printing secret values. [Two current debug controls](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/options.cpp:3013>)

**Startup test.** Cold-start each supported backend, including a saved profile with the former master switch off. Exercise a missing API key, unreachable Ollama endpoint and invalid OpenVINO model path. Change the backend from the error screen, keep navigating by keyboard while it tests, repair the setup, and make a real NPC call without restarting. Check that only the selected backend’s fields receive focus, that switching back restores its values, and that reopening the menu does not repeatedly call a paid API.

## Handoff to DE67 2

The phase 1 design is complete. The first playable journey runs from hearing an unknown voice to finding the speaker from their spoken directions and resolving the meeting with the same physical NPC. The central overmap AI owns travel and dispatch. The Luna calls exercised dialogue outside the game. In-game timing, travel and delivery still need the playtests below.

**Two details for the specification.** For a promised frequency and time, decide whether the speaker may announce an existing channel schedule or can arrange a new one in conversation. The former is the proposed first version. For a giver who dies out of sight, decide when the journal reveals the loss. The proposal is to keep an unwitnessed death unknown until the player learns what happened or chooses to abandon the entry. Apply the same public rule to a traveler and an unrevealed bandit. These proposals remain open.

For the first equipment pass, the agreed devices are the handheld and mounted two-way radios. Which vehicles carry the mounted set is a content choice. The two-hour wait, crew approach and escape timing, trust adjustments and cost of transmitting have first-playtest guidance in the relevant chapters.

### In-game playtests

**First traveler playtest.** Use only the keyboard from inventory through scanning, a channel with Voice, writing, opening Map and accepting a deal. Confirm that plain letters type and returning from Map restores a half-written message. Track the quest and check the goods and remaining quantities. Press **H** after a channel change and reload to reread the call that established it. Check that directions appear only in the spoken transcript across the offer, journal, map and HUD. Meet the same traveler before arrival and try partial, empty and full delivery. Check the remaining trip, credited goods, opinion and recruitment after each outcome. Try a new threat across the route, an unsafe landmark with one reachable neighbor, and an area with no safe neighbor. Save during the two-hour wait and check departure after reload or re-entry into the 40-tile travel area. Also cover the no-proposal popup, another voice on the same frequency and a model failure at handoff. Check substitutions, single application of opinion changes and long-history recall and latency.

**In-game encounter tests.** Use the real scan, proposal, player acceptance and party identities. The debug menu can raise a **loaded** NPC’s trust for repeatable setup, move the player to a chosen overmap tile and spawn a monster. Stage the player’s starting position before the encounter. Allow actual NPC travel and normal boundary crossing to produce the outcome. Also keep one run at ordinary trust. [Edit an NPC’s trust](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/debug_menu.cpp:2416>) · [Loaded character selection](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/debug_menu.cpp:2608>) · [Long-range teleport](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/debug_menu.cpp:2094>) · [Spawn monster](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/debug_menu.cpp:1213>)

- **Shared channel.** Reproduce player no, Finch yes, Robin yes, player yes. Verify that Robin hears Finch, that the player can read both voices before typing, and that the world records a player transmission only when the player actually sends. Neither NPC can commit the player to a deal.
- **Traveler.** Run both arrival orders. Watch the actual traveler approach when the player arrives first. In the other run, find them already waiting, hand over the promised goods, and try recruitment face to face.
- **Bandits.** Run both arrival orders and both reveals. Stay at the meeting until the crew comes to its colleague and hear one demand. In a separate run, recruit the speaker and leave the meeting tile together while the crew is still en route. Check **Pay** and **Fight**, and check that crossing alone while the follower waits behind does not trigger the follower betrayal.
- **Cannibals and Mi-go.** A cannibal contact goes straight to the Fight-only encounter. Put a human and a Mi-go on the same channel. Scan, listen and type an unsent draft: the Mi-go gets no fix. Send a message addressed to the human: the Mi-go immediately gets the sender’s tile, even if only the human replies. Repeat with reachable and unreachable routes to verify the dispatch rule. A Mi-go on another channel gets no fix from that message. With a party already en route to tile A, transmit from reachable tile B, then from unreachable tile C. Verify that the party turns toward B, then keeps heading there after hearing C. The dispatch ID and responder IDs stay the same throughout. Neither case requires the human delivery transaction.
- **Interruptions.** Use the existing overmap threat debug action to place a moving horde across a traveler’s route outside the loaded area. Bring that same traveler and threat into the detailed simulation by approaching normally. Compare the NPC’s identity, position, condition, deadline and quest before and after the transition. Test the opposite arrival order with a monster entering an already occupied waiting tile. Save and reload across one transition. On a separate run, keep radio chat or the map open, then return to normal turns and exercise an interrupted handoff or demand. Check that time advances only on simulation updates, the actor can resume if safe, and no injury, item transfer or opinion change is counted twice. [Overmap threat debug action](</Users/josefhorvath/Schanigarten/Cataclysm-AOL/src/debug_menu.cpp:1217>)

**Open playtest questions.** How often does a crew starting roughly three overmap tiles away reach its speaker during an ordinary meeting? Does the proposed five-minute loss-of-contact period recognize a genuine escape without ending an encounter during a brief separation?
