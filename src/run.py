import time
from gpiozero import Button
from luma.core.interface.serial import i2c
from luma.oled.device import sh1106
from PIL import Image, ImageDraw, ImageFont

# SDA_PIN = 21
# SCL_PIN = 22
ROTARY_A = 27
ROTARY_B = 17
ROTARY_SW = 22
MODE_SW = 23

lastModePress = 0.0
lastRotaryPress = 0.0
debounceDelay = 0.2

lastModeState = True
lastRotaryState = True
lastEncoderAState = 1

trackListFound = True

tracks = [
    "Track Alpha",
    "Track Beta",
    "Track Gamma",
    "Track Delta",
    "Track Echo",
    "Track Foxtrot",
    "Track Golf",
    "Track Hotel",
    "Track India",
    "Track Juliet"
]

TRACK_COUNT = len(tracks)
TRACKS_PER_PAGE = 5

CLOCK = 0
PLAYER = 1
TRACKS = 2
SETTINGS = 3

currentScreen = CLOCK

selectedTrackIndex = 0

counter = 0
needsRedraw = True

WIDTH = 128
HEIGHT = 64

font_small = ImageFont.load_default()
font_big = ImageFont.load_default()

serial = None
display = None
image = None
draw = None

rotaryA = None
rotaryB = None
rotarySW = None
modeSW = None


def drawClockScreen():
    global draw, counter

    draw.text((0, 0), "Clock", font=font_small, fill=255)
    draw.line((0, 12, 128, 12), fill=255)

    draw.text((0, 24), "Count:", font=font_small, fill=255)
    draw.text((60, 24), str(counter), font=font_small, fill=255)

    draw.text((0, 50), "Press to reset", font=font_small, fill=255)


def drawPlayerScreen():
    global draw, selectedTrackIndex

    if selectedTrackIndex < 0:
        selectedTrackIndex = 0
    if selectedTrackIndex >= TRACK_COUNT:
        selectedTrackIndex = TRACK_COUNT - 1

    draw.text((0, 0), "Player", font=font_small, fill=255)
    draw.line((0, 12, 128, 12), fill=255)

    draw.text((0, 30), tracks[selectedTrackIndex], font=font_small, fill=255)


def drawTracksScreen():
    global draw, counter

    draw.text((0, 0), "Tracks", font=font_small, fill=255)
    draw.line((0, 12, 128, 12), fill=255)

    # keep counter within valid range
    if counter < 0:
        counter = 0
    if counter >= TRACK_COUNT:
        counter = TRACK_COUNT - 1

    scrollOffset = counter - (TRACKS_PER_PAGE // 2)

    if trackListFound:
        if scrollOffset < 0:
            scrollOffset = 0

        if scrollOffset > TRACK_COUNT - TRACKS_PER_PAGE:
            scrollOffset = TRACK_COUNT - TRACKS_PER_PAGE

        if scrollOffset < 0:
            scrollOffset = 0

        for i in range(TRACKS_PER_PAGE):
            trackIndex = scrollOffset + i
            if trackIndex >= TRACK_COUNT:
                break

            y = 24 + (i * 8)

            if trackIndex == counter:
                draw.rectangle((0, y - 7, 127, y + 1), fill=255)
                draw.text((2, y - 6), tracks[trackIndex], font=font_small, fill=0)
            else:
                draw.text((2, y - 6), tracks[trackIndex], font=font_small, fill=255)
    else:
        draw.text((2, 25), "No tracks found.", font=font_small, fill=255)


def drawSettingsScreen():
    global draw

    draw.text((0, 0), "Settings", font=font_small, fill=255)
    draw.line((0, 12, 128, 12), fill=255)

    draw.text((0, 30), "Settings menu", font=font_small, fill=255)


def drawCurrentScreen():
    global image, draw, needsRedraw

    image = Image.new("1", (WIDTH, HEIGHT))
    draw = ImageDraw.Draw(image)

    if currentScreen == CLOCK:
        drawClockScreen()
    elif currentScreen == PLAYER:
        drawPlayerScreen()
    elif currentScreen == TRACKS:
        drawTracksScreen()
    elif currentScreen == SETTINGS:
        drawSettingsScreen()

    display.display(image)
    needsRedraw = False


def nextScreen():
    global currentScreen, needsRedraw
    currentScreen = (currentScreen + 1) % 4
    needsRedraw = True


def handleEncoder():
    global counter, lastEncoderAState, needsRedraw

    encoderAState = int(rotaryA.value)

    if lastEncoderAState == 1 and encoderAState == 0:
        if int(rotaryB.value) != encoderAState:
            counter += 1
        else:
            counter -= 1

        print(f"Position: {counter}")
        needsRedraw = True

    lastEncoderAState = encoderAState


def handleButtons():
    global lastModePress, lastRotaryPress
    global lastModeState, lastRotaryState, needsRedraw

    modeState = not modeSW.is_pressed
    rotaryState = not rotarySW.is_pressed

    now = time.time()

    # MODE button (change screen)
    if modeState is False and lastModeState is True and (now - lastModePress > debounceDelay):
        nextScreen()
        lastModePress = now

    # Rotary button
    if rotaryState is False and lastRotaryState is True and (now - lastRotaryPress > debounceDelay):
        rotaryPressed()
        needsRedraw = True
        lastRotaryPress = now

    lastModeState = modeState
    lastRotaryState = rotaryState


def rotaryPressed():
    global selectedTrackIndex, currentScreen, needsRedraw

    if currentScreen == CLOCK:
        pass

    elif currentScreen == PLAYER:
        pass

    elif currentScreen == TRACKS:
        if trackListFound:
            selectedTrackIndex = counter
            currentScreen = PLAYER
            needsRedraw = True

    elif currentScreen == SETTINGS:
        pass


def setup():
    global serial, display
    global rotaryA, rotaryB, rotarySW, modeSW
    global lastEncoderAState, needsRedraw

    rotaryA = Button(ROTARY_A, pull_up=True)
    rotaryB = Button(ROTARY_B, pull_up=True)
    rotarySW = Button(ROTARY_SW, pull_up=True)
    modeSW = Button(MODE_SW, pull_up=True)

    serial = i2c(port=1, address=0x3C)
    display = sh1106(serial)

    lastEncoderAState = int(rotaryA.value)
    needsRedraw = True


def loop():
    handleEncoder()
    handleButtons()

    if needsRedraw:
        drawCurrentScreen()


def main():
    setup()

    while True:
        loop()
        time.sleep(0.005)


if __name__ == "__main__":
    main()