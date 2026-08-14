#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <random>
#include <sstream>
#include <string>
#include <cstdio>
#include <thread>
#include <vector>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")

// ============================================================
// SETTINGS
// ============================================================

const int SAMPLE_RATE = 44100;
const double PI = 3.14159265358979323846;

// ONE PLAYER
const int PLAYER_COUNT = 1;

const double MIN_TEMPO = 40.0;
const double MAX_TEMPO = 480.0;
const double TEMPO_STEP = 5.0;

const int MIN_PITCH = -12;
const int MAX_PITCH = 12;
const int PITCH_STEP = 1;

const int AUDIO_BUFFER_COUNT = 2;

// ============================================================
// MASTER LOUDNESS
// ============================================================

const double MASTER_GAIN = 2.5;

// ============================================================
// COLORS
// ============================================================

const COLORREF BACKGROUND = RGB(0, 207, 74);

const COLORREF COLUMN = RGB(32, 32, 38);
const COLORREF COLUMN_BORDER = RGB(55, 55, 65);

const COLORREF TEXT_COLOR = RGB(255, 255, 255);
const COLORREF BUTTON_TEXT = RGB(0, 255, 0);

const COLORREF EDIT_BACKGROUND = RGB(15, 15, 20);
const COLORREF EDIT_TEXT = RGB(255, 255, 255);

// ============================================================
// PLAYER STATE
// ============================================================

std::atomic_bool playing[PLAYER_COUNT];
std::atomic_bool looping[PLAYER_COUNT];
std::atomic_bool stopRequested[PLAYER_COUNT];

std::atomic<double> currentTempo[PLAYER_COUNT];
std::atomic<int> currentPitch[PLAYER_COUNT];

std::atomic<int> volumeBoost[PLAYER_COUNT];

// ============================================================
// WINDOWS CONTROLS
// ============================================================

HWND playButton[PLAYER_COUNT] = {};
HWND loopButton[PLAYER_COUNT] = {};
HWND boostButton[PLAYER_COUNT] = {};

HWND tempoMinusButton[PLAYER_COUNT] = {};
HWND tempoPlusButton[PLAYER_COUNT] = {};
HWND tempoLabel[PLAYER_COUNT] = {};

HWND pitchMinusButton[PLAYER_COUNT] = {};
HWND pitchPlusButton[PLAYER_COUNT] = {};
HWND pitchLabel[PLAYER_COUNT] = {};

HWND songEditor[PLAYER_COUNT] = {};

HWND mainWindow = nullptr;

// ============================================================
// LAYOUT
// ============================================================

// Top control area.
const int TOP_BAR_HEIGHT = 105;

// One player section.
const int SECTION_WIDTH = 900;
const int SECTION_HEIGHT = 700;

// Compact buttons.
const int SMALL_BUTTON_WIDTH = 70;
const int SMALL_BUTTON_HEIGHT = 28;

const int CONTROL_GAP = 5;

// Editor.
const int EDIT_HEIGHT = 500;

// ============================================================
// RANDOM
// ============================================================

double noise()
{
    thread_local std::mt19937 generator(
        std::random_device{}()
    );

    thread_local std::uniform_real_distribution<double>
        distribution(-1.0, 1.0);

    return distribution(generator);
}

// ============================================================
// SONG DATA
// ============================================================

struct NoteEvent
{
    double startBeat;
    double durationBeats;
    double frequency;
    std::string instrument;
};

struct DrumEvent
{
    double startBeat;
    std::string type;
};

// ============================================================
// NOTE FREQUENCIES
// ============================================================

double noteFrequency(const std::string& note)
{
    if (note == "C2")  return 65.41;
    if (note == "C#2") return 69.30;
    if (note == "D2")  return 73.42;
    if (note == "D#2") return 77.78;
    if (note == "E2")  return 82.41;
    if (note == "F2")  return 87.31;
    if (note == "F#2") return 92.50;
    if (note == "G2")  return 98.00;
    if (note == "G#2") return 103.83;
    if (note == "A2")  return 110.00;
    if (note == "A#2") return 116.54;
    if (note == "B2")  return 123.47;

    if (note == "C3")  return 130.81;
    if (note == "C#3") return 138.59;
    if (note == "D3")  return 146.83;
    if (note == "D#3") return 155.56;
    if (note == "E3")  return 164.81;
    if (note == "F3")  return 174.61;
    if (note == "F#3") return 185.00;
    if (note == "G3")  return 196.00;
    if (note == "G#3") return 207.65;
    if (note == "A3")  return 220.00;
    if (note == "A#3") return 233.08;
    if (note == "B3")  return 246.94;

    if (note == "C4")  return 261.63;
    if (note == "C#4") return 277.18;
    if (note == "D4")  return 293.66;
    if (note == "D#4") return 311.13;
    if (note == "E4")  return 329.63;
    if (note == "F4")  return 349.23;
    if (note == "F#4") return 369.99;
    if (note == "G4")  return 392.00;
    if (note == "G#4") return 415.30;
    if (note == "A4")  return 440.00;
    if (note == "A#4") return 466.16;
    if (note == "B4")  return 493.88;

    if (note == "C5")  return 523.25;
    if (note == "C#5") return 554.37;
    if (note == "D5")  return 587.33;
    if (note == "D#5") return 622.25;
    if (note == "E5")  return 659.25;
    if (note == "F5")  return 698.46;
    if (note == "F#5") return 739.99;
    if (note == "G5")  return 783.99;
    if (note == "G#5") return 830.61;
    if (note == "A5")  return 880.00;
    if (note == "A#5") return 932.33;
    if (note == "B5")  return 987.77;

    if (note == "C6")  return 1046.50;
    if (note == "C#6") return 1108.73;
    if (note == "D6")  return 1174.66;
    if (note == "D#6") return 1244.51;
    if (note == "E6")  return 1318.51;
    if (note == "F6")  return 1396.91;
    if (note == "F#6") return 1479.98;
    if (note == "G6")  return 1567.98;
    if (note == "G#6") return 1661.22;
    if (note == "A6")  return 1760.00;
    if (note == "A#6") return 1864.66;
    if (note == "B6")  return 1975.53;

    return 0.0;
}

