# Fuel writeback repair via wood source zone - imagination source of truth (2026-04-29)

Josef's concrete picture: the brazier placement proof is already okay. Stop trying to make the flaky Multidrop filter confess that it can see an exact `2x4` row. Instead, give the game an absurdly visible firewood source: spawn a huge pile of logs or planks, mark a broad source-firewood zone over it, and let ordinary game behavior spill / expose enough fuel that the player-lit fire path can proceed.

The imagined product scene is simple: a real deployed `f_brazier` sits near the player, a large local wood source exists in-world rather than only as a harness promise, the fire-starting action can draw from or see that fuel, the player lights the brazier through normal play, and saved-state inspection shows actual fire/smoke/light fields after save/writeback. Only then may the bandit signal proof claim a real player-created fire source.

This repair is not a license to credit debug-spawned map fields as fire proof. Debug may create the wood pile and zone footing, but the closure claim still belongs to the normal player-action bridge: accessible fuel -> save/writeback -> lighter/action -> `fd_fire`/smoke/light -> bounded wait -> signal response.
