# Error Recovery Skill

Use this skill when an observation contains `ERROR`, an empty result, or an
unexpected file state.

1. Do not repeat the same failing action without changing the arguments.
2. If a path fails, inspect whether it is relative to the workspace.
3. If JSON parsing fails, produce a simpler one-line `ACTION:` object.
4. If a web request fails, explain the offline limitation and continue with
   local tools when the task permits.
5. If the max step budget is close, write a concise partial result instead of
   looping.
