# Desk Display

A desk screen for the ESP32 "Cheap Yellow Display" (ESP32-2432S028R). It plugs into a Windows PC or a Mac with a USB cable and has seven apps:

- **Music:** album art, title and artist, a scrub gauge, and prev / play-pause / next buttons. On Windows it follows whatever Windows shows in its media controls (Apple Music, Spotify, a browser tab). On a Mac it follows the Music app (Apple Music). The screen can take its colors from the album cover. Left alone while a song plays, it turns into a full-screen cover with visualizer bars.
- **Clock:** time and date. In the Full color look it sits on a moving sky, with the sun or moon crossing it through the day.
- **Weather:** current conditions and the next two days for your city.
- **Reminders:** a checklist you keep in a text file on the computer, with optional timed alerts.
- **Calendar:** the next 7 days from a Notion database, with an alert 10 minutes before timed items.
- **System:** your computer's CPU, memory, disk and temperatures, plus the board's own.
- **Settings:** options for each app, and the display's look.

The display has **two looks**, switched in Settings: **Minimal** is the amber design on every screen. **Full color** gives each app its own color, puts an animated sky behind the weather and the clock, turns that sky orange and pink at sunrise and sunset, and colors the Music screen from the album cover.

After a few minutes without a touch, a **screensaver** takes over: the album cover drifting around while music plays, otherwise a sky with a small clock.

**Swipe down anywhere to open the menu, then tap an app.** Swipe up to close the menu without choosing. On the Music screen, start the swipe on the art or the text, since the gauge and buttons react to touch right away. The display remembers the last app you used.

## What's in this folder

| Folder | What it is |
| --- | --- |
| `firmware/prebuilt/` | Ready-made firmware files. Use these to get going without installing anything. |
| `firmware/DeskDisplay/` | The firmware source. `ui.h` holds the colors, fonts and the Music layout. `apps.h` holds the menu and the other apps. `scene.h` holds the animated sky. |
| `companion/` | The app that runs on your computer and talks to the display. |
| `companion/settings.json` | Your weather city and your Notion details for the calendar. |
| `companion/reminders.txt` | Your reminders, one per line. |
| `companion/requirements-visualizer.txt` | Two extra Windows packages that let the visualizer hear the computer's sound. Installed by `start_windows.bat`. |
| `preview.png` | What the screens look like, rendered from the real firmware code. |

## Step 1: Put the firmware on the board

**Quick way, no installs (Chrome or Edge on a computer):**

1. Plug the board in with a USB data cable. Use a cable with a regular USB-A plug on the computer end, or a USB-C to USB-A adapter.
2. Open https://espressif.github.io/esptool-js/ and connect to the board's port. If it can't connect, hold the BOOT button on the back of the board while you click connect.
3. Set the flash address to `0x0`, choose `firmware/prebuilt/deskdisplay_auto.bin`, and program it.
4. Close that browser tab, then unplug the board and plug it back in.

If the port doesn't show up on Windows, install the CH340 USB driver (search "CH340 driver WCH").

**From source (needed once you start restyling):**

1. Install the Arduino IDE.
2. Boards Manager: install "esp32 by Espressif Systems". Library Manager: install "LovyanGFX".
3. Open `firmware/DeskDisplay/DeskDisplay.ino`, pick the board "ESP32 Dev Module" and the board's port, then Upload.

## Step 2: First start

The first time it powers on, the screen asks you to press the tip of an arrow in each corner. This lines up the touchscreen. Use a fingernail or the stylus. After that it shows NO SIGNAL until the companion app is running.

To redo the touch setup later, hold a finger on the screen while you plug the board in.

## Step 3: Run the companion app

**Windows** (needs Python 3.11 or newer from python.org; tick "Add python.exe to PATH" in the installer):
double-click `companion/start_windows.bat`.

**Mac** (needs Python 3 from python.org):
right-click `companion/start_mac.command` and choose Open. The first time, macOS asks whether Terminal may control Music. Choose Allow.

