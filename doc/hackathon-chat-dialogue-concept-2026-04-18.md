# Hackathon Chat Dialogue Concept

Date: 2026-04-18

This document is a concept-level project plan for the hackathon chat-dialogue feature.
It is intentionally product-focused rather than code-focused.
It is meant to be read, criticized, and revised quickly.

Please comment by section number where possible, for example:
- `2.3 yes`
- `4.2 no, too restrictive`
- `8.1 add a stretch goal for rumors`

## 1. Goal And Constraints

### 1.1 Primary goal
Replace the visible branch-dialogue feel with a chat-like conversation mode where the player types freeform text and the NPC answers in freeform text.

### 1.2 Core philosophy
The player should feel like they are talking to a living NPC in a sandbox, not filling out a menu.

### 1.3 Stability requirement
The game must not lose real functionality such as:
- trade
- quest or job discussion
- training
- follower control
- ordinary branch-driven game consequences

### 1.4 Demonstration requirement
The feature must be an explicit toggle in the `[LLM]` options menu so the demo can switch between:
- classic dialogue branches
- chat mode

### 1.5 Hackathon constraint
This is not a “perfect final architecture” project.
This is a “make something convincing, stable enough, and fast enough to survive the video” project.

## 2. Current Footing

### 2.1 What the game already has
The current game already has a strong authored dialogue system.
NPC conversations are driven by existing talk topics, conditions, effects, and transitions.
That authored material is valuable and should not be thrown away.

### 2.2 What the current LLM system already does
The current LLM path already knows how to:
- build an NPC-centric snapshot
- include opinions, emotional state, and nearby context
- send a player utterance to the runner
- get back text and action tokens

This means the project is not starting from zero.

### 2.3 Important current split
Right now there are really two different interaction styles:
- normal authored dialogue
- separate LLM freeform/shout behavior

That split matters.
The new hackathon feature should not blindly smash them together.
It should deliberately create a new hybrid:
- freeform visible conversation
- hidden authored control surface

### 2.4 Why that matters
If we replace the authored system completely, we will get chaos fast.
If we only expose the authored system, the feature will feel dead and menu-like.
The hybrid is the middle path.

## 3. Player Experience

### 3.1 Entering conversation
The player walks up to an NPC and starts a normal talk interaction.

If chat mode is enabled, the player does not see the classic response list.
Instead, they see:
- the conversation transcript
- a blank input field

### 3.2 What the player should feel
The player should feel:
- “I can ask this in my own words”
- “I am probing a person, not activating a menu”
- “This NPC has mood, memory, attitude, and some unpredictability”

### 3.3 What the player should try naturally
The player should be able to naturally try things like:
- “Who are you?”
- “What’s your deal?”
- “Can we trade?”
- “Need anything done?”
- “Teach me something.”
- “Come with me.”
- “Stay here.”
- “What happened around here?”

The system should reward this kind of sandbox probing.
For the first demo, the hard priorities are identity, trade, and work or quest-style asks.
Training, follower-control style asks, and similar side paths are lower-priority and should not expand Stage 1 scope.

### 3.4 NPC output style
The visible NPC reply should be freeform.
It may include small physical or environmental beats, for example:
- a glance
- a pause
- a muttered aside
- a shift in tone

But it should stay short and grounded.
It should not become overwritten novel prose.

### 3.5 Expected tone
The world is rough, practical, and unstable.
The writing should sound like survivors making it through one more day, not like a polished assistant, therapist, or RPG lore lecturer.

## 4. Player Workflow

### 4.1 Default workflow
1. Player starts conversation.
2. NPC immediately gives a conversation-opening freeform reply.
3. Player types a freeform line.
4. NPC answers with a freeform line.
5. If the line implies a real game interaction, the hidden backend attempts to perform one.
6. The conversation continues unless a hidden action ends it.

### 4.1.1 Opening beat requirement
The first line matters.
When the player approaches someone, the NPC should not feel like an inert menu waiting for activation.

