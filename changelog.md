# 1.4.50
- FIX: Calculates player clicks and state over all frames since last update (fixes accidentally skipping a frame of click or wave update)
- FIX: Did the same for the ghost waves
# 1.4.48
- FIX: Click and frame alignment fix (was 2 frames off)
- FIX: Wave trail overhaul (all fixes only apply to new recordings)
- FIX: Ghosts now reflect correct P1 P2 colors when different (will fix wave color soon)
- FIX: Glow color now works on ghosts
- Separate dual icons for ghosts sounds annoying to impliment so maybe I'll do that when I have more time
- FIX: Massive fix of the start timestamp for startpos, but requires offset math for pre-update attempts (meaning slower loading for old attempts)
- Be aware old practice runs from start positions no longer work (you must record new ones for now, might be able to fix later)
- FIX: Bunch of other stuff that would take too long to explain
# 1.4.47
- FIX: Wave trail sliding issue on gravity switch
- FIX: Ghost visibility bug in practice playback
- FIX: MASSIVE lag fix when using custom death sounds
# 1.4.46
- FIX: Ghosts not resetting properly on restart (leading to many not showing up after restart)
- FIX: With max ghost offset, negative ghosts could be off the screen, now they wrap around to positive
# 1.4.45
- Preload changes: best mode default, set real player objects in reverse order (earliest deaths appear first)
# 1.4.44
- Sort of fixed some of the camera weirdness with teleporting while replaying, but its still janky
- FIX: Preloading 1 attempt now only does the real player and no ghosts (before it did real player and 1 ghost)
- FIX: Practice replay not loading all needed data
- FIX: "Only attempts that passed N%" fixes
# 1.4.42
- Fix: noclip detection issue with start positions
- Fix: blocking preload popup when no start pos attempts exist
# 1.4.41
- Fix: double clicking issue when not in two player mode in dual
- Fix: ghost wave trail staying visible after ending playback
- Fix: preload issue where it would sometimes not detect some attempts
- Fix: dual ghost death visibility issue
# 1.4.36
- New Toggle: Don't record Noclip Attempts (only works for normal attempts, not practice mode)
# 1.4.33
- Using playback on a level and completing it no longer counts for Death Tracker
- Time warp on death for replays
- Dual Ghost visibility fix
# 1.4.31
- Less recording delay for players who want minimal input delay
# 1.4.30
- Fixed visual bugs for wave trails for ghosts and the player
# 1.4.29
- Only load Attempts into memory when needed
- Way more accurate RAM estimate (includes attempt data + player object size)
# 1.4.28
- Fixed practice session overwriting bug
# 1.4.27
- Can now specify to load the best, most recent, or randomly from attempts
# 1.4.24
- Detection of corrupted attempt files when loading
- Avoids counting level resets done by the mod as attempts
- Full overhaul of the practice recording system
- Fix to make object pooling consistant on reset
# 1.4.23
- Yet another crash fix
# 1.4.22
- Crash fix (thank you MagLight for the help debugging)
- RAM indicator fix
# 1.4.21
- 42.86% less memory used by loaded attempts due to dynamic quantization. Runs 5% slower now :(
# 1.4.20
- UI scaling fix for that one tablet user
- Softlock fix
# 1.4.19
- High attempt count playback ghost primming fix (massive lag reduction on insane replays)
# 1.4.18
- Smaller file size for saves (3.5x smaller using quantization and keyframes with offsets)
- Small practice recording fix
# 1.4.17
- We on Geode now!