The first run sets itself up, which takes a minute. After that it finds the display by itself. Leave the window open while you use the display. Every app except Settings gets its data from the companion.

**Test without your own data:** add `--demo`, for example `start_windows.bat --demo`. It shows three fake tracks, a made-up calendar, and made-up weather that changes every 8 seconds so you can see each animated sky: clear, sunset, partly cloudy, rain, sunrise, thunderstorm, snow, fog, cloudy and a clear night. The visualizer bars follow a made-up pattern in demo mode. Each fake cover has four small squares in its top-left corner: red, green, blue, white, left to right. If your screen shows them in other colors, see "Colors look wrong" below.

Other options: `--port COM5` to choose the port yourself, `--app any` (Windows) to follow whatever Windows is playing instead of only Apple Music (the default), `--no-online-art` to stop cover lookups (see below).

## The Settings app

Settings live on the display and are saved on the board. Tap a row to change it. Tap the bottom strip for the next page. There are three. Everything below starts on, except where the table says otherwise, and temperatures start in Celsius.

| App | Setting | Choices |
| --- | --- | --- |
| Display | Look | Minimal or Full color. See "The two looks" below. |
| Music | Album colors | In the Full color look: on takes the Music screen's colors from each album cover, off keeps it amber. It has no effect in the Minimal look. |
| Music | Full-screen cover | AUTO: after 12 seconds without a touch while a song plays, the controls give way to the full-screen cover, and tapping the cover brings it up too. ON TAP: only when you tap the cover. OFF: never. Starts on AUTO. |
| Music | Visualizer bars | The bars along the bottom of the full-screen cover. With them off, the cover and title sit a little lower and the companion stops listening to the computer's sound. |
| Clock | Time format | 12 H or 24 H. Times in Weather, Reminders and Calendar follow it too. |
| Clock | Moving sky | In the Full color look: on puts the time on the animated sky with the sun or moon crossing it. Off goes back to the plain clock, which still changes color with the time of day. |
| Weather | Temperature | C or F. Starts on C. Wind follows it: km/h with C, mph with F. |
| Weather | Sunrise and sunset colors | In the Full color look: on turns the sky orange and pink around sunrise and sunset in Weather, the Clock and the screensaver. Off keeps those skies blue by day and dark at night. |
| Reminders | Timed alerts | On or off |
| Calendar | Alert before events | On or off |
| System | Temperature | C or F. Starts on C. |
| Display | Brightness | 25, 50, 75 or 100% |
| Display | Screensaver | Off, or after 1, 5 or 15 minutes without a touch. Starts on 5. |
| Display | Alert light on the back | On: the light on the back of the board blinks yellow while an alert is on screen. |

## The two looks

**Minimal** is the amber-on-black design on every screen. Weather shows a simple sun or cloud icon.

**Full color** changes every screen:

- **Weather** becomes a full-screen animated sky with the temperature, the conditions and the high and low on top. Sunny is a blue sky with a turning sun. Rain has drifting clouds and falling pixel rain. A thunderstorm has darker clouds, heavier rain and lightning that flashes the whole sky. There are also skies for partly cloudy, cloudy, fog, snow, and clear nights with a moon and blinking stars. For about half an hour around your city's real sunrise and sunset, a clear or partly cloudy sky turns indigo, salmon and peach with an orange sun, and a cloudy one turns mauve.
  - **Swipe up** for the details: feels like, wind, humidity and the next days, shown over a blurred, darkened copy of the sky. The sky holds still while the details are up.
  - **Swipe down** to go back to the moving sky. Swipe down again to open the menu.
- **Music** takes its colors from the album cover (unless you turn Album colors off), and the full-screen cover sits on a blurred copy of itself.
- **Clock** puts the time on the same moving sky as the weather: today's weather, the sunrise and sunset colors, and stars at night. The sun rises at the left edge, is highest at midday and sets at the right. At night the moon makes the same trip. Turn off Moving sky in Settings to get the plain clock back.
- **Reminders, Calendar, System and Settings** each have their own color, and the menu shows every tile in its app's color.

