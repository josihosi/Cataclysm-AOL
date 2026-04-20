# Bandit concept formalization follow-through work packages and micro-items (2026-04-19)

This document greenlights a **bottom-of-stack conceptualization pass** for the parked bandit packet.

The point is not to start coding bandit AI.
The point is to stop leaving the remaining control law as muttered vibes, stale "decay" wording, and scattered half-rules across several parked docs.

This follow-through is intentionally decomposed into **small single-question spec items** so Andi can pressure-test one law at a time instead of smuggling five assumptions into one nice-sounding paragraph.

## Operating rule

- **One package at a time.**
- **One micro-item at a time inside that package.**
- This is **doc/spec work only**, not implementation.
- Keep it at the **bottom of the greenlit pile** until the current locker/basecamp fixes and the already-greenlit backlog above it are honestly done or deliberately skipped.
- Use the existing parked bandit docs as substrate, but correct them where they still smuggle in wrong passive-decay assumptions.
- If a micro-item reveals a real unresolved design fork, stop and report instead of inventing fake certainty.
- Do not silently answer multiple micro-items at once just because the laws smell related.

## Frozen footing

These rules are already decided for this follow-through:

- there is **no passive decay** for bounty
- there is **no passive decay** for threat
- structural bounty is set by terrain / tile class and reduced by harvesting or exploitation
- moving bounty changes when real actors or visible activity change
- threat changes when bandits actually recheck / observe something and rewrite their read
- bandit camp stockpile may go down by explicit consumption, and any future spoilage rule must stay explicit and separate from bounty/threat logic

## Inputs already available

Use these as the current substrate instead of restarting from zero:

- `doc/bandit-overmap-ai-concept-2026-04-19.md`
- `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`
- `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

---

## Package 1. Bounty source, harvesting, and camp-stockpile rules

Resolve these as **separate micro-items**. Do not treat "resource law" as one blob.

### A. Structural bounty law

#### 1. Terrain bounty bucket table
- **Question:** Which terrain / site classes carry base structural bounty?
- **Expected output:** One starter table covering at least open street, field, forest, cabin, house, farm, city structure, and camp footprint.
- **Done when:** The table makes the current lean explicit, fields/open streets near-zero, forests low but non-zero, buildings materially higher where intended.

Current answer:
- Treat this as `structural_bounty_base`, a site-class prior before mobile activity, threat, distance burden, or harvest depletion are applied.

| Terrain / site class | Starter structural bounty bucket (0-5) | Why this starts here |
| --- | --- | --- |
| Open street | 0 | Movement surface only. Any real value here should come later from moving traffic or intercept opportunity, not from the pavement magically printing salvage. |
| Field | 0 | Broad open ground by itself offers little recoverable value unless later rules add active workers, caravans, or a harvestable destination. |
| Forest | 1 | Low background value from shelter, wood, game, and occasional salvage, but still clearly below built sites. |
| Cabin | 2 | Small sheltered stash potential and occasional supplies matter, but the footprint is limited and easy to strip. |
| House | 3 | Ordinary residential interiors offer repeatable medium-value salvage, food, tools, and shelter worth checking. |
| Farm | 4 | House-plus-barn/tool/food storage makes a farm materially richer than an ordinary house even before any fresh activity cues. |
| City structure | 4 | Urban interiors, shops, workshops, and apartment blocks justify high base interest from structure alone. |
| Camp footprint | 5 | A discovered lived-in footprint implies concentrated reusable value; if people are visibly active there, that extra pull belongs on the moving-bounty and threat layers, not hidden inside the terrain bucket. |

Guardrails from this table:
- Roads, streets, and fields do **not** climb out of the basement just because traffic later passes through them; that later value belongs to moving bounty or route/intercept rules.
- Forest stays low but non-zero as background structural bounty, not as a secret all-purpose food printer.
- Built sites are deliberately separated into cabin < house < farm/city structure < camp footprint so later scoring can be explicit about why a place looks worth checking.

#### 2. Structural bounty harvest trigger rule
- **Question:** What exact bandit action counts as harvesting or exploiting structural bounty?
- **Expected output:** One rule note that says what must happen before a site loses some or all structural bounty.
- **Done when:** Later implementation will not have to guess whether mere visitation, scouting, looting, raid completion, or occupation caused the depletion.

Current answer:
- Structural bounty is harvested only when a bandit outing completes a **site-contacting exploitation pass** that actually converts site-bound value into haul, usable supplies, salvage, denied occupancy, or meaningful site degradation.
- The trigger is **conversion or denial of site value**, not mere awareness that the site exists.

| Bandit action | Counts as structural-bounty harvest? | Why |
| --- | --- | --- |
| Distant sighting, route pass, or rumor confirmation | No | The group learned something, but did not touch the site's recoverable value. This can write or refresh bounty/threat marks, not deplete structural bounty. |
| Quick scout or entry probe that leaves without taking, using, damaging, or occupying anything meaningful | No | Mere visitation is not harvest. Otherwise every cautious look would falsely empty the map. |
| Successful snatch loot, salvage grab, forage pass, or other bounded extraction that removes real site-bound value | Yes, partial | The outing converted some of the site's structural value into haul. The later yield rule decides how much was converted, but the depletion trigger starts here. |
| Sustained stripping, repeated haul runs, destructive raiding, or occupation that materially denies the site to others | Yes, heavy or full | The group did more than peek or skim. It left the site meaningfully poorer, emptier, or less usable, so structural bounty should drop hard or bottom out. |

Guardrails from this rule:
- **Scouting, proximity, and failed approach do not deplete structural bounty.** They mostly rewrite confidence, marks, and threat.
- **Combat by itself does not count** unless it opens the way to actual extraction, stripping, or denial of site value.
- **Occupation counts only when it meaningfully denies or consumes the site's value**, not when a group merely pauses there for a moment on the road.
- **Raid completion is not the trigger phrase.** The real trigger is whether site-bound value was actually converted or denied.

#### 3. Structural bounty reappearance rule
- **Question:** Under what real-world changes, if any, can structural bounty become worth considering again?
- **Expected output:** One explicit rule for whether structural bounty can return, and by what world change rather than by passive timer.
- **Done when:** The packet no longer hand-waves about reset or drift.

Current answer:
- Structural bounty does **not** regenerate from idle time, stale-memory cleanup, rumor refresh, or nearby moving activity.
- After depletion, structural bounty only comes back when the world materially puts **new site-bound value** at that location again.
- If the new attraction is only people, lights, smoke, caravans, or route traffic, that is renewed **moving bounty**, not restored structural bounty.

| World change after depletion | Structural bounty returns? | Why |
| --- | --- | --- |
| Time passes and nobody materially changes the site | No | No passive refill. Empty time does not restock a stripped house. |
| Travelers, patrols, caravans, smoke, or lights pass nearby without leaving durable site-bound value behind | No, moving bounty only | The region may become interesting again, but that interest lives on actor/signal layers rather than the structural bucket. |
| A group deliberately restocks, stashes, repairs, rebuilds, or newly occupies the site in a way that leaves reusable supplies, infrastructure, or camp footprint behind | Yes, partial to strong | The place itself became richer again, so structural bounty can be rewritten upward from the new site state. |
| The site honestly changes class in the world, such as a house becoming a lived-in camp or a farm becoming an active defended holding | Yes, rewrite from the new class | This is not "regeneration" so much as a real world-state promotion into a richer structural-bounty tier. |
| Natural regrowth or seasonal recovery that is not explicitly modeled as new harvestable site-bound value | No by default | Future renewable-resource rules must be written explicitly instead of smuggled in as generic reset magic. |

Guardrails from this rule:
- **Structural depletion persists until a real site-state change writes new structural value.**
- **Renewed smoke, light, or traffic can justify a revisit without restoring structural bounty.** Those cues may lead bandits back in, but they belong on the moving-bounty layer unless the revisit discovers real new site-bound stores or infrastructure.
- **Observed reoccupation/resupply can rewrite the structural bucket upward when it leaves durable location-bound value behind.**
- **"Recent activity" is evidence, not the resource itself.** Do not auto-refill structural bounty just because something happened nearby.

### B. Moving bounty law

#### 4. Moving bounty source table
- **Question:** Which live signals create moving bounty?
- **Expected output:** One source table covering humans, NPC traffic, basecamp routine, caravans, smoke, light, sound-derived contact clues, and similar activity.
- **Done when:** Each source is named clearly enough that later mark generation can point to a real category instead of vibes.

Current answer:
- Moving bounty starts from **current actors, current site use, or fresh activity clues** that imply actors or carried value are present now or very recently.
- It is not the structural worth of the ground itself, and this micro-item still does **not** decide where the bounty lives after the source moves or goes quiet.

| Live source family | Creates moving bounty? | Starter pull | Why this counts |
| --- | --- | --- | --- |
| Direct visible humans / NPC groups | Yes | Strong | A directly seen person or group is the clearest sign of current carried goods, vulnerable movement, labor, or escort-worthy traffic. |
| Repeated route traffic | Yes | Medium, rising to strong with repetition | A road or corridor with recurring movement promises future intercept opportunity even if no one actor stays there. |
| Caravans, pack trains, or visible haul convoys | Yes | Very strong | Concentrated mobile cargo and predictable movement are exactly the kind of moving value bandits want to notice. |
| Basecamp routine or obvious occupied-site work | Yes | Medium to strong, site-centered | Hauling, guard shifts, farming, repairs, cooking, or repeated in/out motion prove active use without pretending the dirt itself became richer. |
| Smoke | Yes | Weak to medium alone | Smoke is a coarse sign that somebody is cooking, burning, stripping, signaling, or otherwise doing something active here now. |
| Ordinary visible light | Yes, usually night-weighted | Medium | Visible night light usually means current use or occupation more reliably than old smoke, even though it still does not identify exactly who is there. |
| Searchlights, patrol beams, or other obvious scanning light | Conditional, threat-first | Weak bounty, strong threat implication | They still prove active people and current use, but their first meaning is organized defenders or alert posture rather than easy profit. |
| Sound-derived contact clues | Yes | Weak to medium, stronger when repeated or corroborated | Gunfire, engines, chopping, construction noise, shouted voices, and similar cues imply current actors or active work without requiring clean visual confirmation. |

Guardrails from this table:
- **Moving bounty comes from live presence or live-use clues, not from empty terrain class.** Roads, fields, and buildings only contribute here when something is actively happening on or around them.
- **One weak cue may create only a low-confidence moving-bounty read.** Repetition or corroboration strengthens the read, but does not silently convert it into structural bounty.
- **Direct sightings, caravans, and repeated traffic are the strongest pure moving-bounty sources** because they point at interceptable mobile value, not just possible occupancy.
- **Searchlights and similar defensive cues are not free bounty.** They can still create moving interest because people are active there, but the same cue should sharpen threat more than bounty.

#### 5. Moving bounty attachment rule
- **Question:** Is moving bounty attached to an actor, a route segment, a site-centered mark, or some bounded mixture?
- **Expected output:** One narrow rule describing where moving bounty lives in the abstract model.
- **Done when:** The packet no longer blurs together actor bounty and ground bounty.

Current answer:
- Moving bounty always lives on a **source-shaped carrier**, not as anonymous value baked into the dirt.
- Use the **smallest stable carrier the evidence honestly supports**:
  - **actor/group attachment** when the bounty comes from directly seen people, a caravan, a haul convoy, or another clearly moving group
  - **route attachment** when the value is repeated passage through a corridor or chokepoint rather than one anchored site
  - **site-centered activity attachment** when smoke, light, repeated work, or other occupancy/use clues keep pointing back to one place
- Later mark/heatmap logic may summarize those carriers coarsely, but the attachment class should stay explicit so scoring knows whether it is valuing a moving group, an intercept corridor, or an active site.

| Source family | Primary moving-bounty attachment | Why this lives there |
| --- | --- | --- |
| Direct visible humans / NPC groups | Actor/group | The value is literally on the observed people and what they are carrying or doing. |
| Caravans, pack trains, or haul convoys | Actor/group | The cargo is mobile and should travel with the convoy rather than becoming fake ground richness. |
| Repeated route traffic | Route segment / corridor mark | The opportunity is future interception along a path, not ownership of the pavement itself. |
| Basecamp routine or obvious occupied-site work | Site-centered activity mark | The signal says this place is being actively used, even if the exact workers rotate in and out. |
| Smoke, ordinary visible light, or fixed-location sound/work clues | Site-centered activity mark | These cues imply meaningful current use at or near one place without proving a specific actor identity. |
| Searchlights, patrol beams, or defensive scanning light | Site-centered activity mark, threat-first | The cue still anchors to active defenders at a place, even though its first meaning is danger rather than easy profit. |

Guardrails from this rule:
- **Do not store moving bounty as bonus structural terrain value.** A hot road is still a road; the moving interest lives on a corridor mark, not in the asphalt bucket.
- **One region may carry structural bounty and moving bounty at the same time.** A house can have medium structural value while also hosting fresh site-centered moving activity.
- **Site-centered moving bounty is not the same thing as proven settlement truth.** It only says the current activity points back to that place as the likely carrier.
- **Weak evidence should stay coarse.** If a cue only supports route-level or site-level attachment, do not upgrade it into exact actor identity just to feel clever.

#### 6. Moving bounty clear / rewrite rule
- **Question:** When the source changes or disappears, how is moving bounty rewritten, cleared, or replaced?
- **Expected output:** One rule for source-gone, source-moved, and source-confirmed cases.
- **Done when:** Later implementation does not need to invent fake "soft decay" language to clean up stale moving signals.

Current answer:
- Moving bounty is rewritten by **new evidence about the carrier**, not by a background timer shaving numbers off an old hunch.
- Treat each moving-bounty carrier as the current best read of one live source family, then apply the smallest honest rewrite when fresh evidence arrives.

| New evidence about the source | Rewrite action | Why |
| --- | --- | --- |
| The same actor/group, convoy, or site activity is reobserved on the same carrier | Refresh or overwrite that same carrier's current read | The opportunity is still live in the same abstract place, so the model should update the read instead of minting a second ghost copy. |
| The same actor/group or convoy is now clearly somewhere else | Move the actor-attached moving bounty to the new position/path and clear the old location attachment unless separate route/site evidence still supports it | Mobile value travels with the carrier. The old spot does not keep fake bounty just because the last sighting happened there. |
| Repeated corridor traffic is seen again without one fixed actor identity | Refresh the route-attached carrier on that corridor | The opportunity still lives on the intercept path rather than on a specific remembered traveler or on the ground itself. |
| A site-centered cue is honestly disproved by a negative recheck, quiet revisit, or other close look that finds no current activity | Clear that moving-bounty carrier; if later systems want recently-checked or false-lead memory, store that separately from bounty | This is the honest way to remove stale moving interest without pretending it numerically decayed away on its own. |
| A different live source now explains the same region better, such as smoke resolving into a caravan or vague route traffic resolving into a defended occupied site | Rewrite to the smallest carrier class the new evidence supports, replacing the old read if it is the same opportunity or coexisting only if both live sources are separately evidenced | Better evidence should sharpen the carrier instead of leaving every earlier rough guess alive forever. |
| Fresh evidence shows multiple independent live sources in one region | Keep multiple carriers side by side | A farm can still have site-centered activity while a caravan is also passing nearby; one true signal should not erase another unrelated one. |

Guardrails from this rule:
- **No passive moving-bounty melt.** Source change, negative recheck, or better replacement evidence should do the cleanup work.
- **Do not convert vanished moving bounty into structural bounty.** If the live source is gone, the live source is gone.
- **Carrier class may change when evidence sharpens.** Vague site smoke can later become a known convoy, or recurring convoy sightings can collapse back into a route-level intercept read if only the corridor remains well supported.
- **Low-value stale clutter may be pruned later, but pruning is cleanup, not hidden decay law.** The real conceptual rule is still overwrite, clear, replace, or coexist based on evidence.

### C. Threat law

#### 7. Threat source table
- **Question:** Which conditions create threat marks in the first place?
- **Expected output:** One source table covering zombies, defenders, fortification signs, recent losses, gunfire, failed probes, and similar causes.
- **Done when:** Threat sources are explicit enough that the later score law can point to concrete inputs.

Current answer:
- Threat marks start from **danger evidence**, not from generic attractiveness. The model should write threat when bandits see resistance, hostile pressure, costly uncertainty, or remembered bad outcomes.
- Ordinary smoke, routine occupancy light, and generic route activity stay **bounty-first** unless they arrive with one of the threat-source families below.

| Threat source family | Creates threat? | Starter severity | Why this counts |
| --- | --- | --- | --- |
| Direct visible humans or NPC groups | Conditional | Weak to medium until force is clearer | People are never perfectly safe free loot. Even a plain sighting can justify a low-confidence threat seed, but broad threat confidence should stay below bounty until closer evidence sharpens it. |
| Armed defenders, sentries, patrol groups, or other obvious organized human force | Yes | Strong | Clear hostile or defensive manpower is the cleanest direct human threat source. |
| Fortification signs, barricades, firing positions, traps, or obviously controlled chokepoints | Yes | Medium to strong | Static defensive preparation makes a place harder and riskier even before defenders are fully counted. |
| Searchlights, scanning beams, active watch routines, or obvious alert patrol behavior | Yes | Strong | These are live defender-control signals, not cozy occupancy clues. They should mint threat immediately rather than being flattened into ordinary light. |
| Gunfire, explosions, alarm noise, shouted contact, or other clear combat/contact cues | Yes | Medium, rising to strong with repetition or corroboration | Active violence or alarm says the region is dangerous now, whether the cause is defenders, bandit competition, or chaotic fighting. |
| Failed probes, blocked approaches, near-ambushes, hard withdrawals, or other costly close attempts | Yes | Strong | A bad close look is better threat evidence than distant rumor. If a probe gets repelled or turns ugly, the region deserves a serious threat mark. |
| Recent bandit losses, missing returners, or wounded survivors tied to the region | Yes | Very strong | This is hard mission-result memory. A place that already bloodied one group should stay scary until later recheck really changes the read. |
| Dense zombie or other monster pressure | Yes | Medium to strong | Nonhuman danger still creates real threat even when no defenders are present, because it raises approach, fight, and retreat risk. |

Guardrails from this table:
- **Threat is not created by every bounty clue.** Smoke, ordinary static light, repeated route traffic, and vague occupancy noise remain bounty-first unless paired with defenders, monsters, losses, failed probes, or other hard-danger evidence.
- **Human presence can seed low-confidence threat, but organized force is the real sharpener.** This preserves the intended asymmetry where bounty is often visible earlier than threat.
- **Searchlights and patrol-style scanning count as threat-first evidence.** Ordinary lamps, campfires, and lit windows do not.
- **Recent losses and failed probes count as source families in their own right.** Exact rewrite or downgrade rules belong to micro-item 8, not here.

#### 8. Threat rewrite rule
- **Question:** What kinds of recheck or observation can raise, confirm, or lower threat?
- **Expected output:** One explicit rewrite rule that replaces any lingering passive-threat language.
- **Done when:** The packet plainly says threat only changes through real recheck, not idle clock shave.

Current answer:
- Threat is rewritten only by **fresh evidence about the danger source**, not by an idle timer.
- Rewrite the **narrowest threat source the new observation actually touched**. A quiet route pass can lower route danger if it really checked that corridor, but it does not magically erase separate zombie pressure, fortification memory, or prior-loss memory somewhere nearby.

| Recheck or observation | Rewrite action | Why |
| --- | --- | --- |
| A closer look reveals stronger defenders, harder fortifications, denser monster pressure, or worse retreat geometry than the current read | Raise or overwrite that source upward | Threat rises because bandits learned the area is actually nastier than they thought, not because time matured the mark. |
| Repeated corroboration shows the same danger still present at roughly the same level, such as another sentry sighting, another watchlight pass, or another tense approach with no contradiction | Confirm and keep the current threat read sticky, optionally tightening confidence modestly | This preserves the scary read without minting duplicate ghost threat every time the same danger is glimpsed again. |
| A failed probe, hard withdrawal, new casualties, or another costly mission result hits the same region | Raise sharply and write a sticky mission-result threat read | Bad outcomes are stronger evidence than rumor. If the place bloodies a group again, the model should say so plainly. |
| A close revisit, scout visibility pass, or successful later passage finds one previously feared source absent or materially weaker | Lower or clear only that specific source | Threat can go down, but only because someone actually looked again and found less danger there. |
| A recheck changes the best explanation, such as apparent armed defenders resolving into mostly zombie pressure, or searchlights disappearing while barricades still remain | Rewrite the source mix to match the better classification, keeping any remaining supported danger | Better evidence should refine the danger story instead of doing an all-or-nothing wipe. |

Guardrails from this rule:
- **No passive threat decay.** If nobody rechecks the area, the threat read does not get cheaper just because the clock advanced.
- **Recent-loss and failed-probe threat is the hardest to lower.** Require a later close revisit, successful passage, or equally strong contrary mission-result evidence before downgrading it.
- **Repeated confirmation is not free stacking.** Confirming the same danger should mostly preserve or stiffen the read, not inflate it forever.
- **Threat downgrades should stay source-specific.** Clearing one corridor watch or one defender post does not erase unrelated danger sources that still have support.

#### 9. Threat-and-bounty coexistence rule
- **Question:** How can one region be both attractive and dangerous at the same time?
- **Expected output:** One rule explaining coexistence instead of silent overwrite.
- **Done when:** The model no longer implies that threat deletes bounty or bounty deletes threat.

Current answer:
- Threat and bounty are **separate concurrent reads**, not one shared regional mood meter.
- A region, route, site, or actor can therefore stay **worth attacking and risky to approach at the same time** when the evidence supports both.
- New threat evidence does **not** delete supported bounty, and new bounty evidence does **not** delete supported threat, unless the same recheck directly disproves the older source.
- Keep the **smallest honest coexistence shape**:
  - **same carrier** when one actor, corridor, or site is both lucrative and dangerous
  - **separate nearby carriers** when the value and the danger are related but come from different supported sources in the same region
- Later job scoring decides whether the opportunity is worth the risk. Mark-writing should preserve both reads instead of flattening them into one verdict.

| Situation | Bounty read | Threat read | Coexistence outcome | Why |
| --- | --- | --- | --- | --- |
| Active camp with stores, workers, and visible sentries | High structural bounty plus site-centered moving bounty | High defender/watch threat | Keep both on the same site-centered region | A rich camp is often rich **because** people and stores are concentrated there, and those same facts can also make it dangerous. |
| Repeated caravan corridor with escorts or recent failed ambushes | High route or actor bounty | Medium to high corridor threat | Keep both on the same route/intercept carrier | A road can be profitable specifically because valuable traffic keeps using it, while escorts, alarms, or bad outcomes make the same corridor costly. |
| City edge with scavenger traffic and dense zombie pressure | Medium to high structural/moving bounty | High monster/contact threat | Keep both regional reads alive together | Chaos and salvage opportunity can rise together, danger does not magically erase the value, and value does not make the area safe. |
| Quiet revisit that finds one old sentry post gone while supplies or active use are still supported by other evidence | Supported bounty may remain | Lower or clear only the contradicted threat source | Rewrite only the disproved side | Source-specific recheck should refine the model, not wipe the whole region clean just because one fear or one opportunity changed. |

Guardrails from this rule:
- **Do not zero bounty just because threat rose.** A hard target can still be a tempting target.
- **Do not zero threat just because bounty rose.** Richer evidence can mean better payoff and worse danger at once.
- **Only direct contradiction clears one side.** If a recheck proves the convoy left, clear that actor bounty. If a recheck proves the watch post is gone, lower that threat. Do not let either update silently erase unrelated supported reads.
- **Threat-first and bounty-first cues stay asymmetric.** Searchlights may write strong threat plus only weak bounty from active people, smoke may write bounty first and add threat only when harder danger evidence arrives.
- **The downstream decision layer compares the two.** The mark layer's job is to preserve the honest coexistence, not to pre-decide whether the camp should attack, avoid, stalk, or wait.

### D. Harvesting and stockpile law

#### 10. Destination-only vs along-route collection rule
- **Question:** Can bandits collect value only at the explicit destination, or also opportunistically while traveling?
- **Expected output:** One bounded route-collection rule.
- **Done when:** Later logic will not secretly oscillate between destination-only and vacuum-cleaner route harvesting.

Current answer:
- Bandits may collect value at the explicit destination **and** opportunistically while traveling, but only from opportunities that sit on the **current route or immediate intercept envelope**.
- Route-side collection is therefore **bounded skimming**, not permission to sweep every nearby site. If the opportunity would require a meaningful detour, broad local search, or full goal rewrite, it stops being free along-route collection and becomes a later divert/replan question instead.
- Keep the carrier honest: route-side collection should come from **moving bounty or directly encountered exposed value** on the corridor actually being traveled, not from pretending the road tile itself is an endlessly harvestable treasure field.
- The destination remains the outing's primary plan unless immediate route contact yields a small honest opportunistic take on that same path.

| Situation | Collection allowed? | Why |
| --- | --- | --- |
| The group reaches its planned target site and strips or steals what it can there | Yes | Destination collection is the normal baseline and does not need special route law. |
| The group encounters a traveler, convoy, camp work party, or similarly exposed moving target on the corridor it is already using | Yes | That is genuine along-route moving-bounty collection on the current path, not a separate harvest expedition. |
| The group spots a tempting side site that would require a real detour, fresh search pattern, or abandoning the original path | No, not under this rule | That crosses into later diversion / target-switch law instead of bounded opportunism. |
| The corridor itself has repeated ambush value because traffic keeps using it | Yes, but only through intercepted moving targets | The route keeps value as an intercept carrier, not because the road terrain endlessly prints structural loot. |

Guardrails from this rule:
- **No destination-only rigidity.** Bandits do not have to ignore an easy honest haul that appears directly on the route they are already traveling.
- **No vacuum-cleaner route harvesting.** A mission does not become a free excuse to zig-zag through every nearby shack, field, and back road.
- **Route-side collection stays carrier-shaped.** What gets collected is the intercepted actor, convoy, exposed camp-party packet, or directly encountered stop, not generic road-ground bounty.
- **This rule does not answer yield.** It only says route-side skimming is allowed when it stays on-path and bounded; how much haul that creates belongs to micro-item 11.

#### 11. Collection yield rule
- **Question:** How much harvestable bounty can one outing convert into haul or stockpile?
- **Expected output:** One starter yield rule or small table.
- **Done when:** Later stockpile math has a real ingress rule instead of hand-wavy "some amount".

Current answer:
- One outing should convert at most **0-3 haul steps** into camp stockpile, with the top end reserved for unusually rich, low-friction takes.
- Treat that as an **ingress cap**, not a 1:1 readout of bounty score. High structural or moving bounty makes a target worth attempting, but one trip still brings home only a bounded packet.
- On-route opportunism counts inside the same cap. A route-side skim may improve a trip's result, but it does not stack into a free second full haul.
- Any value left behind stays as residual structural or moving bounty, partially harvested state, or a future mission lead rather than being magically vacuumed up by one successful outing.

| Outing outcome | Stockpile ingress | Typical shape | Why |
| --- | --- | --- | --- |
| Empty hand or broken contact | 0 | False lead, hard resistance, panic retreat, or no portable value reached | The outing may still rewrite marks or threat, but it did not bring home material gain. |
| Light skim | 1 | Robbed traveler, clipped convoy edge, quick on-path supplies, or a small exposed stop | This is the normal bounded result for opportunistic route contact or a thin low-risk take. |
| Ordinary successful haul | 2 | Stripped house/farm packet, good camp-edge theft, modest convoy hit, or other worthwhile site take | A decent outing should matter, but still not liquidate an entire rich region in one pass. |
| Exceptional capped haul | 3 | Rich convoy, soft camp footprint, or unusually favorable storehouse/outbuilding hit | Rare upper bound for a clean, high-value, low-disruption success. This is a cap, not the new normal. |

Guardrails from this rule:
- **Bounty score is not stockpile units.** Attraction and ingress stay related, but separate.
- **One outing means one bounded haul packet.** Even when route-side skimming happens, the total return still caps at 3.
- **Route-side contact usually yields 0-1.** Reaching 2 or 3 should require a genuinely strong main take, not ordinary roadside noise.
- **Rich sites need repeat exploitation.** A high-value region may take multiple outings to drain because one mission does not cart away the whole world.
- **This rule still does not answer consumption cadence or forest practicality.** Those stay for micro-items 12 and 13.

#### 12. Camp stockpile consumption rule
- **Question:** What drains camp stockpile, and on what explicit cadence family?
- **Expected output:** One rule packet for stockpile consumption, separate from bounty/threat logic.
- **Done when:** The packet states plainly that stockpile goes down by consumption rather than by hidden bounty-style decay.

Current answer:
- Bandit camp stockpile drains only through **explicit camp-side use**, split into one **daily housekeeping cadence** plus two **event-driven mission costs**.
- Daily housekeeping is the only default recurring drain. It covers the camp simply existing for another day: mouths/basic food use plus ordinary low-grade camp upkeep.
- Dispatch consumes a one-shot issue packet when a group actually leaves: travel rations first, plus ammo or medical issue only when that job plausibly needs them.
- Return consumes a one-shot recovery packet when a group comes home with spent ammo, wounds, or other consumed expedition supplies: rearm, wound treatment, and restock of expended mission consumables.

| Stockpile drain source | Drains stockpile? | Cadence family | Why |
| --- | --- | --- | --- |
| Daily camp mouths/basic upkeep | Yes | Daily drift | Ordinary residents and an occupied camp should consume food and low-grade upkeep even if no mission launches. |
| Job dispatch provisioning | Yes | Event-driven on dispatch | Outings should pay an explicit ration/loadout issue cost when they actually leave, not through fake background bleed. |
| Post-outing rearm/recovery | Yes | Event-driven on mission return | Ammo replacement, wound treatment, and spent expedition consumables should hit stockpile when the outing comes home needing them. |
| Idle passage of time outside the normal camp day | No extra drain | No extra cadence | There is no hidden bounty-style stockpile melt beyond the one daily housekeeping pass. |
| Bounty/threat aging or mark cleanup | No | Never | Map-memory maintenance must not double as fake supply decay. |
| Spoilage, camp construction, or broad repair projects | Not by default in v1 | Future explicit rule only | If later systems want those costs, they must model them openly instead of smuggling them into “consumption”. |

Guardrails from this rule:
- **Stockpile consumption is not passive decay math.** It is a small set of named costs on named cadence families.
- **Daily consumption and mission consumption stay separate.** Mouths/basic camp upkeep happen every day; expedition prep and post-mission replenishment happen only when a group actually leaves or returns.
- **Pressure rises when due costs cannot be covered.** Food/ammo/med pressure should come from these unpaid or thinning buckets, not from a second hidden desperation timer.
- **No silent spoilage or build-cost bleed in v1.** Those can be later systems, but only as explicit added rules.

#### 13. Forest yield rule
- **Question:** What practical value can bandits actually extract from forest, separate from generic low forest bounty?
- **Expected output:** One small forest-yield note with starter numbers or bounded categories.
- **Done when:** "Forest has a little bounty" and "forest can actually feed stockpile a bit" are separated cleanly.

Current answer:
- Pure forest should count as **low-yield, low-density structural opportunity**. It can help stockpile a little, but mostly through food, fuel, and rough material rather than rich general loot.
- A forest outing should usually return **0-1 stockpile step**. Reaching 2 should require the group to hit a distinct embedded site or unusually favorable take inside the woods, and that extra value should mostly be attributed to that site rather than to generic forest.
- Forest yield should mainly ease **food pressure** and **basic camp upkeep burden**, not act as a routine source of meaningful ammo, medicine, or high-grade salvage.
- Repeated forest exploitation should flatten quickly unless a later packet explicitly models renewable hunting, logging, or regrowth. V1 should not let "go to the woods" become infinite offscreen income.

| Forest outing shape | Typical stockpile ingress | Practical value | Why |
| --- | --- | --- | --- |
| Short forage, hunt, or firewood cut | 0-1 | Food, fuel, rough material | This is the default pure-forest take: useful, but thin and labor-heavy. |
| Concealed route pass with no real harvest | 0 | None | Cover/concealment can help a mission without itself paying stockpile. |
| Small trap, cache, or campsite found in the woods | 1-2 | Mixed small supplies | The upside comes from the embedded human-use pocket, not from magical generic forest bounty. |
| Rich cabin, logging stash, or occupied camp hidden in forest | Treat as cabin/site/camp rule first | Site-bound supplies | Once the value is really a distinct site, score it by that site class plus any moving activity, not as plain forest. |

Guardrails from this rule:
- **Low forest bounty now has a bounded stockpile meaning.** "Forest has a little bounty" does not mean forest is a rich generic target.
- **Concealment and moving activity stay separate.** Smoke, lights, camps, routes, or ambush opportunities in forest belong on moving-bounty or site layers.
- **Pure forest mostly feeds food/basic-upkeep relief, not ammo/med abundance.**
- **No infinite woods economy in v1.** If later renewable harvesting loops are wanted, write them explicitly instead of letting generic forest bounty refill forever.

### Package 1 acceptance bar
- all 13 micro-items above exist as separate answers or tables
- structural vs moving bounty is no longer blurred
- harvesting, collection, and consumption language fully replaces passive-decay slop
- later implementation will not need to invent the basic resource law from scratch

### Out of scope for Package 1
- coding the system
- per-bandit inventory simulation
- full faction-economy balance for the whole game

---

## Package 2. Cadence, distance burden, and route-fallback rules

Resolve these as **separate movement/control-law micro-items**.

### E. Movement budget and cadence law

#### 14. Daily movement budget rule
- **Question:** What is the starter daily OMT travel budget for relevant bandit outing types?
- **Expected output:** One small starter table.
- **Done when:** The packet no longer relies on vague phrases like "they can move a few tiles".

Current answer:
- Freeze daily travel as a **job-shaped per-day allowance**, not as a player-distance ring and not as one fresh hop every cadence wake.
- This micro-item only answers how much ground one ordinary day of a given outing can honestly cover before later cadence-spend, distance-burden, return-clock, and cargo/wound rules start trimming it.

| Outing type | Starter daily travel budget (OMT/day) | Why |
| --- | --- | --- |
| Local forage skim / camp-edge opportunism | 1 | Keeps thin woods/roadside skims truly local instead of letting every low-grade food or fuel run become a same-day county tour. |
| Scout / cautious probe / short shadow | 2 | A look-around outing should reach a little beyond the home footprint, but still behave like reconnaissance rather than free map-wide presence. |
| Toll setup / convoy hit / route ambush | 3 | Intercept work needs enough room to reach and work a corridor, but not enough to teleport between several unrelated routes in one day. |
| Ordinary scavenge / steal run | 4 | A committed haul trip can push farther than a probe because it is planned around one worthwhile target, but it should still stay far short of half-map crossing. |
| Raid / hard reinforce / committed strike | 5 | The most serious ordinary hostile outing gets the broadest same-day envelope, while still needing multiple days for deep-theater pressure. |
| Rare explicit theater reposition / emergency redeploy | 6 | Reserve the top end for exceptional movement packets that are more like strategic reposition than the normal day-to-day income loop. |

Guardrails from this rule:
- **This is one per-day allowance, not one allowance per cadence wake.** Exact cadence spend belongs to micro-item 15.
- **This answers outing type, not desirability by range.** Distance burden still belongs to micro-item 16.
- **Later burden rules may shrink usable travel, not silently enlarge it.** Cargo, wounds, panic, and hard caution belong to micro-item 18.
- **Far targets require multiple days or staged pressure.** A same-day raid or convoy hit should not cross half the strategic theater just because the camp noticed it.
- **The 6-OMT row is a rare top-end exception, not the normal economy loop.** Most routine movement should live inside the 1-5 envelope.

#### 15. Cadence budget-spend rule
- **Question:** How do cadence wakes spend existing budget without minting fresh travel budget every pass?
- **Expected output:** One explicit cadence-spend rule.
- **Done when:** The concept cannot accidentally teleport groups across the map just because the AI woke often.

Current answer:
- Treat daily movement budget as an **elapsed-time-earned travel credit**, not as a per-wake grant.
- Each active outing carries three separate movement values:
  - **daily_budget_omt** from micro-item 14
  - **spent_today_omt** or an equivalent remaining-budget counter
  - **fractional_progress_omt** or equivalent carried travel credit
- On each cadence wake, first add only the travel the group has honestly earned since the last update:

```text
earned_travel = daily_budget_omt * elapsed_time_since_last_update / 1 day
available_travel = min( remaining_daily_budget, carried_fractional_progress + earned_travel )
```

- The wake may then spend only that **available travel** along the current route or current search pattern.
- Any unused remainder stays carried forward as fractional progress. Do **not** round every wake up to a whole OMT just because the scheduler looked again.
- The daily budget refreshes only when a real new outing day begins, not because the group hit another 20-minute or 2-hour cadence boundary.

Starter examples:

| Outing type | Daily budget | Honest cadence spend shape | Why this avoids fake travel |
| --- | --- | --- | --- |
| Local forage skim | 1 OMT/day | earns about 1 OMT every 24 hours of elapsed world time | A camp-edge skim stays truly local instead of hopping one full tile every wake. |
| Scout / short shadow | 2 OMT/day | earns about 1 OMT every 12 hours | Recon can keep creeping outward, but only because time passed, not because the AI twitched repeatedly. |
| Ordinary scavenge / steal run | 4 OMT/day | earns about 1 OMT every 6 hours | A serious haul trip can cover ground within a day, but a string of 20-minute wakes still cannot mint half-county teleporting. |
| Raid / hard reinforce | 5 OMT/day | earns about 1 OMT every 4.8 hours | High-commitment hostile jobs move faster, while still needing real elapsed time to cross distance. |
| Rare theater redeploy | 6 OMT/day | earns about 1 OMT every 4 hours | The rare top-end packet is still broad strategic motion, not free instant reposition on every scheduler pass. |

Guardrails from this rule:
- **Cadence is a decision opportunity, not a fuel grant.** Wakes let groups reconsider, observe, divert, or spend already-earned movement. They do not print extra distance.
- **Elapsed world time is what earns travel.** One 6-hour sleep, three 2-hour wakes, or eighteen 20-minute wakes should all expose the same total spendable movement.
- **Fractional travel must carry forward honestly.** If the implementation dislikes raw fractions, it still needs an equivalent partial-progress ledger instead of wake-by-wake rounding gifts.
- **This rule still does not answer desirability falloff, outing endurance, or burden trimming.** Distance burden remains micro-item 16, return clock remains micro-item 17, and cargo/wounds/panic remain micro-item 18.

#### 16. Distance burden rule
- **Question:** How does target desirability fall off with travel distance and return burden?
- **Expected output:** One discount rule or starter table.
- **Done when:** Range pressure is concrete enough to compare two targets honestly.

#### 17. Return-clock rule
- **Question:** How long can a group stay out before it should prefer turning home?
- **Expected output:** One return-clock rule with starter values.
- **Done when:** The packet no longer implies immortal stalking squads that stay out forever.

#### 18. Cargo / wounds / panic burden rule
- **Question:** Which burden factors shorten outings or reduce useful movement?
- **Expected output:** One bounded penalty table or rule note.
- **Done when:** The concept has explicit burden pressure beyond raw distance.

### F. Fallback and diversion law

#### 19. No-target fallback rule
- **Question:** What does a camp do when nothing nearby looks worthwhile?
- **Expected output:** One explicit idle / low-value fallback behavior.
- **Done when:** The system will not inherit current random-NPC wander nonsense by accident.

#### 20. No-path fallback rule
- **Question:** What happens when a target exists but has no sensible reachable route?
- **Expected output:** One explicit no-path rule.
- **Done when:** The packet no longer leaves unreachable targets to implicit engine behavior.

#### 21. Mid-route abort / divert / shadow rule
- **Question:** What can make a group abandon, shadow, probe, or switch away from its original plan mid-route?
- **Expected output:** One bounded diversion rule.
- **Done when:** Mission leads stop looking like sacred tile commitments.

### Package 2 acceptance bar
- all 8 micro-items above exist as separate answers or tables
- movement budget, cadence, distance burden, and return pressure are explicit enough to compare routes honestly
- no-target and no-path behavior is implementation-legible
- the packet does not use passive decay as a crutch for movement cleanup

### Out of scope for Package 2
- replacing existing pathfinding code right now
- exact micro-path cost formulas for every overmap terrain
- local bubble combat behavior

---

## Package 3. Cross-layer interaction packet with starter numbers and worked scenarios

Resolve these as **separate scoring/seam sanity micro-items**.

### G. Job generation and scoring law

#### 22. Job candidate generation rule
- **Question:** What job candidates even get onto the board for a camp to consider?
- **Expected output:** One candidate-generation rule note.
- **Done when:** Later scoring is not secretly deciding eligibility and desirability in one muddy pass.

#### 23. Job scoring formula shape
- **Question:** What are the main weighted factors in job scoring?
- **Expected output:** One explicit formula shape or score table, even if some numbers remain provisional.
- **Done when:** The packet names the factors instead of hiding them in prose.

#### 24. Need-pressure override rule
- **Question:** When does camp need or low stockpile make a mediocre opportunity worth taking anyway?
- **Expected output:** One bounded override rule.
- **Done when:** The model can explain desperate behavior without pretending bandits became stupid.

#### 25. Threat veto vs soft-veto rule
- **Question:** When does threat fully block a job, and when does it merely discount it?
- **Expected output:** One explicit veto ladder.
- **Done when:** The concept no longer hand-waves the difference between "dangerous but worth it" and "absolutely not".

### H. Handoff and persistence law

#### 26. Overmap-to-bubble entry-mode chooser
- **Question:** Which high-level mode should a group enter local play in, and what decides that?
- **Expected output:** One small entry-mode chooser packet covering at least scout, probe, harvest, ambush, raid, shadow, and withdrawal-style cases.
- **Done when:** The handoff seam has explicit mode selection instead of generic spawn-and-vibes.

#### 27. Bubble-to-overmap return-state packet
- **Question:** Which exact fields must come back from bubble play?
- **Expected output:** One return-state field list.
- **Done when:** Cargo, wounds, losses, morale/panic, mark changes, and stockpile delta are named concretely enough to serialize later.

#### 28. Save/load persistence boundary
- **Question:** Which bandit-side state must survive save/load and which may stay abstract?
- **Expected output:** One persistence-boundary note.
- **Done when:** The concept has a real schema boundary instead of a vague "save all the important stuff" gesture.

### I. Sanity-check packet

#### 29. Starter numbers table
- **Question:** What compact starter number sheet should the whole packet use consistently?
- **Expected output:** One cross-cutting table covering the still-open control-law numbers.
- **Done when:** The packet stops scattering provisional numbers across prose.

#### 30. Worked scenarios packet
- **Question:** Do the combined laws still make sense when pushed through concrete situations?
- **Expected output:** At least 8 worked scenarios.
- **Done when:** The packet includes cases such as forest-adjacent low structural bounty with visible activity, harvested houses, scary city edges, long-distance tempting targets, unreachable targets, moving-activity trails, and stockpile-pressure decisions.

#### 31. Invariants and non-goals packet
- **Question:** What must never happen, and what is explicitly out of scope for v1?
- **Expected output:** One invariants/non-goals sheet.
- **Done when:** Later implementation and review can spot nonsense immediately instead of arguing from vibes.

### Package 3 acceptance bar
- all 10 micro-items above exist as separate answers or tables
- starter numbers, worked scenarios, and invariants are explicit enough to pressure-test the whole control law
- stale passive-decay wording is corrected or explicitly flagged in the parked docs
- the result makes the whole bandit packet more implementation-legible without silently greenlighting implementation

### Out of scope for Package 3
- implementation
- balancing every edge case in one pass
- reopening the whole bandit concept from scratch

---

## What done looks like for the whole follow-through

This follow-through is done when:
- the 3 packages above exist as explicit doc/spec slices
- the **31 micro-items** above each have a narrow answer, table, or rule note
- the no-passive-decay footing is normalized across the packet
- later implementation planning can pick the packet up without rediscovering the control law from scratch

If that happens, the parked bandit chain is cleaner.
It is still not automatic permission to code the whole thing. One bureaucratic indignity at a time.
