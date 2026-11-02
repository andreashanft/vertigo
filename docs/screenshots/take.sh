#!/bin/sh
# Takes the README's screenshots again, so that they show the game as it is.
#
#   docs/screenshots/take.sh [build/vertigo]
#
# Needs a desktop build and the game data with the AI-upscaled set switched on
# (Options: Upscaled graphics), which also draws the menus' silver font from the
# vector font. Each shot is a test-mode run of a few seconds in a 1024x768 window;
# on a high-density display the PNGs come out at 2048x1536, and sips (macOS) turns
# them into the JPEGs the README shows. The in-game shot is reproducible: a fixed
# place and heading, the default seed and fixed time steps.
#
# The iPad shot is taken on an iPad (not the simulator, whose software renderer is
# too slow for the 3D cockpit): the first training mission against the Mogh boats
# in the Gator, as the original's training flies it, at the moment a boat passes
# the city; the bundle id is the one the app was built with (-DVERTIGO_BUNDLE_ID):
#
#   xcrun devicectl device install app --device <udid> build-ios/RelWithDebInfo/Vertigo.app
#   xcrun devicectl device process launch --device <udid> <bundle id> \
#       strai0 --boat 2050 --cockpit 3d --fixed-step --start 213,191,30,6 --screenshot 29.5=ipad-moghs.png
#   (wait for the shot, then stop the app: devicectl device info processes / process terminate)
#   xcrun devicectl device copy from --device <udid> --domain-type appDataContainer \
#       --domain-identifier <bundle id> --source Documents/ipad-moghs.png --destination ipad-moghs.png
#
# devicectl takes --timeout as its own option, so the run gets none and has to be
# stopped by hand; a relative --screenshot path lands in the app's Documents folder.
# The iPad must be unlocked, or the launch is refused. The JPEG keeps the iPad's
# 2752x2064 pixels.
set -e
cd "$(dirname "$0")"
vertigo=${1:-../../build/vertigo}
size=1024x768

shot() {
    name=$1
    shift
    # (a mission run ends with exit code 3, timeout, the menu runs with 0)
    "$vertigo" --window $size "$@" > /dev/null || [ $? -eq 3 ]
    sips -s format jpeg -s formatOptions 85 "$name.png" --out "$name.jpg" > /dev/null
    rm "$name.png"
    echo "$name.jpg"
}

# the main menu (a click ends the title sequence)
shot main-menu --click 1=320,240 --screenshot 3=main-menu.png --timeout 4
# Topo's Asylum: a new game taken to the station, without the intro, movies and dialogues
shot topos-asylum --new-game --station st0203 --screenshot 3=topos-asylum.png --timeout 4
# the depot with a Succubus: the open mission's depot
shot depot-succubus --open-mission 2052 --screenshot 3=depot-succubus.png --timeout 4
# the Zorn's cockpit beside the KILLTEK freighter of the second training mission
shot zorn-freighter strai2 --boat 2051 --cockpit 2d --start 168,98,26,0 --fixed-step --screenshot 1.5=zorn-freighter.png --timeout 2