Alerts and the startup screens stay amber in both looks.

**Nothing playing?** The Music screen shows an **OPEN APPLE MUSIC** button. Tap it and the computer opens Apple Music (or brings it to the front), so you can start something from there.

## Music: full-screen cover and visualizer

Tap the cover on the Music screen and the cover view takes over: the cover in the middle, the title and artist under it, and 16 bars along the bottom that move with the song in the album's color. In the Full color look the background is a blurred, darkened copy of the cover. In Minimal it is black with amber bars. Tap anywhere to get the controls back. Swipe down for the menu as usual.

With Full-screen cover on AUTO in Settings (the starting choice), the Music screen switches to this view by itself after 12 seconds without a touch while a song plays. ON TAP keeps the tap and drops the automatic switch. OFF turns the view off completely. Visualizer bars can be turned off on its own.

**What the bars follow:**

- **Windows:** the sound the computer is playing. The companion listens to the sound on its way to your speakers or headphones (no microphone is involved), so the bars react to everything the computer plays, not only the music app. This needs two extra packages that `start_windows.bat` installs by itself. If that install fails, everything else still works and the bars fall back to the pattern below.
- **Mac:** macOS has no built-in way for an app to hear the computer's own sound, so the bars follow a made-up pattern. It moves in time but does not match the song.

**Bars ahead of the sound?** Bluetooth headphones and some speakers play sound a moment late. Set `"visualizer_delay_ms"` in `companion/settings.json` (for example 200) to hold the bars back by that many milliseconds, then restart the companion.

The companion window says which one is in use when it starts: "Visualizer: following the sound this computer is playing" or "...the bars follow a made-up pattern".

## Screensaver

After the time set in Settings (5 minutes to start with) without a touch, the screensaver takes over whatever app is showing:

- **While a song plays:** no screensaver. The display switches to the Music app's full-screen cover, with the visualizer bars, and stays there. (With Full-screen cover set to OFF, it shows the music controls instead.)
- **Otherwise:** the sky for the current weather and time of day, so rain when it rains and stars at night, with a small clock drifting around it. In Minimal it is a black sky with amber stars and an amber clock.

The first touch only wakes the screen and does nothing else. An alert wakes it too. The screensaver does not start while the display shows NO SIGNAL.

## Weather

Set `"city"` in `companion/settings.json`, then restart the companion. It is set to Culver City. If the wrong place comes up, add the state or country, like `"Springfield, Illinois"`.

## Reminders

Reminders are in `companion/reminders.txt`. Write one per line and save; the display updates within a second or two.

- Tap a reminder on the display to check it off. That puts `[x]` in front of the line in the file. Tap it again to undo. Delete the line when you no longer want it listed.
- Start a line with a time, like `3:30pm Pick up prints` or `15:30 Pick up prints`, and the display shows a full-screen alert at that time today. Tap the screen to dismiss it.
- The display shows the first ten reminders, five per page. Tap the bottom strip for the next page.

## Calendar (Notion)

The calendar reads one Notion database. It only reads; it never changes anything in Notion. Rows with a ticked Done checkbox or a Done status are left out.

`settings.json` already has `"notion_database_id"` filled in with your To Do List database, the one your Calendar page shows (tasks plus the calendar events your sync brings in). To use another database, open it in Notion as a full page, copy the link, and paste the link or its id there.

The one thing you need to add is a token, which lets the companion read that database:

1. Go to https://www.notion.so/profile/integrations and create a new **internal** integration. Name it anything, for example "Desk Display". Read access is all it needs.
2. Copy its secret (it starts with `ntn_`) and paste it between the quotes after `"notion_token"` in `settings.json`.
3. In Notion, open the To Do List database, click the `...` menu at the top right, choose Connections, and add your integration.
4. Restart the companion. Its window says which date column it is reading.

The token stays in `settings.json` on your computer and is sent only to Notion. Treat it like a password: don't share that file.

The calendar refreshes every 5 minutes. If the database has more than one date column and it picks the wrong one, add `"notion_date_property": "Column name"` to `settings.json`.