// ============================================================
// INSTRUMENT WAVEFORM
// ============================================================

double instrumentWave(
    const std::string& instrument,
    double frequency,
    double t)
{
    double phase =
        2.0 * PI * frequency * t;

    // PIANO
    if (instrument == "PIANO")
    {
        double envelope =
            std::exp(-1.7 * t);

        return
            (
                std::sin(phase) +
                std::sin(phase * 2.0) * 0.35 +
                std::sin(phase * 3.0) * 0.15 +
                std::sin(phase * 4.0) * 0.06
            )
            * envelope;
    }

    // BASS
    if (instrument == "BASS")
    {
        double envelope =
            std::exp(-1.0 * t);

        return
            (
                std::sin(phase) * 0.9 +
                std::sin(phase * 2.0) * 0.25 +
                std::sin(phase * 3.0) * 0.08
            )
            * envelope;
    }

    // GUITAR
    if (instrument == "GUITAR")
    {
        double attack =
            1.0 - std::exp(-120.0 * t);

        double decay =
            std::exp(-3.8 * t);

        double pluck =
            std::exp(-35.0 * t);

        double sound =
            std::sin(phase) * 0.60 +
            std::sin(phase * 2.0) * 0.30 +
            std::sin(phase * 3.0) * 0.18 +
            std::sin(phase * 5.0) * 0.08;

        return
            sound *
            attack *
            decay
            +
            std::sin(phase * 2.0) *
            pluck *
            0.12;
    }

    // SYNTH
    if (instrument == "SYNTH")
    {
        double saw =
            2.0 *
            (
                frequency * t -
                std::floor(
                    frequency * t + 0.5
                )
            );

        return saw * 0.8;
    }

    // ORGAN
    if (instrument == "ORGAN")
    {
        return
            std::sin(phase) * 0.65 +
            std::sin(phase * 2.0) * 0.25 +
            std::sin(phase * 4.0) * 0.12 +
            std::sin(phase * 8.0) * 0.04;
    }

    // FLUTE
    if (instrument == "FLUTE")
    {
        return
            (
                std::sin(phase) * 0.85 +
                std::sin(phase * 2.0) * 0.10
            )
            * std::exp(-0.8 * t);
    }

    // TRUMPET
    if (instrument == "TRUMPET")
    {
        double attack =
            1.0 - std::exp(-45.0 * t);

        double envelope =
            0.85 +
            0.15 * std::exp(-1.0 * t);

        double brass =
            std::sin(phase) * 0.45 +
            std::sin(phase * 2.0) * 0.30 +
            std::sin(phase * 3.0) * 0.28 +
            std::sin(phase * 4.0) * 0.20 +
            std::sin(phase * 5.0) * 0.14 +
            std::sin(phase * 6.0) * 0.08;

        return
            brass *
            attack *
            envelope;
    }

    // BELL
    if (instrument == "BELL")
    {
        double envelope =
            std::exp(-2.2 * t);

        return
            (
                std::sin(phase) +
                std::sin(phase * 2.71) * 0.4 +
                std::sin(phase * 4.13) * 0.2
            )
            * envelope;
    }

    return std::sin(phase);
}

// ============================================================
// DRUM SOUNDS
// ============================================================

double kick(double t)
{
    if (t < 0.0 || t >= 0.8)
        return 0.0;

    double frequency =
        180.0 *
        std::exp(-18.0 * t)
        +
        42.0;

    double envelope =
        std::exp(-5.5 * t);

    double transient =
        std::exp(-70.0 * t);

    double body =
        std::sin(
            2.0 *
            PI *
            frequency *
            t
        );

    return
        body *
        envelope *
        1.25
        +
        transient *
        0.40;
}

double snare(double t)
{
    if (t < 0 || t >= 0.35)
        return 0.0;

    double envelope =
        std::exp(-15.0 * t);

    double noisePart =
        noise() *
        envelope;

    double body =
        std::sin(
            2.0 *
            PI *
            180.0 *
            t
        ) *
        std::exp(-20.0 * t);

    return
        noisePart * 0.8 +
        body * 0.25;
}

double closedHiHat(double t)
{
    if (t < 0 || t >= 0.12)
        return 0.0;

    return
        noise() *
        std::exp(-45.0 * t) *
        0.7;
}

double openHiHat(double t)
{
    if (t < 0 || t >= 0.8)
        return 0.0;

    return
        noise() *
        std::exp(-5.0 * t) *
        0.6;
}

double clap(double t)
{
    if (t < 0 || t >= 0.3)
        return 0.0;

    double burst1 =
        std::exp(-80.0 * std::abs(t));

    double burst2 =
        std::exp(
            -60.0 *
            std::abs(t - 0.025)
        );

    double burst3 =
        std::exp(
            -50.0 *
            std::abs(t - 0.050)
        );

    double envelope =
        std::exp(-14.0 * t);

    return
        noise() *
        (
            burst1 +
            burst2 +
            burst3
        ) *
        envelope;
}

double tom(
    double t,
    double frequency)
{
    if (t < 0 || t >= 0.6)
        return 0.0;

    double pitch =
        frequency *
        std::exp(-3.0 * t)
        +
        frequency * 0.4;

    double envelope =
        std::exp(-7.0 * t);

    return
        std::sin(
            2.0 *
            PI *
            pitch *
            t
        ) *
        envelope;
}

double crash(double t)
{
    if (t < 0 || t >= 2.0)
        return 0.0;

    return
        noise() *
        std::exp(-2.5 * t) *
        0.65;
}

double ride(double t)
{
    if (t < 0 || t >= 1.5)
        return 0.0;

    double envelope =
        std::exp(-2.0 * t);

    double metallic =
        noise();

    double tone =
        std::sin(
            2.0 *
            PI *
            3500.0 *
            t
        );

    return
        (
            metallic * 0.5 +
            tone * 0.5
        ) *
        envelope *
        0.4;
}