The opener should therefore be LLM-generated and should usually contain:
- a short physical or environmental beat
- a short spoken line, or a meaningful silence

The opener's job is to set the mood from:
- current emotional state
- opinion of the player
- personality and background
- current danger, fatigue, pain, and nearby situation

This should be brief.
The point is not to deliver a monologue.
The point is to make the NPC feel present before the player says anything.

### 4.2 Example workflow: identity
1. Player asks, “Who are you?”
2. NPC answers in freeform style.
3. Hidden backend may use an authored “personal info” or similar branch if available.
4. If no strong backend action exists, the NPC still answers conversationally and the conversation state remains stable.

### 4.3 Example workflow: trade
1. Player asks, “Can we trade?”
2. NPC gives a freeform answer.
3. Hidden backend triggers the real trade path if available.
4. If trade is not available, the reply stays conversational and no fake trade is invented.

### 4.4 Example workflow: quest or work
1. Player asks, “Got anything that needs doing?”
2. NPC gives a natural answer.
3. Hidden backend uses the real authored mission/job path if one exists.
4. If no such path exists, the NPC may still answer in-character without pretending the game created a new real quest.

### 4.5 Example workflow: weird sandbox input
1. Player asks something unexpected.
2. NPC answers freely.
3. If no clear real action applies, the system performs no hidden side-effect.
4. The conversation remains alive instead of failing hard.

This matters a lot for the sandbox feel.

## 5. Backend Concept

### 5.1 Core backend idea
Visible text should be freeform.
Hidden state should remain constrained.

So each turn should be treated as two separate layers:
- visible dialogue output
- hidden game action or state transition

### 5.2 Hidden state machine
The existing authored dialogue system should remain the underlying state machine.
That means:
- current topic still matters
- current legal responses still matter
- current effects and transitions still matter

But those things become hidden support machinery, not visible menu content.

### 5.3 The missing problem: writing injection
The hidden state machine solves legality and consequences.
It does not by itself solve tone, authored voice, or plot flavor.

So the backend also needs a deliberate writing-injection stack.
This is where authored dialogue and character-writing material get turned into usable prompt context without dumping a giant raw JSON swamp into the model.

### 5.4 Recommended writing-injection stack
The recommended stack is:
1. live state layer
2. short runtime summary layer
3. medium voice/story card layer
4. active authored context layer
5. recent thematic memory layer

This is the core recommendation of the concept plan.

### 5.5 Live state layer
This is the most important layer because it is the current truth.
It should include:
- current NPC snapshot
- current mood/emotions/opinion
- current world situation
- current conversation state
- current legal hidden actions

If this layer is weak, the NPC will feel disconnected from the actual moment.

### 5.6 Short runtime summary layer
The existing short summaries in the repo are still useful.
They should stay as the thin baseline character anchor.

Their job is:
- background identity
- broad personality grounding
- a light voice anchor

Their job is not:
- carrying the whole plot
- carrying every named-NPC detail
- replacing authored dialogue context

### 5.7 Medium voice/story card layer
Yes, this plan still wants personality/story cards.
But they should be medium cards, not huge mushy summaries.

Their job is:
- stronger authored voice
- stronger named-character identity
- memorable habits of speech
- key life-view and story facts that matter conversationally

These cards should be compact enough that a human can review them and the model can still use them cleanly.

### 5.8 Do we need to generate those cards?
Probably yes, but selectively.

Recommended policy:
- generic or minor NPCs: use the existing short summary only
- important archetypes: use short summary plus one medium voice/story card
- named or demo-important NPCs: strongly prefer a medium voice/story card

So the answer is not "generate giant summaries for everyone."
The answer is "generate or curate stronger medium cards where they buy real demo value."

### 5.9 Active authored context layer
This layer is the immediate authored dialogue injection for the current moment.
It should be built from:
- the current authored NPC line
- the current legal branch responses
- nearby relevant authored topic material when useful
- any current mission/job/conversation facts immediately tied to this pocket of dialogue