## System

CPU, memory and disk work on both Windows and Mac. Temperatures depend on the computer:

- **Windows:** CPU and graphics temperature show up while LibreHardwareMonitor (a free app) is running. Without it, you still get graphics temperature if you have an NVIDIA card.
- **Mac:** macOS doesn't give temperatures to ordinary apps, so they show N/A.
- **The board's own chip:** many of these boards have no working temperature sensor. If yours doesn't, CHIP TEMP shows N/A.

## Good to know

- **Weather lookups:** the companion gets the weather from Open-Meteo, a free service that needs no account. It sends your city name, then that city's coordinates.
- **Cover lookups:** when the music app has no picture for a track, the companion looks the cover up with Apple's public iTunes Search, which sends the artist and title to Apple. `--no-online-art` turns this off.
- **Letters:** the screen fonts cover basic Latin letters. Accents are simplified (Björk shows as Bjork) and other scripts show as `?`.
- **Scrubbing on Windows** works only if the music app allows seeking through Windows' media controls. If it doesn't, the gauge still shows progress but ignores drags.
- **Alerts** (reminders and calendar) only appear while the companion is running.
- **Sunrise and sunset** times come with the weather. Until the weather has loaded, the sky assumes 6:30 AM and 6:30 PM.
- **The visualizer on Windows** follows the device Windows is playing sound through when the companion starts. If you switch to other speakers or headphones and the bars stop, they pick up again within about 15 seconds of the music playing.

## If something's off

- **Screen stays black, red light blinking on the back:** the board couldn't detect its screen chip. Flash `deskdisplay_st7789.bin` (boards with two USB ports) or `deskdisplay_ili9341.bin` (one port) instead. From source, set `DISPLAY_DRIVER` at the top of `DeskDisplay.ino`.
- **Colors look wrong or inverted:** same fix, try the other of those two files.
- **Picture is upside down:** change `SCREEN_ROTATION` from 1 to 3 in `DeskDisplay.ino` and upload from source.
- **Touches land in the wrong place:** redo the touch setup (hold the screen while plugging in).
- **Swipes don't open the menu:** press firmly and drag slowly, about a third of the screen height. If it's still unreliable, lower `SWIPE_MIN` in `DeskDisplay.ino`.
- **Weather or Calendar shows a message instead of data:** the message says what's wrong (no city or token set, Notion can't see the database, no internet). The companion window has more detail, and `--verbose` adds to it.
- **Companion says it can't open the port:** close the web flasher tab or the Arduino serial monitor. Only one program can use the port at a time.
- **Visualizer bars don't match the music on Windows:** check the Visualizer line near the top of the companion window. If it says the bars follow a made-up pattern, run `start_windows.bat --verbose` to see why the computer's sound couldn't be opened.
- **The sky animation or the floating cover looks slow:** this is the part most likely to need tuning on the real board. `SCENE_FRAME_MS` at the top of `scene.h` sets how often the sky is redrawn.
- **Stuck on NO SIGNAL:** check the companion window. It says whether it found the display.

## Restyling

Open `firmware/DeskDisplay/ui.h`. The THEME block at the top has the five colors as ordinary hex values and the four fonts, and every screen uses them. The LAYOUT block has the Music screen's positions and sizes. The menu and the other apps are drawn in `apps.h`. Upload again to see changes.

For the Full color look: `paletteFromHS` in `ui.h` turns one hue into a full set of colors (how dark the background is, how bright the accent is), and `paletteFromArt` picks that hue from an album cover. Each app's hue is in `APP_HUE` near the top of `apps.h`. The sky colors for each kind of weather are in `sceneColors` in `scene.h`. How dark the blurred sky gets behind the details is the number passed to `sceneBlurToScreen` in `apps.h` (45 means it keeps 45% of its brightness). `GLOW_MINUTES` in `apps.h` is how long the sunrise and sunset colors last on each side. For the full-screen cover, `ARTV_KEEP` in `ui.h` is how bright the blurred cover stays, and the `VIZ_` values next to it set the size of the bars. `ART_IDLE_MS` near the top of `DeskDisplay.ino` is the 12-second wait before the cover takes over.

