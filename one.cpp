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

const double PI =
    3.14159265358979323846;

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

const COLORREF BACKGROUND =
    RGB(0, 207, 74);

const COLORREF PANEL =
    RGB(32, 32, 38);

const COLORREF PANEL_BORDER =
    RGB(55, 55, 65);

const COLORREF TEXT_COLOR =
    RGB(255, 255, 255);

const COLORREF BUTTON_TEXT =
    RGB(0, 255, 0);

const COLORREF EDIT_BACKGROUND =
    RGB(15, 15, 20);

const COLORREF EDIT_TEXT =
    RGB(255, 255, 255);

// ============================================================
// SINGLE PLAYER STATE
// ============================================================

std::atomic_bool playing(false);
std::atomic_bool looping(false);
std::atomic_bool stopRequested(false);

std::atomic<double> currentTempo(120.0);
std::atomic<int> currentPitch(0);
std::atomic<int> volumeBoost(0);

// ============================================================
// WINDOWS CONTROLS
// ============================================================

HWND playButton = nullptr;
HWND loopButton = nullptr;
HWND boostButton = nullptr;

HWND tempoMinusButton = nullptr;
HWND tempoPlusButton = nullptr;
HWND tempoLabel = nullptr;

HWND pitchMinusButton = nullptr;
HWND pitchPlusButton = nullptr;
HWND pitchLabel = nullptr;

HWND songEditor = nullptr;
HWND saveTrackButton = nullptr;

HWND mainWindow = nullptr;

// ============================================================
// SAVED TRACKS
// ============================================================

struct SavedTrack
{
    std::string name;
    std::string text;
};

std::vector<SavedTrack> savedTracks;

// Buttons for saved tracks
std::vector<HWND> trackPlayButtons;
std::vector<HWND> trackLoadButtons;

// ============================================================
// LAYOUT
// ============================================================

int scrollY = 0;
int contentHeight = 0;

const int TOP_BAR_HEIGHT = 58;

const int LEFT_MARGIN = 20;
const int PANEL_WIDTH = 760;

const int EDITOR_TOP = 225;
const int EDITOR_HEIGHT = 300;

const int TRACK_START_Y = 585;

const int TRACK_ROW_HEIGHT = 42;

const int TRACK_BUTTON_WIDTH = 150;
const int LOAD_BUTTON_WIDTH = 90;

const int TRACK_GAP = 10;

// ============================================================
// RANDOM NOISE
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

double noteFrequency(
    const std::string& note)
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
        2.0 *
        PI *
        frequency *
        t;

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
            *
            envelope;
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
            *
            envelope;
    }

    // GUITAR
    if (instrument == "GUITAR")
    {
        double attack =
            1.0 -
            std::exp(-120.0 * t);

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
            *
            std::exp(-0.8 * t);
    }

    // TRUMPET
    if (instrument == "TRUMPET")
    {
        double attack =
            1.0 -
            std::exp(-45.0 * t);

        double envelope =
            0.85 +
            0.15 *
            std::exp(-1.0 * t);

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
            *
            envelope;
    }

    return std::sin(phase);
}

// ============================================================
// DRUMS
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
    if (t < 0.0 || t >= 0.35)
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
        )
        *
        std::exp(-20.0 * t);

    return
        noisePart * 0.8 +
        body * 0.25;
}

double closedHiHat(double t)
{
    if (t < 0.0 || t >= 0.12)
        return 0.0;

    return
        noise() *
        std::exp(-45.0 * t) *
        0.7;
}

double openHiHat(double t)
{
    if (t < 0.0 || t >= 0.8)
        return 0.0;

    return
        noise() *
        std::exp(-5.0 * t) *
        0.6;
}

double clap(double t)
{
    if (t < 0.0 || t >= 0.3)
        return 0.0;

    double burst1 =
        std::exp(
            -80.0 *
            std::abs(t)
        );

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
        )
        *
        envelope;
}

double tom(
    double t,
    double frequency)
{
    if (t < 0.0 || t >= 0.6)
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
        )
        *
        envelope;
}

double crash(double t)
{
    if (t < 0.0 || t >= 2.0)
        return 0.0;

    return
        noise() *
        std::exp(-2.5 * t) *
        0.65;
}

double ride(double t)
{
    if (t < 0.0 || t >= 1.5)
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
        )
        *
        envelope *
        0.4;
}