This is how the model stays connected to the authored writing and plot logic without being forced to copy it word-for-word.

### 5.10 Recent thematic memory layer
This layer should be small and recent.
It should track things like:
- what the player just asked
- what the NPC already admitted, refused, or implied
- whether tension is rising or dropping
- any short-lived conversational thread that should persist for a few turns

This layer is important for continuity, but it should not replace the voice/story card or the authored context layer.

### 5.11 What not to inject
Do not start with:
- a giant raw JSON monolith of all talk topics
- giant long summaries for every NPC
- a purely memory-driven conversation model with no authored anchor

Those are slow, noisy, and likely to produce mush.

### 5.12 What the backend sends to the model
Each turn, the model should get a packet built from:
- the live state layer
- the player’s latest utterance
- recent thematic memory
- the current authored NPC line
- the current legal authored responses as hidden affordances
- the short runtime summary
- the medium voice/story card if one exists
- active authored context for the current dialogue pocket

Not everything has to be shown literally to the model as giant raw dumps.
It should be structured, bounded, and readable.

### 5.13 What the model returns
The model should return something like:
- `say`: the freeform visible reply
- `tool` or `action`: an optional hidden branch selection or other backend action

The critical point is this:
the text is freeform, but the side-effect channel is bounded.

### 5.14 Why this is the right compromise
This allows:
- lively NPC writing
- interpretation of weird player wording
- use of mood, tone, and context

while still preserving:
- real trade
- real quests/jobs
- real training
- real follow/hold type actions
- real existing dialogue consequences

## 6. Hidden Tooling Philosophy

### 6.1 What a hidden tool means here
A hidden tool is not a separate magical AI capability.
It is just a real currently-legal game action or branch that the model is allowed to invoke under the hood.

### 6.2 Why we need hidden tools at all
If the model only talks and never invokes real backend actions, the feature becomes fake quickly.
The player will ask to trade, ask for work, ask to follow, and the NPC will only improvise about it.
That is not enough.

### 6.3 Why hidden tools should stay narrow in v1
For the first version, the hidden action layer should stay conservative:
- at most one meaningful stateful action per player turn
- only from what is genuinely legal right now
- no broad future-branch planning

That keeps the system understandable and hard to break.
This remains the recommended v1 rule after review.

### 6.4 Desired model behavior
The model should:
- speak freely
- choose a hidden action when clearly appropriate
- avoid forcing an action every turn

The model should not:
- invent unavailable backend behavior
- silently manufacture quests, trade, or state changes

## 7. Writing And Prompting Direction

### 7.1 Writing goals
The writing should feel:
- human
- rough
- local
- a little reactive
- not too clean

### 7.2 Stage direction policy
Small physical or environmental notes are good.
Examples:
- “He rubs his jaw.”
- “She glances past you toward the road.”
- “He gives your rifle a long look before answering.”

But these should be used sparingly.
The system should not turn every line into screenplay markup.
The practical target is "enough to count, no bloat."
We should spend a little token budget on context if it buys mood, presence, or situational read, but not on generic filler.

### 7.3 Emotional grounding
The model should use the existing snapshot to express:
- trust or distrust
- confidence or fear
- pain or exhaustion
- urgency
- familiarity with the player

This is one of the major advantages of using the existing LLM snapshot system instead of treating authored dialogue as a pure menu.

### 7.4 Authored dialogue as source material
The existing dialogue branches should still influence:
- what the NPC can realistically talk about
- what specific concrete interactions are available
- how personality and authored voice remain anchored

But the visible wording does not need to copy the authored branch text exactly.
Slight deviation is acceptable and even desirable.

### 7.5 Prompt bar
The prompt should strongly forbid:
- assistant-like meta explanations
- modern chatbot politeness style
- bullet-point answers in ordinary speech
- overly long monologues
- fake backend promises