double rimshot(double t)
{
    if (t < 0 || t >= 0.15)
        return 0.0;

    double envelope =
        std::exp(-40.0 * t);

    return
        std::sin(
            2.0 *
            PI *
            1200.0 *
            t
        ) *
        envelope *
        0.8
        +
        noise() *
        envelope *
        0.3;
}

double cowbell(double t)
{
    if (t < 0 || t >= 0.3)
        return 0.0;

    double envelope =
        std::exp(-15.0 * t);

    double tone1 =
        std::sin(
            2.0 *
            PI *
            540.0 *
            t
        );

    double tone2 =
        std::sin(
            2.0 *
            PI *
            800.0 *
            t
        );

    return
        (
            tone1 +
            tone2
        ) *
        envelope *
        0.4;
}

double shaker(double t)
{
    if (t < 0 || t >= 0.25)
        return 0.0;

    return
        noise() *
        std::exp(-18.0 * t) *
        0.5;
}

double tambourine(double t)
{
    if (t < 0 || t >= 0.7)
        return 0.0;

    double envelope =
        std::exp(-6.0 * t);

    return
        noise() *
        envelope *
        0.5;
}

// ============================================================
// DRUM SELECTOR
// ============================================================

double makeDrum(
    const std::string& type,
    double t)
{
    if (
        type == "KICK" ||
        type == "BASS_DRUM")
        return kick(t);

    if (type == "SNARE")
        return snare(t);

    if (
        type == "HIHAT" ||
        type == "CLOSED_HIHAT")
        return closedHiHat(t);

    if (
        type == "OPEN_HIHAT" ||
        type == "OPENHIHAT")
        return openHiHat(t);

    if (type == "CLAP")
        return clap(t);

    if (type == "LOW_TOM")
        return tom(t, 110.0);

    if (type == "MID_TOM")
        return tom(t, 180.0);

    if (type == "HIGH_TOM")
        return tom(t, 280.0);

    if (type == "CRASH")
        return crash(t);

    if (type == "RIDE")
        return ride(t);

    if (type == "RIMSHOT")
        return rimshot(t);

    if (type == "COWBELL")
        return cowbell(t);

    if (type == "SHAKER")
        return shaker(t);

    if (type == "TAMBOURINE")
        return tambourine(t);

    return 0.0;
}

// ============================================================
// UPPERCASE
// ============================================================

std::string upper(std::string value)
{
    for (char& c : value)
    {
        if (c >= 'a' && c <= 'z')
        {
            c =
                static_cast<char>(
                    c - 'a' + 'A'
                );
        }
    }

    return value;
}

// ============================================================
// LOAD SONG
// ============================================================

bool LoadSongText(
    const std::string& text,
    std::vector<NoteEvent>& notes,
    std::vector<DrumEvent>& drums,
    double& tempo,
    double& loopLengthBeats)
{
    notes.clear();
    drums.clear();

    tempo = 120.0;
    loopLengthBeats = 4.0;

    std::istringstream input(text);
    std::string command;

    while (input >> command)
    {
        command = upper(command);

        if (command == "TEMPO")
        {
            input >> tempo;

            tempo =
                std::max(
                    MIN_TEMPO,
                    std::min(
                        MAX_TEMPO,
                        tempo
                    )
                );
        }
        else if (command == "LENGTH")
        {
            input >> loopLengthBeats;

            if (loopLengthBeats <= 0.0)
                loopLengthBeats = 4.0;
        }
        else if (command == "NOTE")
        {
            std::string noteName;
            double startBeat;
            double durationBeats;

            input >>
                noteName >>
                startBeat >>
                durationBeats;

            double frequency =
                noteFrequency(
                    upper(noteName)
                );

            if (frequency > 0.0)
            {
                notes.push_back(
                    {
                        startBeat,
                        durationBeats,
                        frequency,
                        "PIANO"
                    }
                );
            }
        }
        else if (
            command == "PIANO" ||
            command == "BASS" ||
            command == "GUITAR" ||
            command == "SYNTH" ||
            command == "ORGAN" ||
            command == "FLUTE" ||
            command == "TRUMPET" ||
            command == "BELL")
        {
            std::string noteName;
            double startBeat;
            double durationBeats;

            input >>
                noteName >>
                startBeat >>
                durationBeats;

            double frequency =
                noteFrequency(
                    upper(noteName)
                );

            if (frequency > 0.0)
            {
                notes.push_back(
                    {
                        startBeat,
                        durationBeats,
                        frequency,
                        command
                    }
                );
            }
        }
        else if (command == "DRUM")
        {
            std::string drumType;
            double startBeat;

            input >>
                drumType >>
                startBeat;

            drums.push_back(
                {
                    startBeat,
                    upper(drumType)
                }
            );
        }
    }

    return true;
}

// ============================================================
// GET EDITOR TEXT
// ============================================================

std::string GetEditorText(int playerIndex)
{
    if (
        playerIndex < 0 ||
        playerIndex >= PLAYER_COUNT)
        return "";

    HWND editor =
        songEditor[playerIndex];

    if (!editor)
        return "";

    int length =
        GetWindowTextLengthA(editor);

    if (length <= 0)
        return "";

    std::string text(
        length + 1,
        '\0'
    );

    int actualLength =
        GetWindowTextA(
            editor,
            &text[0],
            length + 1
        );

    text.resize(actualLength);

    return text;
}

// ============================================================
// VOLUME MULTIPLIER
// ============================================================

double GetVolumeMultiplier(int level)
{
    switch (level)
    {
        case 1:
            return 2.0;

        case 2:
            return 3.5;

        case 3:
            return 5.0;

        default:
            return 1.0;
    }
}

// ============================================================
// GENERATE AUDIO
// ============================================================