double rimshot(double t)
{
    if (t < 0.0 || t >= 0.15)
        return 0.0;

    double envelope =
        std::exp(-40.0 * t);

    return
        std::sin(
            2.0 *
            PI *
            1200.0 *
            t
        )
        *
        envelope *
        0.8
        +
        noise() *
        envelope *
        0.3;
}

double cowbell(double t)
{
    if (t < 0.0 || t >= 0.3)
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
        )
        *
        envelope *
        0.4;
}

double shaker(double t)
{
    if (t < 0.0 || t >= 0.25)
        return 0.0;

    return
        noise() *
        std::exp(-18.0 * t) *
        0.5;
}

double tambourine(double t)
{
    if (t < 0.0 || t >= 0.7)
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

std::string upper(
    std::string value)
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
// LOAD SONG TEXT
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

std::string GetEditorText()
{
    if (!songEditor)
        return "";

    int length =
        GetWindowTextLengthA(
            songEditor
        );

    if (length <= 0)
        return "";

    std::string text(
        length + 1,
        '\0'
    );

    int actualLength =
        GetWindowTextA(
            songEditor,
            &text[0],
            length + 1
        );

    text.resize(actualLength);

    return text;
}

// ============================================================
// VOLUME MULTIPLIER
// ============================================================

double GetVolumeMultiplier(
    int level)
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
            static_cast<double>(pitch) /
            12.0
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

#define ID_PLAY             1000
#define ID_LOOP             1001
#define ID_BOOST            1002

#define ID_TEMPO_MINUS      1003
#define ID_TEMPO_PLUS       1004

#define ID_PITCH_MINUS      1005
#define ID_PITCH_PLUS       1006

#define ID_SAVE_TRACK       1007

#define ID_TRACK_PLAY_BASE  3000
#define ID_TRACK_LOAD_BASE  4000

// ============================================================
// EDITOR BRUSH
// ============================================================

HBRUSH editBrush = nullptr;

// ============================================================
// UPDATE TEMPO
// ============================================================

void UpdateTempoDisplay()
{
    if (!tempoLabel)
        return;

    int tempo =
        static_cast<int>(
            std::round(
                currentTempo.load()
            )
        );

    char text[64];

    wsprintfA(
        text,
        "TEMPO: %d",
        tempo
    );

    SetWindowTextA(
        tempoLabel,
        text
    );
}

// ============================================================
// UPDATE PITCH
// ============================================================

void UpdatePitchDisplay()
{
    if (!pitchLabel)
        return;

    int pitch =
        currentPitch.load();

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
        pitchLabel,
        text
    );
}

// ============================================================
// UPDATE BOOST
// ============================================================

void UpdateBoostDisplay()
{
    if (!boostButton)
        return;

    int level =
        volumeBoost.load();

    double multiplier =
        GetVolumeMultiplier(level);

    char text[64];

    snprintf(
        text,
        sizeof(text),
        "BOOST: %.1fx",
        multiplier
    );

    SetWindowTextA(
        boostButton,
        text
    );
}

// ============================================================
// CONTENT HEIGHT
// ============================================================

void CalculateContentHeight(
    RECT rect)
{
    int trackRows =
        static_cast<int>(
            (savedTracks.size() + 3) / 4
        );

    contentHeight =
        TRACK_START_Y +
        trackRows *
        TRACK_ROW_HEIGHT +
        80;

    int minimumHeight =
        rect.bottom;

    if (contentHeight < minimumHeight)
        contentHeight = minimumHeight;
}

// ============================================================
// DELETE TRACK BUTTONS
// ============================================================

void DeleteTrackButtons()
{
    for (HWND button : trackPlayButtons)
    {
        if (button)
            DestroyWindow(button);
    }

    for (HWND button : trackLoadButtons)
    {
        if (button)
            DestroyWindow(button);
    }

    trackPlayButtons.clear();
    trackLoadButtons.clear();
}

// ============================================================
// CREATE TRACK BUTTONS
// ============================================================