### 7.6 Naturalistic conversation guide
The prompt should explicitly encourage:
- short to medium replies
- partial answers
- suspicion, reluctance, or rough humor when appropriate
- occasional short questions back to the player
- slight deviation from authored wording when it improves naturalness

The prompt should explicitly discourage:
- overexplaining the world
- sounding like a lore wiki
- always being cooperative
- always being emotionally tidy
- repeating the same beat structure every turn

### 7.7 Prompt-file reviewability
This chat feature should follow the same prompt-file philosophy as the rest of the LLM system.

That means the important prompt pieces should live in accessible text files in the prompt directory structure, not be hidden only in code.

The practical goal is:
- Josef can open the prompt text directly
- Josef can review how the system prompt is actually put together
- the chat feature fits the same maintainable pattern as the existing LLM prompt files

### 7.8 Prompt-file structure recommendation
Recommended high-level prompt pieces:
- a chat conversation prompt
- an optional conversation-opener prompt or opener section
- a small README note explaining what each file is for

Recommended file-home philosophy:
- bundled prompt text should live alongside the other LLM prompt files
- local overrides should follow the same existing prompt override pattern

Reasonable example filenames for the final implementation would be:
- `npc_dialogue_chat_prompt.txt`
- `npc_dialogue_chat_opening_prompt.txt`
- or one main prompt plus an explicitly marked opener section

The final implementation can decide whether the opener is a separate prompt file or a dedicated prompt section inside the main chat prompt.
The important concept requirement is reviewability and override-friendliness, not one exact file split.

### 7.9 Dedicated logging requirement
This chat pipeline should have its own dedicated log file in the same config folder area as the current LLM logs, but not in `config/llm_intent.log`.

The concept requirement is a separate chat log so the pipeline can be reviewed cleanly without mixing it into the ordinary intent log stream.

A reasonable target shape would be something like:
- `config/llm_dialogue_chat.log`

The log should capture at minimum:
- raw assembled input packet
- raw prompt text or prompt sections used for the turn
- raw model output
- resolved hidden action if any
- fallback reason if chat routing fails or declines to act

### 7.10 Why this logging matters
The chat feature will be prompt-sensitive.
Without a clean raw-input/raw-output log, debugging will be guesswork.

We want the chat pipeline to be inspectable enough that:
- bad prose can be traced back to prompt context
- weird tool choices can be traced back to the hidden affordance packet
- failures can be debugged fast during the hackathon

## 8. Stability Rules

### 8.1 Default-safe rule
If the freeform reply is good but no hidden action is valid, the system should still keep the conversation going.

That is better than brittle refusal.

### 8.2 Side-effect safety rule
If a hidden action is invalid, ambiguous, or unavailable:
- do not perform it
- keep the freeform visible reply if it is acceptable
- stay in a safe conversation state

### 8.3 Hard fallback rule
If chat mode is disabled, or the LLM path fails too badly, the old branch dialogue must still be available.

That protects the demo and protects the game.

### 8.4 Scope rule
The system does not need to solve every possible dialogue edge case in the first hackathon pass.
It needs to feel alive and not catastrophically break obvious interactions.

## 9. Development Workflow

### 9.1 Overall workflow principle
We should work in narrow vertical slices that produce a real visible checkpoint quickly.

### 9.2 Recommended milestone order
1. Finalize the concept and response contract.
2. Define the prompt-file structure and dedicated chat logging contract.
3. Add the options toggle and safe fallback behavior.
4. Replace visible branch selection with blank-slate chat input plus the opening beat.
5. Implement one-turn freeform reply plus optional hidden action selection.
6. Make identity / trade / work interactions feel acceptable, and let rumors/background land softly.
7. Only then consider polish or stretch behavior.

### 9.3 What not to do early
Do not start with:
- massive raw JSON ingestion experiments
- elegant general dialogue architecture refactors
- broad branch summarization pipelines
- polishing every writing edge case