bool GenerateAudio(
    const std::string& text,
    double tempoOverride,
    int pitchSemitones,
    double volumeMultiplier,
    std::vector<short>& samples)
{
    std::vector<NoteEvent> notes;
    std::vector<DrumEvent> drums;

    double fileTempo;
    double loopLengthBeats;

    if (!LoadSongText(
        text,
        notes,
        drums,
        fileTempo,
        loopLengthBeats))
    {
        return false;
    }

    double tempo =
        std::max(
            MIN_TEMPO,
            std::min(
                MAX_TEMPO,
                tempoOverride
            )
        );

    int pitch =
        std::max(
            MIN_PITCH,
            std::min(
                MAX_PITCH,
                pitchSemitones
            )
        );

    double pitchMultiplier =
        std::pow(
            2.0,
            static_cast<double>(pitch) / 12.0
        );

    double secondsPerBeat =
        60.0 / tempo;

    double loopDuration =
        loopLengthBeats *
        secondsPerBeat;

    int totalSamples =
        static_cast<int>(
            std::round(
                loopDuration *
                SAMPLE_RATE
            )
        );

    if (totalSamples <= 0)
        return false;

    std::vector<double> audio(
        totalSamples,
        0.0
    );

    // ========================================================
    // MELODY
    // ========================================================

    for (const NoteEvent& note : notes)
    {
        double startSeconds =
            note.startBeat *
            secondsPerBeat;

        double durationSeconds =
            note.durationBeats *
            secondsPerBeat;

        int startSample =
            static_cast<int>(
                std::round(
                    startSeconds *
                    SAMPLE_RATE
                )
            );

        int noteSamples =
            static_cast<int>(
                std::round(
                    durationSeconds *
                    SAMPLE_RATE
                )
            );

        double shiftedFrequency =
            note.frequency *
            pitchMultiplier;

        for (
            int i = 0;
            i < noteSamples;
            ++i)
        {
            int index =
                startSample + i;

            if (index < 0)
                continue;

            if (index >= totalSamples)
                break;

            double time =
                static_cast<double>(i) /
                SAMPLE_RATE;

            double envelope = 1.0;

            if (time < 0.01)
            {
                envelope =
                    time / 0.01;
            }

            double remaining =
                durationSeconds -
                time;

            if (remaining < 0.05)
            {
                envelope =
                    std::min(
                        envelope,
                        std::max(
                            0.0,
                            remaining / 0.05
                        )
                    );
            }

            double wave =
                instrumentWave(
                    note.instrument,
                    shiftedFrequency,
                    time
                );

            audio[index] +=
                wave *
                envelope *
                0.55;
        }
    }

    // ========================================================
    // DRUMS
    // ========================================================

    for (const DrumEvent& drum : drums)
    {
        double startSeconds =
            drum.startBeat *
            secondsPerBeat;

        int startSample =
            static_cast<int>(
                std::round(
                    startSeconds *
                    SAMPLE_RATE
                )
            );

        int drumSamples =
            static_cast<int>(
                2.0 *
                SAMPLE_RATE
            );

        for (
            int i = 0;
            i < drumSamples;
            ++i)
        {
            int index =
                startSample + i;

            if (index < 0)
                continue;

            if (index >= totalSamples)
                break;

            double time =
                static_cast<double>(i) /
                SAMPLE_RATE;

            double drumVolume = 1.50;

            if (
                drum.type == "KICK" ||
                drum.type == "BASS_DRUM")
            {
                drumVolume = 2.25;
            }
            else if (drum.type == "SNARE")
            {
                drumVolume = 1.75;
            }
            else if (
                drum.type == "HIHAT" ||
                drum.type == "CLOSED_HIHAT")
            {
                drumVolume = 1.10;
            }
            else if (drum.type == "OPEN_HIHAT")
            {
                drumVolume = 1.30;
            }
            else if (drum.type == "CRASH")
            {
                drumVolume = 1.50;
            }
            else if (drum.type == "RIDE")
            {
                drumVolume = 1.25;
            }

            audio[index] +=
                makeDrum(
                    drum.type,
                    time
                )
                *
                drumVolume;
        }
    }

    // ========================================================
    // MASTER VOLUME
    // ========================================================

    if (volumeMultiplier < 1.0)
        volumeMultiplier = 1.0;

    samples.resize(
        totalSamples
    );

    for (
        int i = 0;
        i < totalSamples;
        ++i)
    {
        double value =
            audio[i] *
            MASTER_GAIN *
            volumeMultiplier;

        value =
            std::tanh(value);

        samples[i] =
            static_cast<short>(
                value *
                32767.0
            );
    }

    return true;
}

// ============================================================
// BUTTON IDS
// ============================================================

#define ID_PLAY_BASE         1000
#define ID_LOOP_BASE         1100
#define ID_TEMPO_MINUS_BASE  1200
#define ID_TEMPO_PLUS_BASE   1300
#define ID_PITCH_MINUS_BASE  1400
#define ID_PITCH_PLUS_BASE   1500
#define ID_BOOST_BASE        1600

// ============================================================
// EDIT CONTROL COLORS
// ============================================================

HBRUSH editBrush = nullptr;

// ============================================================
// UPDATE TEMPO DISPLAY
// ============================================================

void UpdateTempoDisplay(int playerIndex)
{
    if (
        playerIndex < 0 ||
        playerIndex >= PLAYER_COUNT)
        return;

    if (!tempoLabel[playerIndex])
        return;

    int tempo =
        static_cast<int>(
            std::round(
                currentTempo[playerIndex].load()
            )
        );

    char text[64];

    wsprintfA(
        text,
        "TEMPO: %d",
        tempo
    );

    SetWindowTextA(
        tempoLabel[playerIndex],
        text
    );
}

// ============================================================
// UPDATE PITCH DISPLAY
// ============================================================

void UpdatePitchDisplay(int playerIndex)
{
    if (
        playerIndex < 0 ||
        playerIndex >= PLAYER_COUNT)
        return;

    if (!pitchLabel[playerIndex])
        return;

    int pitch =
        currentPitch[playerIndex].load();

    char text[64];

    if (pitch > 0)
    {
        wsprintfA(
            text,
            "PITCH: +%d",
            pitch
        );
    }
    else
    {
        wsprintfA(
            text,
            "PITCH: %d",
            pitch
        );
    }

    SetWindowTextA(
        pitchLabel[playerIndex],
        text
    );
}

// ============================================================
// UPDATE BOOST DISPLAY
// ============================================================

