# Notes for Claude

- Vertigo recreates the original game from its data files. Compare with the
  original only by observing its behaviour and reading its data; implement
  every feature independently. The repository records what the original does
  (docs/KNOWLEDGE.md), never how that was found out.
- The original game's programs are never run, built or inspected from this
  repository or on the development machine as part of its work.
- The version is `project(vertigo VERSION …)` in CMakeLists.txt (the main menu,
  `--help` and the iPad app show it). A feature branch raises master's patch
  number by one, e.g. 0.9.0 to 0.9.1 for a branch like fidelity-tuning; if
  master has moved on when the branch merges it, the branch takes the next
  number. 1.0.0 is the feature-complete clone.
- README.md shows screenshots from docs/screenshots, taken by
  docs/screenshots/take.sh (the iPad one by hand, as the script's comment
  says). A change that alters what one of them shows (the main menu, the
  station screens, the depot, the cockpits and HUD, the models' textures,
  the touch controls, the upscaled graphics or the vector font) also retakes
  that screenshot with the script and commits the new image.
