LLM prompt templates for Cataclysm-AOL
====================================

These files are the bundled default prompts.

User-editable copies are automatically created in:
  config/llm_prompts/

If you want to change the assistant/NPC tone without touching C++ code,
edit the files in config/llm_prompts after the game has used the LLM once.
Missing files are re-seeded from these bundled defaults.

This folder also contains helper-script prompt templates used by:
- tools/llm_runner/background_summarizer.py
- tools/llm_runner/prompt_playground.py

Those helpers also seed editable copies into config/llm_prompts when needed.

This folder also contains Basecamp handoff templates used by deterministic camp
crafting flows, such as:
- basecamp_craft_handoff_snapshot.txt
- basecamp_board_handoff_snapshot.txt
- basecamp_board_handoff_job_line.txt

Important placeholders
----------------------
Keep these placeholders exactly as written if they appear in a file:

- {{snapshot}}
- {{action_list}}
- {{action_list_with_target}}
- {{npc_name}}
- {{player_utterance}}
- {{items_xml}}
- {{inventory_xml}}
- {{story_text_block}}
- {{request_id}}
- {{job_id}}
- {{query}}
- {{count}}
- {{source_utterance}}
- {{request_subject}}
- {{resolved_recipe}}
- {{status}}
- {{approval_state}}
- {{worker}}
- {{blockers}}
- {{details_token}}
- {{next_token}}
- {{active_count}}
- {{archived_count}}
- {{jobs}}

If a prompt file is missing a required placeholder, the game falls back to the
bundled default version instead of using the broken file.

Tips
----
- You can change tone, wording, examples, and style freely.
- Do not change the output format requirements unless you also change the parser.
- The action prompt is the strictest one; be careful with its formatting rules.