void UpdateBoostDisplay(int playerIndex)
{
    if (
        playerIndex < 0 ||
        playerIndex >= PLAYER_COUNT)
        return;

    if (!boostButton[playerIndex])
        return;

    int level =
        volumeBoost[playerIndex].load();

    char text[64];

    double multiplier =
        GetVolumeMultiplier(level);

    snprintf(
        text,
        sizeof(text),
        "BOOST %.1fx",
        multiplier
    );

    SetWindowTextA(
        boostButton[playerIndex],
        text
    );
}

// ============================================================
// PLAYER BUTTON ID
// ============================================================

int PlayerButtonID(
    int base,
    int player)
{
    return base + player;
}

// ============================================================
// RESIZE CONTROLS
// ============================================================

void ResizePlayerControls(HWND window)
{
    RECT rect;

    GetClientRect(
        window,
        &rect
    );

    int width =
        rect.right;

    if (width < 400)
        width = 400;

    // --------------------------------------------------------
    // TOP LEFT CONTROL STRIP
    // --------------------------------------------------------

    int x = 15;
    int y = 15;

    // PLAY
    MoveWindow(
        playButton[0],
        x,
        y,
        65,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 65 + CONTROL_GAP;

    // LOOP
    MoveWindow(
        loopButton[0],
        x,
        y,
        75,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 75 + CONTROL_GAP;

    // BOOST
    MoveWindow(
        boostButton[0],
        x,
        y,
        80,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 80 + CONTROL_GAP;

    // TEMPO MINUS
    MoveWindow(
        tempoMinusButton[0],
        x,
        y,
        28,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 28;

    // TEMPO LABEL
    MoveWindow(
        tempoLabel[0],
        x,
        y,
        90,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 90;

    // TEMPO PLUS
    MoveWindow(
        tempoPlusButton[0],
        x,
        y,
        28,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 28 + CONTROL_GAP;

    // PITCH MINUS
    MoveWindow(
        pitchMinusButton[0],
        x,
        y,
        28,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 28;

    // PITCH LABEL
    MoveWindow(
        pitchLabel[0],
        x,
        y,
        80,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    x += 80;

    // PITCH PLUS
    MoveWindow(
        pitchPlusButton[0],
        x,
        y,
        28,
        SMALL_BUTTON_HEIGHT,
        TRUE
    );

    // --------------------------------------------------------
    // SONG EDITOR
    // --------------------------------------------------------

    int editorX = 15;
    int editorY = 60;

    int editorWidth =
        rect.right - 30;

    if (editorWidth < 300)
        editorWidth = 300;

    MoveWindow(
        songEditor[0],
        editorX,
        editorY,
        editorWidth,
        EDIT_HEIGHT,
        TRUE
    );
}

// ============================================================
// DRAW PLAYER SECTION
// ============================================================

void DrawPlayerSection(
    HDC dc,
    RECT rect)
{
    RECT section =
    {
        10,
        5,
        rect.right - 10,
        EDIT_HEIGHT + 80
    };

    HBRUSH brush =
        CreateSolidBrush(
            COLUMN
        );

    FillRect(
        dc,
        &section,
        brush
    );

    DeleteObject(
        brush
    );

    HPEN borderPen =
        CreatePen(
            PS_SOLID,
            1,
            COLUMN_BORDER
        );

    HPEN oldPen =
        (HPEN)SelectObject(
            dc,
            borderPen
        );

    HBRUSH oldBrush =
        (HBRUSH)SelectObject(
            dc,
            GetStockObject(
                NULL_BRUSH
            )
        );

    Rectangle(
        dc,
        section.left,
        section.top,
        section.right,
        section.bottom
    );

    SelectObject(
        dc,
        oldBrush
    );

    SelectObject(
        dc,
        oldPen
    );

    DeleteObject(
        borderPen
    );
}

// ============================================================
// PLAY SONG
// ============================================================

void PlaySong(int playerIndex)
{
    if (
        playerIndex < 0 ||
        playerIndex >= PLAYER_COUNT)
        return;

    if (playing[playerIndex])
        return;

    playing[playerIndex] = true;
    stopRequested[playerIndex] = false;

    std::string songText =
        GetEditorText(
            playerIndex
        );

    if (songText.empty())
    {
        playing[playerIndex] = false;

        PostMessage(
            mainWindow,
            WM_USER + playerIndex + 1,
            0,
            0
        );

        return;
    }

    std::vector<NoteEvent> notes;
    std::vector<DrumEvent> drums;

    double fileTempo = 120.0;
    double loopLengthBeats = 4.0;

    LoadSongText(
        songText,
        notes,
        drums,
        fileTempo,
        loopLengthBeats
    );

    if (
        notes.empty() &&
        drums.empty())
    {
        playing[playerIndex] = false;

        PostMessage(
            mainWindow,
            WM_USER + playerIndex + 1,
            0,
            0
        );

        return;
    }

    // ========================================================
    // AUDIO FORMAT
    // ========================================================

    WAVEFORMATEX format = {};

    format.wFormatTag =
        WAVE_FORMAT_PCM;

    format.nChannels =
        1;

    format.nSamplesPerSec =
        SAMPLE_RATE;

    format.wBitsPerSample =
        16;

    format.nBlockAlign =
        format.nChannels *
        format.wBitsPerSample /
        8;

    format.nAvgBytesPerSec =
        format.nSamplesPerSec *
        format.nBlockAlign;

    HWAVEOUT audioDevice =
        nullptr;

    MMRESULT result =
        waveOutOpen(
            &audioDevice,
            WAVE_MAPPER,
            &format,
            0,
            0,
            CALLBACK_NULL
        );

    if (
        result !=
        MMSYSERR_NOERROR)
    {
        playing[playerIndex] = false;

        PostMessage(
            mainWindow,
            WM_USER + playerIndex + 1,
            0,
            0
        );

        return;
    }

    // ========================================================
    // AUDIO BUFFERS
    // ========================================================

    std::vector<short> audioSamples[
        AUDIO_BUFFER_COUNT
    ];

    WAVEHDR headers[
        AUDIO_BUFFER_COUNT
    ] = {};

    bool prepared[
        AUDIO_BUFFER_COUNT
    ] = {};

    bool queued[
        AUDIO_BUFFER_COUNT
    ] = {};

    // ========================================================
    // GENERATE BUFFER
    // ========================================================

    auto generateBuffer =
        [&](int bufferIndex) -> bool
    {
        double tempo =
            currentTempo[playerIndex].load();

        int pitch =
            currentPitch[playerIndex].load();

        int boostLevel =
            volumeBoost[playerIndex].load();

        double multiplier =
            GetVolumeMultiplier(
                boostLevel
            );

        return GenerateAudio(
            songText,
            tempo,
            pitch,
            multiplier,
            audioSamples[
                bufferIndex
            ]
        );
    };

    // ========================================================
    // GENERATE FIRST BUFFER
    // ========================================================

    if (!generateBuffer(0))
    {
        waveOutClose(
            audioDevice
        );

        playing[playerIndex] = false;

        PostMessage(
            mainWindow,
            WM_USER + playerIndex + 1,
            0,
            0
        );

        return;
    }

    // ========================================================
    // PREPARE AND WRITE
    // ========================================================

    auto prepareAndWrite =
        [&](int bufferIndex) -> bool
    {
        headers[bufferIndex] = {};

        headers[bufferIndex].lpData =
            reinterpret_cast<LPSTR>(
                audioSamples[
                    bufferIndex
                ].data()
            );

        headers[bufferIndex].dwBufferLength =
            static_cast<DWORD>(
                audioSamples[
                    bufferIndex
                ].size() *
                sizeof(short)
            );

        MMRESULT prepareResult =
            waveOutPrepareHeader(
                audioDevice,
                &headers[bufferIndex],
                sizeof(WAVEHDR)
            );

        if (
            prepareResult !=
            MMSYSERR_NOERROR)
        {
            return false;
        }

        prepared[bufferIndex] = true;

        MMRESULT writeResult =
            waveOutWrite(
                audioDevice,
                &headers[bufferIndex],
                sizeof(WAVEHDR)
            );

        if (
            writeResult !=
            MMSYSERR_NOERROR)
        {
            waveOutUnprepareHeader(
                audioDevice,
                &headers[bufferIndex],
                sizeof(WAVEHDR)
            );

            prepared[bufferIndex] = false;

            return false;
        }

        queued[bufferIndex] = true;

        return true;
    };

    // ========================================================
    // FIRST BUFFER
    // ========================================================

    if (!prepareAndWrite(0))
    {
        waveOutReset(
            audioDevice
        );

        waveOutClose(
            audioDevice
        );

        playing[playerIndex] = false;

        PostMessage(
            mainWindow,
            WM_USER + playerIndex + 1,
            0,
            0
        );

        return;
    }

    // ========================================================
    // SECOND BUFFER IF LOOPING
    // ========================================================

    if (looping[playerIndex])
    {
        if (!generateBuffer(1))
        {
            stopRequested[playerIndex] = true;
        }
        else
        {
            if (!prepareAndWrite(1))
            {
                stopRequested[playerIndex] = true;
            }
        }
    }

    // ========================================================
    // PLAYBACK LOOP
    // ========================================================

    while (
        !stopRequested[playerIndex])
    {
        bool didSomething =
            false;

        for (
            int i = 0;
            i < AUDIO_BUFFER_COUNT;
            ++i)
        {
            if (!queued[i])
                continue;

            if (
                !(headers[i].dwFlags &
                  WHDR_DONE))
            {
                continue;
            }

            didSomething = true;

            waveOutUnprepareHeader(
                audioDevice,
                &headers[i],
                sizeof(WAVEHDR)
            );

            prepared[i] = false;
            queued[i] = false;

            if (!looping[playerIndex])
                continue;

            if (stopRequested[playerIndex])
                continue;

            if (!generateBuffer(i))
            {
                stopRequested[playerIndex] = true;
                continue;
            }

            if (!prepareAndWrite(i))
            {
                stopRequested[playerIndex] = true;
                continue;
            }
        }

        if (!looping[playerIndex])
        {
            bool anythingQueued = false;

            for (
                int i = 0;
                i < AUDIO_BUFFER_COUNT;
                ++i)
            {
                if (queued[i])
                {
                    anythingQueued = true;
                    break;
                }
            }

            if (!anythingQueued)
                break;
        }

        if (!didSomething)
            Sleep(1);
    }

    // ========================================================
    // STOP AUDIO
    // ========================================================

    waveOutReset(
        audioDevice
    );

    for (
        int i = 0;
        i < AUDIO_BUFFER_COUNT;
        ++i)
    {
        if (prepared[i])
        {
            waveOutUnprepareHeader(
                audioDevice,
                &headers[i],
                sizeof(WAVEHDR)
            );

            prepared[i] = false;
        }

        queued[i] = false;
    }

    waveOutClose(
        audioDevice
    );

    playing[playerIndex] = false;

    PostMessage(
        mainWindow,
        WM_USER + playerIndex + 1,
        0,
        0
    );
}

// ============================================================
// WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
        // ====================================================
        // BUTTON COLOR
        // ====================================================

        case WM_CTLCOLORBTN:
        {
            HDC dc =
                (HDC)wParam;

            SetTextColor(
                dc,
                BUTTON_TEXT
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            return (LRESULT)
                GetStockObject(
                    NULL_BRUSH
                );
        }

        // ====================================================
        // EDIT COLOR
        // ====================================================

        case WM_CTLCOLOREDIT:
        {
            HDC dc =
                (HDC)wParam;

            SetTextColor(
                dc,
                EDIT_TEXT
            );

            SetBkColor(
                dc,
                EDIT_BACKGROUND
            );

            return (LRESULT)
                editBrush;
        }

        // ====================================================
        // STATIC COLOR
        // ====================================================

        case WM_CTLCOLORSTATIC:
        {
            HDC dc =
                (HDC)wParam;

            SetTextColor(
                dc,
                TEXT_COLOR
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            return (LRESULT)
                GetStockObject(
                    NULL_BRUSH
                );
        }

        // ====================================================
        // COMMAND
        // ====================================================

        case WM_COMMAND:
        {
            int id =
                LOWORD(wParam);

            // Since there is only one player,
            // all controls belong to player 0.

            const int p = 0;

            // ------------------------------------------------
            // PLAY / STOP
            // ------------------------------------------------

            if (
                id ==
                PlayerButtonID(
                    ID_PLAY_BASE,
                    p
                ))
            {
                if (!playing[p])
                {
                    stopRequested[p] =
                        false;

                    SetWindowTextA(
                        playButton[p],
                        "STOP"
                    );

                    std::thread(
                        PlaySong,
                        p
                    ).detach();
                }
                else
                {
                    stopRequested[p] =
                        true;

                    SetWindowTextA(
                        playButton[p],
                        "PLAY"
                    );
                }

                break;
            }

            // ------------------------------------------------
            // LOOP
            // ------------------------------------------------

            if (
                id ==
                PlayerButtonID(
                    ID_LOOP_BASE,
                    p
                ))
            {
                looping[p] =
                    !looping[p];

                SetWindowTextA(
                    loopButton[p],
                    looping[p]
                        ? "LOOP ON"
                        : "LOOP OFF"
                );

                break;
            }

            // ------------------------------------------------
            // BOOST
            // ------------------------------------------------

            if (
                id ==
                PlayerButtonID(
                    ID_BOOST_BASE,
                    p
                ))
            {
                int level =
                    volumeBoost[p].load();

                level++;

                if (level > 3)
                    level = 0;

                volumeBoost[p] =
                    level;

                UpdateBoostDisplay(
                    p
                );

                break;
            }

            // ------------------------------------------------
            // TEMPO MINUS
            // ------------------------------------------------

            if (
                id ==
                PlayerButtonID(
                    ID_TEMPO_MINUS_BASE,
                    p
                ))
            {
                double tempo =
                    currentTempo[p].load();

                tempo -=
                    TEMPO_STEP;

                tempo =
                    std::max(
                        MIN_TEMPO,
                        tempo
                    );

                currentTempo[p] =
                    tempo;

                UpdateTempoDisplay(
                    p
                );

                break;
            }

            // ------------------------------------------------
            // TEMPO PLUS
            // ------------------------------------------------

            if (
                id ==
                PlayerButtonID(
                    ID_TEMPO_PLUS_BASE,
                    p
                ))
            {
                double tempo =
                    currentTempo[p].load();

                tempo +=
                    TEMPO_STEP;

                tempo =
                    std::min(
                        MAX_TEMPO,
                        tempo
                    );

                currentTempo[p] =
                    tempo;

                UpdateTempoDisplay(
                    p
                );

                break;
            }

            // ------------------------------------------------
            // PITCH MINUS
            // ------------------------------------------------

            if (
                id ==
                PlayerButtonID(
                    ID_PITCH_MINUS_BASE,
                    p
                ))
            {
                int pitch =
                    currentPitch[p].load();

                pitch -=
                    PITCH_STEP;

                pitch =
                    std::max(
                        MIN_PITCH,
                        pitch
                    );

                currentPitch[p] =
                    pitch;

                UpdatePitchDisplay(
                    p
                );

                break;
            }

            // ------------------------------------------------
            // PITCH PLUS
            // ------------------------------------------------

            if (
                id ==
                PlayerButtonID(
                    ID_PITCH_PLUS_BASE,
                    p
                ))
            {
                int pitch =
                    currentPitch[p].load();

                pitch +=
                    PITCH_STEP;

                pitch =
                    std::min(
                        MAX_PITCH,
                        pitch
                    );

                currentPitch[p] =
                    pitch;

                UpdatePitchDisplay(
                    p
                );

                break;
            }

            break;
        }

        // ====================================================
        // PLAYBACK FINISHED
        // ====================================================

        case WM_USER + 1:
        {
            SetWindowTextA(
                playButton[0],
                "PLAY"
            );

            break;
        }

        // ====================================================
        // RESIZE
        // ====================================================

        case WM_SIZE:
        {
            ResizePlayerControls(
                window
            );

            InvalidateRect(
                window,
                nullptr,
                TRUE
            );

            break;
        }

        // ====================================================
        // PAINT
        // ====================================================

        case WM_PAINT:
        {
            PAINTSTRUCT ps;

            HDC dc =
                BeginPaint(
                    window,
                    &ps
                );

            RECT rect;

            GetClientRect(
                window,
                &rect
            );

            // Background
            HBRUSH backgroundBrush =
                CreateSolidBrush(
                    BACKGROUND
                );

            FillRect(
                dc,
                &rect,
                backgroundBrush
            );

            DeleteObject(
                backgroundBrush
            );

            // Player panel
            DrawPlayerSection(
                dc,
                rect
            );

            EndPaint(
                window,
                &ps
            );

            break;
        }

        // ====================================================
        // DESTROY
        // ====================================================

        case WM_DESTROY:
        {
            stopRequested[0] =
                true;

            if (editBrush)
            {
                DeleteObject(
                    editBrush
                );

                editBrush =
                    nullptr;
            }

            PostQuitMessage(
                0
            );

            break;
        }

        default:
        {
            return DefWindowProcA(
                window,
                message,
                wParam,
                lParam
            );
        }
    }

    return 0;
}

// ============================================================
// MAIN
// ============================================================

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE,
    LPSTR,
    int)
{
    const char CLASS_NAME[] =
        "CppSongMakerOnePlayer";

    // ========================================================
    // INITIAL STATE
    // ========================================================

    playing[0] =
        false;

    looping[0] =
        false;

    stopRequested[0] =
        false;

    currentTempo[0] =
        120.0;

    currentPitch[0] =
        0;

    volumeBoost[0] =
        0;

    // ========================================================
    // EDIT BRUSH
    // ========================================================

    editBrush =
        CreateSolidBrush(
            EDIT_BACKGROUND
        );

    // ========================================================
    // WINDOW CLASS
    // ========================================================

    WNDCLASSA windowClass = {};

    windowClass.lpfnWndProc =
        WindowProcedure;

    windowClass.hInstance =
        instance;

    windowClass.lpszClassName =
        CLASS_NAME;

    windowClass.hbrBackground =
        CreateSolidBrush(
            BACKGROUND
        );

    windowClass.hCursor =
        LoadCursorA(
            nullptr,
            IDC_ARROW
        );

    RegisterClassA(
        &windowClass
    );

    // ========================================================
    // MAIN WINDOW
    // ========================================================

    HWND window =
        CreateWindowExA(
            WS_EX_COMPOSITED,
            CLASS_NAME,
            "C++ Song Maker - One Player",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1150,
            750,
            nullptr,
            nullptr,
            instance,
            nullptr
        );

    if (!window)
        return 0;

    mainWindow =
        window;

    // ========================================================
    // PLAYER 0 CONTROL IDS
    // ========================================================

    int playID =
        PlayerButtonID(
            ID_PLAY_BASE,
            0
        );

    int loopID =
        PlayerButtonID(
            ID_LOOP_BASE,
            0
        );

    int boostID =
        PlayerButtonID(
            ID_BOOST_BASE,
            0
        );

    int tempoMinusID =
        PlayerButtonID(
            ID_TEMPO_MINUS_BASE,
            0
        );

    int tempoPlusID =
        PlayerButtonID(
            ID_TEMPO_PLUS_BASE,
            0
        );

    int pitchMinusID =
        PlayerButtonID(
            ID_PITCH_MINUS_BASE,
            0
        );

    int pitchPlusID =
        PlayerButtonID(
            ID_PITCH_PLUS_BASE,
            0
        );

    // ========================================================
    // PLAY BUTTON
    // ========================================================

    playButton[0] =
        CreateWindowA(
            "BUTTON",
            "PLAY",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            65,
            SMALL_BUTTON_HEIGHT,
            window,
            (HMENU)(INT_PTR)
                playID,
            instance,
            nullptr
        );

    // ========================================================
    // LOOP BUTTON
    // ========================================================

    loopButton[0] =
        CreateWindowA(
            "BUTTON",
            "LOOP OFF",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            75,
            SMALL_BUTTON_HEIGHT,
            window,
            (HMENU)(INT_PTR)
                loopID,
            instance,
            nullptr
        );

    // ========================================================
    // BOOST BUTTON
    // ========================================================

    boostButton[0] =
        CreateWindowA(
            "BUTTON",
            "BOOST 1.0x",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            80,
            SMALL_BUTTON_HEIGHT,
            window,
            (HMENU)(INT_PTR)
                boostID,
            instance,
            nullptr
        );

    // ========================================================
    // TEMPO MINUS
    // ========================================================

    tempoMinusButton[0] =
        CreateWindowA(
            "BUTTON",
            "-",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            28,
            SMALL_BUTTON_HEIGHT,
            window,
            (HMENU)(INT_PTR)
                tempoMinusID,
            instance,
            nullptr
        );

    // ========================================================
    // TEMPO LABEL
    // ========================================================

    tempoLabel[0] =
        CreateWindowA(
            "STATIC",
            "TEMPO: 120",
            WS_VISIBLE |
            WS_CHILD |
            SS_CENTER |
            SS_CENTERIMAGE,
            0,
            0,
            90,
            SMALL_BUTTON_HEIGHT,
            window,
            nullptr,
            instance,
            nullptr
        );

    // ========================================================
    // TEMPO PLUS
    // ========================================================

    tempoPlusButton[0] =
        CreateWindowA(
            "BUTTON",
            "+",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            28,
            SMALL_BUTTON_HEIGHT,
            window,
            (HMENU)(INT_PTR)
                tempoPlusID,
            instance,
            nullptr
        );

    // ========================================================
    // PITCH MINUS
    // ========================================================

    pitchMinusButton[0] =
        CreateWindowA(
            "BUTTON",
            "-",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            28,
            SMALL_BUTTON_HEIGHT,
            window,
            (HMENU)(INT_PTR)
                pitchMinusID,
            instance,
            nullptr
        );

    // ========================================================
    // PITCH LABEL
    // ========================================================

    pitchLabel[0] =
        CreateWindowA(
            "STATIC",
            "PITCH: 0",
            WS_VISIBLE |
            WS_CHILD |
            SS_CENTER |
            SS_CENTERIMAGE,
            0,
            0,
            80,
            SMALL_BUTTON_HEIGHT,
            window,
            nullptr,
            instance,
            nullptr
        );

    // ========================================================
    // PITCH PLUS
    // ========================================================

    pitchPlusButton[0] =
        CreateWindowA(
            "BUTTON",
            "+",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            28,
            SMALL_BUTTON_HEIGHT,
            window,
            (HMENU)(INT_PTR)
                pitchPlusID,
            instance,
            nullptr
        );

    // ========================================================
    // SONG EDITOR
    // ========================================================

    songEditor[0] =
        CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            "",
            WS_VISIBLE |
            WS_CHILD |
            WS_VSCROLL |
            ES_MULTILINE |
            ES_AUTOVSCROLL |
            ES_WANTRETURN |
            ES_NOHIDESEL,
            0,
            0,
            500,
            EDIT_HEIGHT,
            window,
            nullptr,
            instance,
            nullptr
        );

    SendMessageA(
        songEditor[0],
        WM_SETFONT,
        (WPARAM)GetStockObject(
            DEFAULT_GUI_FONT
        ),
        TRUE
    );

    // ========================================================
    // INITIAL LAYOUT
    // ========================================================

    ResizePlayerControls(
        window
    );

    // ========================================================
    // SHOW WINDOW
    // ========================================================

    ShowWindow(
        window,
        SW_SHOW
    );

    UpdateWindow(
        window
    );

    // ========================================================
    // MESSAGE LOOP
    // ========================================================

    MSG message = {};

    while (
        GetMessageA(
            &message,
            nullptr,
            0,
            0
        ))
    {
        TranslateMessage(
            &message
        );

        DispatchMessageA(
            &message
        );
    }

    return 0;
}
