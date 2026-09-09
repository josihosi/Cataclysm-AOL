# R-029 sound eligibility result

The retained sound was detected correctly, but it cannot become an eligible structural candidate under the current timing rules.

At minute 8280, the lead was last seen at 8220 and last checked at 8225. Its signal strength was still 399 because it was only 60 minutes old, inside the 180-minute sound lifetime. The recent-check rule rejected it because only 55 minutes had passed and the cooldown requires 360 minutes.

The planner computes temporary fields and a cheap score first. It then applies source, profile, time, camp cooldown, pressure, mission-slot, kind, status, bounty, signal-expiry and recent-check rules. The recent-check rule runs before effective interest, static risk, route construction, policy eligibility and pair selection. The scheduler sees no admitted cheap plan, so aggregate drive 347 is evaluated later and is not the first cause.

Natural sound observation writes `last_checked_minutes` at observation time. Unchanged reads preserve the existing timestamp. This sound expires at minute 8400, while its cooldown ends at 8585. Waiting cannot make the retained lead eligible.

The retained evidence does not establish individual drive components, later interest or risk, route reachability, policy eligibility, an exact pair or final dispatch. No native replay or source change was needed.

Josef's smallest remaining choice is whether distant sensing should start the same cooldown as completed physical investigation. After that decision, the cheapest verification is a focused predicate test for timestamp behavior followed by one saved-lead harness run with an authoritative semantic transition receipt. The firearm should not be replayed merely to detect the same sound again.
