"""Current waiting input state, without a history of full response packets."""
from pathlib import Path


def compact_response(response):
    """Keep only what the waiting client needs to act or establish an outcome."""
    from gameplay_display import observation

    result = {key: response[key] for key in ("ok", "error", "reason", "failure") if key in response}
    observed = observation(response)
    if observed:
        surface = observed["surface"]
        result["observation"] = {
            key: observed[key] for key in ("observation_id", "run_id", "game_minutes", "game_turn")
            if key in observed}
        result["observation"]["surface"] = {
            "kind": surface.get("kind"), "actions": surface.get("actions", []),
            "facts": {key: value for key, value in surface.get("facts", {}).items()
                      if key in {"text", "title", "messages", "activity_type"}},
        }
    terminal = response.get("result")
    if isinstance(terminal, dict) and terminal.get("schema") == "caol-cockpit-live-final-v1":
        result["result"] = {key: terminal[key] for key in ("schema", "state", "cleanup") if key in terminal}
    return result


def retire_collected_packets(session: Path, current_request: str):
    """An admitted successor proves the client has consumed its predecessor.

    Keep the latest response for crash recovery. This applies only to explicitly
    selected plain waiting sessions, whose client stores current input state.
    """
    # The next request may already be spooled once this receipt is published.
    # Only a completed predecessor's receipt authorizes retiring its packet.
    for receipt in (session / "responses").glob("*.receipt.json"):
        request_id = receipt.name.removesuffix(".receipt.json")
        if request_id != current_request:
            for path in (session / "requests" / (request_id + ".json"),
                         session / "responses" / (request_id + ".json"), receipt):
                path.unlink(missing_ok=True)