void RebuildTrackButtons()
{
    if (!mainWindow)
        return;

    DeleteTrackButtons();

    HINSTANCE instance =
        (HINSTANCE)GetWindowLongPtrA(
            mainWindow,
            GWLP_HINSTANCE
        );

    for (
        int i = 0;
        i < static_cast<int>(
                savedTracks.size());
        ++i)
    {
        int playID =
            ID_TRACK_PLAY_BASE + i;

        int loadID =
            ID_TRACK_LOAD_BASE + i;

        HWND play =
            CreateWindowA(
                "BUTTON",
                savedTracks[i].name.c_str(),
                WS_VISIBLE |
                WS_CHILD |
                BS_PUSHBUTTON,
                0,
                0,
                TRACK_BUTTON_WIDTH,
                32,
                mainWindow,
                (HMENU)(INT_PTR)playID,
                instance,
                nullptr
            );

        HWND load =
            CreateWindowA(
                "BUTTON",
                "LOAD",
                WS_VISIBLE |
                WS_CHILD |
                BS_PUSHBUTTON,
                0,
                0,
                LOAD_BUTTON_WIDTH,
                32,
                mainWindow,
                (HMENU)(INT_PTR)loadID,
                instance,
                nullptr
            );

        trackPlayButtons.push_back(play);
        trackLoadButtons.push_back(load);
    }

    RECT rect;

    GetClientRect(
        mainWindow,
        &rect
    );

    CalculateContentHeight(rect);

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// RESIZE TRACK BUTTONS
// ============================================================

void ResizeTrackButtons()
{
    if (!mainWindow)
        return;

    RECT rect;

    GetClientRect(
        mainWindow,
        &rect
    );

    int availableWidth =
        rect.right -
        LEFT_MARGIN * 2;

    int columns = 4;

    int columnWidth =
        availableWidth /
        columns;

    if (columnWidth < 220)
        columnWidth = 220;

    for (
        int i = 0;
        i < static_cast<int>(
                trackPlayButtons.size());
        ++i)
    {
        int row =
            i / columns;

        int column =
            i % columns;

        int x =
            LEFT_MARGIN +
            column *
            columnWidth;

        int y =
            TRACK_START_Y +
            row *
            TRACK_ROW_HEIGHT -
            scrollY;

        MoveWindow(
            trackPlayButtons[i],
            x,
            y,
            TRACK_BUTTON_WIDTH,
            32,
            TRUE
        );

        MoveWindow(
            trackLoadButtons[i],
            x +
            TRACK_BUTTON_WIDTH +
            5,
            y,
            LOAD_BUTTON_WIDTH,
            32,
            TRUE
        );
    }
}

// ============================================================
// UPDATE SCROLL BAR - FORWARD DECLARATION
// ============================================================

void UpdateScrollBar(HWND window);

// ============================================================
// RESIZE ALL CONTROLS
// ============================================================

void ResizeControls()
{
    if (!mainWindow)
        return;

    RECT rect;

    GetClientRect(
        mainWindow,
        &rect
    );

    int width =
        rect.right -
        LEFT_MARGIN * 2;

    if (width < 300)
        width = 300;

    // --------------------------------------------------------
    // TOP CONTROLS
    // --------------------------------------------------------

    MoveWindow(
        playButton,
        LEFT_MARGIN,
        12 - scrollY,
        100,
        32,
        TRUE
    );

    MoveWindow(
        loopButton,
        LEFT_MARGIN + 105,
        12 - scrollY,
        110,
        32,
        TRUE
    );

    MoveWindow(
        boostButton,
        LEFT_MARGIN + 220,
        12 - scrollY,
        110,
        32,
        TRUE
    );

    // --------------------------------------------------------
    // TEMPO
    // --------------------------------------------------------

    MoveWindow(
        tempoMinusButton,
        LEFT_MARGIN,
        65 - scrollY,
        35,
        30,
        TRUE
    );

    MoveWindow(
        tempoLabel,
        LEFT_MARGIN + 40,
        65 - scrollY,
        120,
        30,
        TRUE
    );

    MoveWindow(
        tempoPlusButton,
        LEFT_MARGIN + 165,
        65 - scrollY,
        35,
        30,
        TRUE
    );

    // --------------------------------------------------------
    // PITCH
    // --------------------------------------------------------

    MoveWindow(
        pitchMinusButton,
        LEFT_MARGIN,
        102 - scrollY,
        35,
        30,
        TRUE
    );

    MoveWindow(
        pitchLabel,
        LEFT_MARGIN + 40,
        102 - scrollY,
        120,
        30,
        TRUE
    );

    MoveWindow(
        pitchPlusButton,
        LEFT_MARGIN + 165,
        102 - scrollY,
        35,
        30,
        TRUE
    );

    // --------------------------------------------------------
    // SAVE
    // --------------------------------------------------------

    MoveWindow(
        saveTrackButton,
        LEFT_MARGIN,
        145 - scrollY,
        200,
        35,
        TRUE
    );

    // --------------------------------------------------------
    // EDITOR
    // --------------------------------------------------------

    MoveWindow(
        songEditor,
        LEFT_MARGIN,
        EDITOR_TOP - scrollY,
        width,
        EDITOR_HEIGHT,
        TRUE
    );

    // --------------------------------------------------------
    // TRACK BUTTONS
    // --------------------------------------------------------

    ResizeTrackButtons();

    // --------------------------------------------------------
    // UPDATE SCROLL BAR
    // --------------------------------------------------------

    UpdateScrollBar(
        mainWindow
    );
}

// ============================================================
// UPDATE SCROLL BAR
// ============================================================

void UpdateScrollBar(
    HWND window)
{
    if (!window)
        return;

    RECT rect;

    GetClientRect(
        window,
        &rect
    );

    // Recalculate the total content height.
    CalculateContentHeight(
        rect
    );

    int visibleHeight =
        rect.bottom;

    if (visibleHeight < 1)
        visibleHeight = 1;

    int maximum =
        std::max(
            0,
            contentHeight -
            visibleHeight
        );

    if (scrollY > maximum)
        scrollY = maximum;

    if (scrollY < 0)
        scrollY = 0;

    SCROLLINFO si = {};

    si.cbSize =
        sizeof(SCROLLINFO);

    si.fMask =
        SIF_RANGE |
        SIF_PAGE |
        SIF_POS;

    si.nMin = 0;

    si.nMax =
        std::max(
            0,
            contentHeight - 1
        );

    si.nPage =
        static_cast<UINT>(
            visibleHeight
        );

    si.nPos =
        scrollY;

    SetScrollInfo(
        window,
        SB_VERT,
        &si,
        TRUE
    );
}
// ============================================================
// DRAW BACKGROUND / PANELS
// ============================================================

void DrawInterface(
    HDC dc,
    RECT rect)
{
    HBRUSH background =
        CreateSolidBrush(
            BACKGROUND
        );

    FillRect(
        dc,
        &rect,
        background
    );

    DeleteObject(background);

    // --------------------------------------------------------
    // TOP PANEL
    // --------------------------------------------------------

    RECT topPanel =
    {
        10,
        5 - scrollY,
        rect.right - 10,
        190 - scrollY
    };

    HBRUSH panelBrush =
        CreateSolidBrush(
            PANEL
        );

    FillRect(
        dc,
        &topPanel,
        panelBrush
    );

    DeleteObject(panelBrush);

    HPEN border =
        CreatePen(
            PS_SOLID,
            1,
            PANEL_BORDER
        );

    HPEN oldPen =
        (HPEN)SelectObject(
            dc,
            border
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
        topPanel.left,
        topPanel.top,
        topPanel.right,
        topPanel.bottom
    );

    SelectObject(
        dc,
        oldBrush
    );

    SelectObject(
        dc,
        oldPen
    );

    DeleteObject(border);

    // --------------------------------------------------------
    // EDITOR PANEL
    // --------------------------------------------------------

    RECT editorPanel =
    {
        10,
        210 - scrollY,
        rect.right - 10,
        EDITOR_TOP +
        EDITOR_HEIGHT +
        10 -
        scrollY
    };

    panelBrush =
        CreateSolidBrush(
            PANEL
        );

    FillRect(
        dc,
        &editorPanel,
        panelBrush
    );

    DeleteObject(panelBrush);

    border =
        CreatePen(
            PS_SOLID,
            1,
            PANEL_BORDER
        );

    oldPen =
        (HPEN)SelectObject(
            dc,
            border
        );

    oldBrush =
        (HBRUSH)SelectObject(
            dc,
            GetStockObject(
                NULL_BRUSH
            )
        );

    Rectangle(
        dc,
        editorPanel.left,
        editorPanel.top,
        editorPanel.right,
        editorPanel.bottom
    );

    SelectObject(
        dc,
        oldBrush
    );

    SelectObject(
        dc,
        oldPen
    );

    DeleteObject(border);

    // --------------------------------------------------------
    // TRACK LIBRARY
    // --------------------------------------------------------

    int trackRows =
        static_cast<int>(
            (savedTracks.size() + 3) / 4
        );

    RECT tracksPanel =
    {
        10,
        TRACK_START_Y - 45 - scrollY,
        rect.right - 10,
        TRACK_START_Y +
        std::max(
            1,
            trackRows
        ) *
        TRACK_ROW_HEIGHT +
        20 -
        scrollY
    };

    panelBrush =
        CreateSolidBrush(
            PANEL
        );

    FillRect(
        dc,
        &tracksPanel,
        panelBrush
    );

    DeleteObject(panelBrush);

    border =
        CreatePen(
            PS_SOLID,
            1,
            PANEL_BORDER
        );

    oldPen =
        (HPEN)SelectObject(
            dc,
            border
        );

    oldBrush =
        (HBRUSH)SelectObject(
            dc,
            GetStockObject(
                NULL_BRUSH
            )
        );

    Rectangle(
        dc,
        tracksPanel.left,
        tracksPanel.top,
        tracksPanel.right,
        tracksPanel.bottom
    );

    SelectObject(
        dc,
        oldBrush
    );

    SelectObject(
        dc,
        oldPen
    );

    DeleteObject(border);

    // --------------------------------------------------------
    // SECTION LABELS
    // --------------------------------------------------------

    SetTextColor(
        dc,
        TEXT_COLOR
    );

    SetBkMode(
        dc,
        TRANSPARENT
    );

    HFONT oldFont =
        (HFONT)SelectObject(
            dc,
            GetStockObject(
                DEFAULT_GUI_FONT
            )
        );

    TextOutA(
        dc,
        LEFT_MARGIN,
        198 - scrollY,
        "SONG EDITOR",
        11
    );

    TextOutA(
        dc,
        LEFT_MARGIN,
        TRACK_START_Y - 35 - scrollY,
        "SAVED TRACKS",
        11
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ============================================================
// PLAY SONG TEXT
// ============================================================

void PlaySongText(
    const std::string& songText)
{
    if (songText.empty())
        return;

    // Only one player is allowed.
    if (playing)
        return;

    playing = true;
    stopRequested = false;

    // --------------------------------------------------------
    // CHECK SONG
    // --------------------------------------------------------

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
        playing = false;
        return;
    }

    // --------------------------------------------------------
    // AUDIO FORMAT
    // --------------------------------------------------------

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
        playing = false;
        return;
    }

    // --------------------------------------------------------
    // BUFFERS
    // --------------------------------------------------------

    std::vector<short>
        audioSamples[
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

    // --------------------------------------------------------
    // GENERATE BUFFER
    // --------------------------------------------------------

    auto generateBuffer =
        [&](int bufferIndex) -> bool
    {
        double tempo =
            currentTempo.load();

        int pitch =
            currentPitch.load();

        int boost =
            volumeBoost.load();

        double multiplier =
            GetVolumeMultiplier(
                boost
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

    // --------------------------------------------------------
    // PREPARE + WRITE
    // --------------------------------------------------------

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

    // --------------------------------------------------------
    // FIRST BUFFER
    // --------------------------------------------------------

    if (!generateBuffer(0))
    {
        waveOutClose(
            audioDevice
        );

        playing = false;
        return;
    }

    if (!prepareAndWrite(0))
    {
        waveOutClose(
            audioDevice
        );

        playing = false;
        return;
    }

    // --------------------------------------------------------
    // SECOND BUFFER IF LOOPING
    // --------------------------------------------------------

    if (looping)
    {
        if (generateBuffer(1))
        {
            prepareAndWrite(1);
        }
    }

    // --------------------------------------------------------
    // PLAYBACK LOOP
    // --------------------------------------------------------

    while (
        !stopRequested)
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

            // No loop.
            if (!looping)
                continue;

            if (stopRequested)
                continue;

            if (!generateBuffer(i))
            {
                stopRequested = true;
                continue;
            }

            if (!prepareAndWrite(i))
            {
                stopRequested = true;
                continue;
            }
        }

        // ----------------------------------------------------
        // NON-LOOPING PLAYBACK FINISHED
        // ----------------------------------------------------

        if (!looping)
        {
            bool anythingQueued =
                false;

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

    // --------------------------------------------------------
    // STOP AUDIO
    // --------------------------------------------------------

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

    playing = false;

    PostMessageA(
        mainWindow,
        WM_USER + 1,
        0,
        0
    );
}

// ============================================================
// START EDITOR PLAYBACK
// ============================================================

void PlayEditor()
{
    if (playing)
    {
        stopRequested = true;
        return;
    }

    std::string text =
        GetEditorText();

    if (text.empty())
        return;

    std::thread(
        PlaySongText,
        text
    ).detach();
}

// ============================================================
// PLAY SAVED TRACK
// ============================================================

void PlaySavedTrack(
    int trackIndex)
{
    if (
        trackIndex < 0 ||
        trackIndex >=
            static_cast<int>(
                savedTracks.size()))
        return;

    if (playing)
    {
        stopRequested = true;
        return;
    }

    std::string text =
        savedTracks[
            trackIndex
        ].text;

    std::thread(
        PlaySongText,
        text
    ).detach();
}

// ============================================================
// LOAD SAVED TRACK
// ============================================================

void LoadSavedTrack(
    int trackIndex)
{
    if (
        trackIndex < 0 ||
        trackIndex >=
            static_cast<int>(
                savedTracks.size()))
        return;

    // Loading never plays.
    SetWindowTextA(
        songEditor,
        savedTracks[
            trackIndex
        ].text.c_str()
    );

    // Put the cursor at the beginning.
    SendMessageA(
        songEditor,
        EM_SETSEL,
        0,
        0
    );

    SetFocus(
        songEditor
    );
}

// ============================================================
// SAVE CURRENT TRACK
// ============================================================

void SaveCurrentTrack()
{
    std::string text =
        GetEditorText();

    if (text.empty())
        return;

    // --------------------------------------------------------
    // VERIFY THERE IS ACTUALLY A SONG
    // --------------------------------------------------------

    std::vector<NoteEvent> notes;
    std::vector<DrumEvent> drums;

    double tempo = 120.0;
    double length = 4.0;

    LoadSongText(
        text,
        notes,
        drums,
        tempo,
        length
    );

    if (
        notes.empty() &&
        drums.empty())
    {
        MessageBoxA(
            mainWindow,
            "There are no valid notes or drums to save.",
            "Cannot Save Track",
            MB_OK |
            MB_ICONWARNING
        );

        return;
    }

    // --------------------------------------------------------
    // SAVE
    // --------------------------------------------------------

    SavedTrack track;

    track.name =
        "TRACK " +
        std::to_string(
            savedTracks.size() + 1
        );

    track.text = text;

    savedTracks.push_back(
        track
    );

    // --------------------------------------------------------
    // CLEAR EDITOR
    // --------------------------------------------------------

    SetWindowTextA(
        songEditor,
        ""
    );

    // --------------------------------------------------------
    // REBUILD TRACK LIST
    // --------------------------------------------------------

    RebuildTrackButtons();

    ResizeControls();

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
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
        // BUTTON COLORS
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

            // ------------------------------------------------
            // PLAY
            // ------------------------------------------------

            if (id == ID_PLAY)
            {
                if (!playing)
                {
                    SetWindowTextA(
                        playButton,
                        "STOP"
                    );

                    PlayEditor();
                }
                else
                {
                    stopRequested = true;

                    SetWindowTextA(
                        playButton,
                        "PLAY"
                    );
                }

                break;
            }

            // ------------------------------------------------
            // LOOP
            // ------------------------------------------------

            if (id == ID_LOOP)
            {
                looping =
                    !looping.load();

                SetWindowTextA(
                    loopButton,
                    looping
                        ? "LOOP: ON"
                        : "LOOP: OFF"
                );

                break;
            }

            // ------------------------------------------------
            // BOOST
            // ------------------------------------------------

            if (id == ID_BOOST)
            {
                int level =
                    volumeBoost.load();

                level++;

                if (level > 3)
                    level = 0;

                volumeBoost =
                    level;

                UpdateBoostDisplay();

                break;
            }

            // ------------------------------------------------
            // TEMPO MINUS
            // ------------------------------------------------

            if (id == ID_TEMPO_MINUS)
            {
                double tempo =
                    currentTempo.load();

                tempo -= TEMPO_STEP;

                tempo =
                    std::max(
                        MIN_TEMPO,
                        tempo
                    );

                currentTempo =
                    tempo;

                UpdateTempoDisplay();

                break;
            }

            // ------------------------------------------------
            // TEMPO PLUS
            // ------------------------------------------------

            if (id == ID_TEMPO_PLUS)
            {
                double tempo =
                    currentTempo.load();

                tempo += TEMPO_STEP;

                tempo =
                    std::min(
                        MAX_TEMPO,
                        tempo
                    );

                currentTempo =
                    tempo;

                UpdateTempoDisplay();

                break;
            }

            // ------------------------------------------------
            // PITCH MINUS
            // ------------------------------------------------

            if (id == ID_PITCH_MINUS)
            {
                int pitch =
                    currentPitch.load();

                pitch -= PITCH_STEP;

                pitch =
                    std::max(
                        MIN_PITCH,
                        pitch
                    );

                currentPitch =
                    pitch;

                UpdatePitchDisplay();

                break;
            }

            // ------------------------------------------------
            // PITCH PLUS
            // ------------------------------------------------

            if (id == ID_PITCH_PLUS)
            {
                int pitch =
                    currentPitch.load();

                pitch += PITCH_STEP;

                pitch =
                    std::min(
                        MAX_PITCH,
                        pitch
                    );

                currentPitch =
                    pitch;

                UpdatePitchDisplay();

                break;
            }

            // ------------------------------------------------
            // SAVE TRACK
            // ------------------------------------------------

            if (id == ID_SAVE_TRACK)
            {
                SaveCurrentTrack();
                break;
            }

            // ------------------------------------------------
            // SAVED TRACK PLAY BUTTONS
            // ------------------------------------------------

            if (
                id >= ID_TRACK_PLAY_BASE &&
                id <
                    ID_TRACK_PLAY_BASE +
                    static_cast<int>(
                        savedTracks.size()))
            {
                int trackIndex =
                    id -
                    ID_TRACK_PLAY_BASE;

                PlaySavedTrack(
                    trackIndex
                );

                break;
            }

            // ------------------------------------------------
            // SAVED TRACK LOAD BUTTONS
            // ------------------------------------------------

            if (
                id >= ID_TRACK_LOAD_BASE &&
                id <
                    ID_TRACK_LOAD_BASE +
                    static_cast<int>(
                        savedTracks.size()))
            {
                int trackIndex =
                    id -
                    ID_TRACK_LOAD_BASE;

                LoadSavedTrack(
                    trackIndex
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
                playButton,
                "PLAY"
            );

            break;
        }

        // ====================================================
        // VERTICAL SCROLL
        // ====================================================

        case WM_VSCROLL:
        {
            SCROLLINFO si = {};

            si.cbSize =
                sizeof(SCROLLINFO);

            si.fMask =
                SIF_ALL;

            GetScrollInfo(
                window,
                SB_VERT,
                &si
            );

            int oldPos =
                si.nPos;

            int newPos =
                oldPos;

            switch (
                LOWORD(wParam))
            {
                case SB_LINEUP:
                    newPos -= 40;
                    break;

                case SB_LINEDOWN:
                    newPos += 40;
                    break;

                case SB_PAGEUP:
                    newPos -=
                        static_cast<int>(
                            si.nPage
                        );
                    break;

                case SB_PAGEDOWN:
                    newPos +=
                        static_cast<int>(
                            si.nPage
                        );
                    break;

                case SB_THUMBTRACK:
                    newPos =
                        si.nTrackPos;
                    break;

                case SB_TOP:
                    newPos =
                        si.nMin;
                    break;

                case SB_BOTTOM:
                    newPos =
                        si.nMax;
                    break;

                default:
                    break;
            }

            int maxPos =
                si.nMax -
                static_cast<int>(
                    si.nPage
                ) +
                1;

            if (maxPos < 0)
                maxPos = 0;

            newPos =
                std::max(
                    si.nMin,
                    std::min(
                        newPos,
                        maxPos
                    )
                );

            if (newPos != oldPos)
            {
                scrollY =
                    newPos;

                SetScrollPos(
                    window,
                    SB_VERT,
                    scrollY,
                    TRUE
                );

                ResizeControls();

                InvalidateRect(
                    window,
                    nullptr,
                    TRUE
                );
            }

            break;
        }

        // ====================================================
        // MOUSE WHEEL
        // ====================================================

        case WM_MOUSEWHEEL:
        {
            int delta =
                GET_WHEEL_DELTA_WPARAM(
                    wParam
                );

            SCROLLINFO si = {};

            si.cbSize =
                sizeof(SCROLLINFO);

            si.fMask =
                SIF_ALL;

            GetScrollInfo(
                window,
                SB_VERT,
                &si
            );

            int amount =
                delta > 0
                    ? -80
                    : 80;

            int newPos =
                si.nPos +
                amount;

            int maxPos =
                si.nMax -
                static_cast<int>(
                    si.nPage
                ) +
                1;

            if (maxPos < 0)
                maxPos = 0;

            newPos =
                std::max(
                    0,
                    std::min(
                        newPos,
                        maxPos
                    )
                );

            scrollY =
                newPos;

            SetScrollPos(
                window,
                SB_VERT,
                scrollY,
                TRUE
            );

            ResizeControls();

            InvalidateRect(
                window,
                nullptr,
                TRUE
            );

            break;
        }

        // ====================================================
        // RESIZE
        // ====================================================

        case WM_SIZE:
        {
            ResizeControls();

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

            DrawInterface(
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
            stopRequested = true;

            // Give the audio thread a moment to
            // notice the stop request.
            Sleep(50);

            DeleteTrackButtons();

            if (editBrush)
            {
                DeleteObject(
                    editBrush
                );

                editBrush = nullptr;
            }

            PostQuitMessage(0);

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
        "CppSongMakerSingle";

    // ========================================================
    // INITIAL STATE
    // ========================================================

    playing = false;
    looping = false;
    stopRequested = false;

    currentTempo = 120.0;
    currentPitch = 0;
    volumeBoost = 0;

    // ========================================================
    // EDITOR BRUSH
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
            "C++ Song Maker - Single Player",
            WS_OVERLAPPEDWINDOW |
            WS_VSCROLL,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            900,
            850,
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
    // PLAY BUTTON
    // ========================================================

    playButton =
        CreateWindowA(
            "BUTTON",
            "PLAY",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            100,
            32,
            window,
            (HMENU)(INT_PTR)
                ID_PLAY,
            instance,
            nullptr
        );

    // ========================================================
    // LOOP BUTTON
    // ========================================================

    loopButton =
        CreateWindowA(
            "BUTTON",
            "LOOP: OFF",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            110,
            32,
            window,
            (HMENU)(INT_PTR)
                ID_LOOP,
            instance,
            nullptr
        );

    // ========================================================
    // BOOST BUTTON
    // ========================================================

    boostButton =
        CreateWindowA(
            "BUTTON",
            "BOOST: 1.0x",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            110,
            32,
            window,
            (HMENU)(INT_PTR)
                ID_BOOST,
            instance,
            nullptr
        );

    // ========================================================
    // TEMPO MINUS
    // ========================================================

    tempoMinusButton =
        CreateWindowA(
            "BUTTON",
            "-",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            35,
            30,
            window,
            (HMENU)(INT_PTR)
                ID_TEMPO_MINUS,
            instance,
            nullptr
        );

    // ========================================================
    // TEMPO LABEL
    // ========================================================

    tempoLabel =
        CreateWindowA(
            "STATIC",
            "TEMPO: 120",
            WS_VISIBLE |
            WS_CHILD |
            SS_CENTER |
            SS_CENTERIMAGE,
            0,
            0,
            120,
            30,
            window,
            nullptr,
            instance,
            nullptr
        );

    // ========================================================
    // TEMPO PLUS
    // ========================================================

    tempoPlusButton =
        CreateWindowA(
            "BUTTON",
            "+",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            35,
            30,
            window,
            (HMENU)(INT_PTR)
                ID_TEMPO_PLUS,
            instance,
            nullptr
        );

    // ========================================================
    // PITCH MINUS
    // ========================================================

    pitchMinusButton =
        CreateWindowA(
            "BUTTON",
            "-",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            35,
            30,
            window,
            (HMENU)(INT_PTR)
                ID_PITCH_MINUS,
            instance,
            nullptr
        );

    // ========================================================
    // PITCH LABEL
    // ========================================================

    pitchLabel =
        CreateWindowA(
            "STATIC",
            "PITCH: 0",
            WS_VISIBLE |
            WS_CHILD |
            SS_CENTER |
            SS_CENTERIMAGE,
            0,
            0,
            120,
            30,
            window,
            nullptr,
            instance,
            nullptr
        );

    // ========================================================
    // PITCH PLUS
    // ========================================================

    pitchPlusButton =
        CreateWindowA(
            "BUTTON",
            "+",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            35,
            30,
            window,
            (HMENU)(INT_PTR)
                ID_PITCH_PLUS,
            instance,
            nullptr
        );

    // ========================================================
    // SAVE TRACK
    // ========================================================

    saveTrackButton =
        CreateWindowA(
            "BUTTON",
            "SAVE TRACK",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            200,
            35,
            window,
            (HMENU)(INT_PTR)
                ID_SAVE_TRACK,
            instance,
            nullptr
        );

    // ========================================================
    // SONG EDITOR
    // ========================================================

    songEditor =
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
            700,
            EDITOR_HEIGHT,
            window,
            nullptr,
            instance,
            nullptr
        );

    SendMessageA(
        songEditor,
        WM_SETFONT,
        (WPARAM)GetStockObject(
            DEFAULT_GUI_FONT
        ),
        TRUE
    );

    // ========================================================
    // INITIAL LAYOUT
    // ========================================================

    ResizeControls();

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
