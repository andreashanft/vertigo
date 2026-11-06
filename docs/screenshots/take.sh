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
#       strai0 --boat 2050 --cockpit 3d --fixed-step --start 213,191,30,6 \
#       --touch "26=0.13,0.81>0.23,0.81/1.0" --screenshot 30=ipad-moghs.png
#
# The touch drags the on-screen stick right for a second, turning the boat towards
# the Mogh boats (a key or a game controller would hide the touch controls).
#   (wait for the shot, then stop the app: devicectl device info processes / process terminate)
#   xcrun devicectl device copy from --device <udid> --domain-type appDataContainer \
#       --domain-identifier <bundle id> --source Documents/ipad-moghs.png --destination ipad-moghs.png
#
# devicectl takes --timeout as its own option, so the run gets none and has to be
# stopped by hand; a relative --screenshot path lands in the app's Documents folder.
# The iPad must be unlocked, or the launch is refused, and its Game Data Upscaled
# must be the set the build expects (its source.txt starts with the set version,
# else the original art shows); a 3d-cockpits folder in the game data would
# replace the kit cockpit. The JPEG keeps the iPad's 2752x2064 pixels.
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

# the main menu (after the presents picture, which shows for a second)
shot main-menu --screenshot 3=main-menu.png --timeout 4
# Topo's Asylum: a new game taken to the station, without the intro, movies and dialogues
shot topos-asylum --new-game --station st0203 --screenshot 3=topos-asylum.png --timeout 4
# the depot with a Succubus: the open mission's depot
shot depot-succubus --open-mission 2052 --screenshot 3=depot-succubus.png --timeout 4
# the Zorn's cockpit beside the KILLTEK freighter of the second training mission
shot zorn-freighter strai2 --boat 2051 --cockpit 2d --start 168,98,26,0 --fixed-step --screenshot 1.5=zorn-freighter.png --timeout 2

# the help's controller page (its fourth, H four times), with an Xbox controller's letters, cut
# out of the window: the help's box, 54 to 577 across and 92 to 406 down of the 640x480 picture
"$vertigo" --window $size strai0 --cockpit 2d --faces xbox --fixed-step --keys "1=H,1.2=H,1.4=H,1.6=H" \
    --screenshot 2=controller-help.png --timeout 2.2 > /dev/null || [ $? -eq 3 ]
# (the picture fills the window's height or width, centred in the other)
crop=$(sips -g pixelWidth -g pixelHeight controller-help.png | awk '
    /pixelWidth/ { w = $2 } /pixelHeight/ { h = $2 }
    END { s = (w/640 < h/480) ? w/640 : h/480; x = (w - 640*s)/2; y = (h - 480*s)/2
          printf "%d %d --cropOffset %d %d", 314*s, 523*s, y + 92*s, x + 54*s }')
sips -c $crop controller-help.png > /dev/null
sips -s format jpeg -s formatOptions 85 controller-help.png --out controller-help.jpg > /dev/null
rm controller-help.png
echo controller-help.jpg