## Running it without a window (Windows)

Double-click `companion\start_tray.bat` instead of `start_windows.bat`. A black window flashes for a moment, then the companion runs as a small amber play icon by the clock in the bottom-right corner (click the ^ arrow there if it's hidden). Right-click the icon to see what it's doing, open its log, or quit. Everything the window would have printed goes to `companion\companion.log`.

To start it automatically when you sign in, press Win + R, type `shell:startup`, and put a shortcut to `start_tray.bat` in the folder that opens.

Run `start_windows.bat` once first after any update, so new parts get installed, and use it with `--verbose` whenever something needs troubleshooting.

## What has been checked

Built without the hardware, so here is exactly what was and wasn't tested.

Checked:
- The firmware compiles for the ESP32 with all three screen-chip settings.
- The real firmware code was run in a simulator on a computer, connected to the real companion app in demo mode, with scripted touches. That covered the music controls and scrubbing, the menu, all seven apps, checking off a reminder, paging lists, an alert with the back light blinking, every kind of setting, and both looks: the full-screen sky changing between weathers, swiping up for the details on the blurred sky and back down, each app's colors, and switching to Minimal and back.
- The same simulator covered the newer parts: the Music screen turning into the full-screen cover after sitting untouched, tapping it back to the controls, tapping the cover to bring it up again, the bars moving from the companion's levels, the floating-cover screensaver starting and a touch waking it without pressing anything, skipping a track, pausing, the clock on its sky, the sky screensaver once the music was paused, and the Screensaver setting changing.
- A second simulator run fed the firmware messages directly to reach cases the demo can't: a new track arriving while the full-screen cover or the floating cover is up (before and after its picture arrives), music pausing during the screensaver, an alert arriving during the screensaver, music stopping, and the computer going quiet. It was run in both looks.
- Each of the new on/off settings was checked: the Settings pages, what each choice draws (the cover view without bars, the sky without sunset colors, the plain clock), and, with the real companion, that turning the bars off tells the companion and it stops sending sound levels. Weather arrives in Celsius from the start.
- The sunrise and sunset rule was checked minute by minute for a full day, including that the sun and moon stay on screen and only move left to right. Weather sent without sun times (the older format) still works.
- The code that turns sound into bars was fed pure tones and a mix: a low tone lights the low bars, a high tone the high bars, silence shows nothing, and a quiet song still fills the bars. The Windows listening code was run against a stand-in for the sound package, including losing the sound and reopening it.
- The Notion calendar code was run against made-up rows in the format of Notion's API, using the column names from your To Do List database.
- The weather code was run against a made-up response in Open-Meteo's documented format, including the sunrise and sunset times.
- The companion's Windows and Mac music code was run against stand-ins for the system media APIs.
- CPU, memory and disk readings were run for real, on Linux.

`preview.png` comes from these runs. It is a set of still frames, so it can't show the motion.

Not checked yet, so expect some adjusting when the board arrives:
- Anything on the physical board: screen detection, touch accuracy, USB speed, how well swipes register, the back light, the chip temperature reading, and how long the blur takes to draw.
- Speed. The clock and the screensaver now redraw the whole screen about 11 times a second, the same way the weather sky does, and the floating cover redraws about 16 times a second. None of that has been timed on the real chip. The bars are sent 14 times a second over the same USB link as the album art.
- Memory. The sky picture, the album art and the blur all have to fit in the board's memory at once. The numbers work on paper. If the sky can't get its memory, Weather and Clock fall back to their plain screens and the screensaver to a drifting clock.
- The visualizer hearing real sound on Windows. The listening code has only met a stand-in, never a real sound card.
- The live Notion and weather lookups. The machine this was built on could not reach either service.
- Reading Apple Music through the real Windows media controls and through real AppleScript on a Mac.
- Temperature readings on Windows.