That is too slow for the hackathon.

### 9.4 Product workflow while testing
We should keep checking this from the player’s point of view:
- “Can I just type a thing?”
- “Does the NPC feel alive?”
- “Does trade or work still actually function?”
- “If I say something strange, does it fail softly?”

## 10. Compile Minimization And Speed

### 10.1 Compile policy
Do not compile by ritual.
Compile only at a real checkpoint.

### 10.2 First checkpoint rule
The first compile should happen only after the first coherent feature slice exists on disk, not after every tiny edit.

### 10.3 Narrow compile rule
Use the narrowest honest compile possible first.
Prefer changed-object compilation over a full rebuild whenever the dependency surface allows it.

### 10.4 Full build rule
Only run a broader build when:
- the checkpoint touches broad headers or shared infrastructure
- narrow compilation stops being trustworthy
- we are preparing a real handoff or demo milestone

### 10.5 Practical speed mindset
To stay fast:
- batch edits before compiling
- keep the first milestone narrow
- avoid speculative refactors
- do not spread the first pass across too many files
- checkpoint before the tree turns into soup

### 10.6 Why this matters
The hackathon window is short.
A feature that compiles once and demos well beats a theoretically cleaner version that burns the entire session on rebuilds.

## 11. Recommended V1 Scope

### 11.1 What v1 should absolutely do
- chat-mode toggle in `[LLM]`
- conversation-opening LLM beat when talk starts
- freeform player input
- freeform NPC replies
- hidden use of real backend dialogue actions where appropriate
- dedicated prompt files that are easy to inspect
- dedicated chat logging separate from `llm_intent.log`
- stable fallback to ordinary branch dialogue

### 11.2 What v1 should probably do
- short physical-beat writing
- visible support for identity, trade, and work/quest style asks
- use of snapshot emotions and opinion in the reply tone
- use existing short summaries plus selective medium voice/story cards
- allow some rumors/background texture when it helps the demo, even if that layer is softer and less grounded than the core action paths

### 11.3 What v1 should probably not prioritize
- follower control as a major first-demo focus
- complex multi-action turns
- streaming or inline chat UI if that endangers the first milestone

### 11.4 What v1 does not need
- perfect branch coverage
- perfect prose
- universal action support
- grand unified AI conversation architecture

## 12. Stretch Goals If We Move Fast

### 12.1 Better branch-aware writing
Allow the model to use a slightly richer summary of nearby authored branch content, not just the immediate current legal surface.

### 12.2 Better memory
Improve short conversation memory so the NPC feels more continuous over several turns.

### 12.3 More expressive environmental beats
Let the reply lightly reflect:
- nearby danger
- visible gear
- camp context
- injuries

### 12.4 More hidden actions
After the first version is stable, widen the hidden action layer carefully.

## 13. Immediate Decision Packet

### 13.1 Recommended direction
The recommended direction is:
- freeform visible chat
- hidden authored state machine
- optional hidden action per turn
- strict fallback path

### 13.2 Why this should be the first build
It is the best balance of:
- “feels alive”
- “still uses real game systems”
- “can be built in hackathon time”
- “low enough risk for a demo”

### 13.3 Review answers locked in
- visible NPC replies should always be freeform
- physical or environmental beats should be present enough to matter, but not bloated
- one hidden action per turn is the right v1 rule
- demo priorities are:
  - identity: highest priority
  - trade: highest priority
  - work/quests: highest priority
  - rumors/background: useful but softer priority
  - follower control: out of scope for the first demo

### 13.4 UI and debugging decisions locked in
- Stage 1 input can use popup-style text entry if that is the fast safe path
- streaming or a truer "someone is talking to you right now" feel is desirable, but belongs to Stage 2 unless it turns out to be unexpectedly cheap
- the dedicated `llm_dialogue_chat.log` should be always-on during the hackathon unless there is a concrete reason to gate it later
