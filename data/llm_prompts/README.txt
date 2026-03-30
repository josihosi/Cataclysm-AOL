LLM prompt templates for Cataclysm-AOL
====================================

These files are the bundled default prompts.

User-editable copies are automatically created in:
  config/llm_prompts/

If you want to change the assistant/NPC tone without touching C++ code,
edit the files in config/llm_prompts after the game has used the LLM once.
Missing files are re-seeded from these bundled defaults.

Important placeholders
----------------------
Keep these placeholders exactly as written if they appear in a file:

- {{snapshot}}
- {{action_list_with_target}}
- {{npc_name}}
- {{player_utterance}}
- {{items_xml}}
- {{inventory_xml}}

If a prompt file is missing a required placeholder, the game falls back to the
bundled default version instead of using the broken file.

Tips
----
- You can change tone, wording, examples, and style freely.
- Do not change the output format requirements unless you also change the parser.
- The action prompt is the strictest one; be careful with its formatting rules.
