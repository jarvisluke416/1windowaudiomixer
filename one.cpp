#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
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
// LAYOUT
// ============================================================

const int LEFT_MARGIN = 20;

const int TOP_AREA = 185;

const int EDITOR_TOP = 195;

const int EDITOR_HEIGHT = 330;

const int TRACK_TOP = 555;

const int TRACK_BUTTON_HEIGHT = 35;

const int TRACK_GAP = 8;

const int BOTTOM_MARGIN = 40;

// ============================================================
// AUDIO
// ============================================================

const int AUDIO_BUFFER_COUNT = 2;

// ============================================================
// PLAYER STATE
// ============================================================

std::atomic_bool playing(false);
std::atomic_bool looping(false);
std::atomic_bool stopRequested(false);

std::atomic<double> currentTempo(120.0);
std::atomic<int> currentPitch(0);

std::atomic<int> volumeBoost(0);

// ============================================================
// SAVED TRACKS
// ============================================================

std::vector<std::string> savedTracks;

// ============================================================
// WINDOWS
// ============================================================

HWND mainWindow = nullptr;

HWND playButton = nullptr;
HWND loopButton = nullptr;
HWND boostButton = nullptr;

HWND tempoMinusButton = nullptr;
HWND tempoPlusButton = nullptr;
HWND tempoLabel = nullptr;

HWND pitchMinusButton = nullptr;
HWND pitchPlusButton = nullptr;
HWND pitchLabel = nullptr;

HWND saveTrackButton = nullptr;

HWND songEditor = nullptr;

HWND instrumentsButton = nullptr;
HWND chordsButton = nullptr;

// ============================================================
// SCROLLING
// ============================================================

int scrollY = 0;
int contentHeight = 800;

// ============================================================
// BRUSH
// ============================================================

HBRUSH editBrush = nullptr;

// ============================================================
// TRACK BUTTONS
// ============================================================

std::vector<HWND> trackButtons;
std::vector<HWND> loadButtons;

// ============================================================
// IDs
// ============================================================

#define ID_PLAY             1001
#define ID_LOOP             1002
#define ID_BOOST            1003

#define ID_TEMPO_MINUS      1004
#define ID_TEMPO_PLUS       1005

#define ID_PITCH_MINUS      1006
#define ID_PITCH_PLUS       1007

#define ID_SAVE             1008

#define ID_INSTRUMENTS      1009
#define ID_CHORDS           1010

#define ID_TRACK_BASE       2000
#define ID_LOAD_BASE        3000

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
// NOTE FREQUENCY
// ============================================================

double noteFrequency(
    const std::string& note)
{
    static const double frequencies[] =
    {
        16.35, 17.32, 18.35, 19.45,
        20.60, 21.83, 23.12, 24.50,
        25.96, 27.50, 29.14, 30.87,

        32.70, 34.65, 36.71, 38.89,
        41.20, 43.65, 46.25, 49.00,
        51.91, 55.00, 58.27, 61.74,

        65.41, 69.30, 73.42, 77.78,
        82.41, 87.31, 92.50, 98.00,
        103.83, 110.00, 116.54, 123.47,

        130.81, 138.59, 146.83, 155.56,
        164.81, 174.61, 185.00, 196.00,
        207.65, 220.00, 233.08, 246.94,

        261.63, 277.18, 293.66, 311.13,
        329.63, 349.23, 369.99, 392.00,
        415.30, 440.00, 466.16, 493.88,

        523.25, 554.37, 587.33, 622.25,
        659.25, 698.46, 739.99, 783.99,
        830.61, 880.00, 932.33, 987.77,

        1046.50, 1108.73, 1174.66, 1244.51,
        1318.51, 1396.91, 1479.98, 1567.98,
        1661.22, 1760.00, 1864.66, 1975.53
    };

    static const char* names[] =
    {
        "C1","C#1","D1","D#1","E1","F1",
        "F#1","G1","G#1","A1","A#1","B1",

        "C2","C#2","D2","D#2","E2","F2",
        "F#2","G2","G#2","A2","A#2","B2",

        "C3","C#3","D3","D#3","E3","F3",
        "F#3","G3","G#3","A3","A#3","B3",

        "C4","C#4","D4","D#4","E4","F4",
        "F#4","G4","G#4","A4","A#4","B4",

        "C5","C#5","D5","D#5","E5","F5",
        "F#5","G5","G#5","A5","A#5","B5",

        "C6","C#6","D6","D#6","E6","F6",
        "F#6","G6","G#6","A6","A#6","B6",

        "C7","C#7","D7","D#7","E7","F7",
        "F#7","G7","G#7","A7","A#7","B7"
    };

    const int count =
        sizeof(frequencies) /
        sizeof(frequencies[0]);

    for (int i = 0; i < count; ++i)
    {
        if (note == names[i])
            return frequencies[i];
    }

    return 0.0;
}

// ============================================================
// INSTRUMENT WAVE
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

    if (instrument == "ORGAN")
    {
        return
            std::sin(phase) * 0.65 +
            std::sin(phase * 2.0) * 0.25 +
            std::sin(phase * 4.0) * 0.12 +
            std::sin(phase * 8.0) * 0.04;
    }

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

    if (instrument == "TRUMPET")
    {
        double attack =
            1.0 -
            std::exp(-45.0 * t);

        double brass =
            std::sin(phase) * 0.45 +
            std::sin(phase * 2.0) * 0.30 +
            std::sin(phase * 3.0) * 0.28 +
            std::sin(phase * 4.0) * 0.20 +
            std::sin(phase * 5.0) * 0.14 +
            std::sin(phase * 6.0) * 0.08;

        return
            brass *
            attack;
    }

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

    return
        std::sin(
            2.0 *
            PI *
            frequency *
            t
        )
        *
        envelope *
        1.25;
}

double snare(double t)
{
    if (t < 0.0 || t >= 0.35)
        return 0.0;

    double envelope =
        std::exp(-15.0 * t);

    return
        noise() *
        envelope *
        0.8
        +
        std::sin(
            2.0 *
            PI *
            180.0 *
            t
        )
        *
        std::exp(-20.0 * t)
        *
        0.25;
}

double closedHiHat(double t)
{
    if (t < 0.0 || t >= 0.12)
        return 0.0;

    return
        noise() *
        std::exp(-45.0 * t)
        *
        0.7;
}

double openHiHat(double t)
{
    if (t < 0.0 || t >= 0.8)
        return 0.0;

    return
        noise() *
        std::exp(-5.0 * t)
        *
        0.6;
}

double clap(double t)
{
    if (t < 0.0 || t >= 0.3)
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

    return
        noise() *
        (
            burst1 +
            burst2 +
            burst3
        )
        *
        std::exp(-14.0 * t);
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

    return
        std::sin(
            2.0 *
            PI *
            pitch *
            t
        )
        *
        std::exp(-7.0 * t);
}

double crash(double t)
{
    if (t < 0.0 || t >= 2.0)
        return 0.0;

    return
        noise() *
        std::exp(-2.5 * t)
        *
        0.65;
}

double ride(double t)
{
    if (t < 0.0 || t >= 1.5)
        return 0.0;

    double envelope =
        std::exp(-2.0 * t);

    return
        (
            noise() * 0.5 +
            std::sin(
                2.0 *
                PI *
                3500.0 *
                t
            ) *
            0.5
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

    return
        (
            std::sin(
                2.0 *
                PI *
                540.0 *
                t
            )
            +
            std::sin(
                2.0 *
                PI *
                800.0 *
                t
            )
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
        std::exp(-18.0 * t)
        *
        0.5;
}

double tambourine(double t)
{
    if (t < 0.0 || t >= 0.7)
        return 0.0;

    return
        noise() *
        std::exp(-6.0 * t)
        *
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
// LOAD SONG TEXT
// ============================================================

bool LoadSongText(
    const std::string& text,
    std::vector<NoteEvent>& notes,
    std::vector<DrumEvent>& drums,
    double& tempo,
    double& length)
{
    notes.clear();
    drums.clear();

    tempo = 120.0;
    length = 8.0;

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
            input >> length;

            if (length <= 0.0)
                length = 8.0;
        }
        else if (
            command == "NOTE" ||
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
            double duration;

            input >>
                noteName >>
                startBeat >>
                duration;

            double frequency =
                noteFrequency(
                    upper(noteName)
                );

            if (frequency > 0.0)
            {
                std::string instrument =
                    command;

                if (command == "NOTE")
                    instrument = "PIANO";

                notes.push_back(
                    {
                        startBeat,
                        duration,
                        frequency,
                        instrument
                    }
                );
            }
        }
        else if (command == "DRUM")
        {
            std::string type;
            double startBeat;

            input >>
                type >>
                startBeat;

            drums.push_back(
                {
                    startBeat,
                    upper(type)
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

    int actual =
        GetWindowTextA(
            songEditor,
            &text[0],
            length + 1
        );

    text.resize(actual);

    return text;
}

// ============================================================
// VOLUME MULTIPLIER
// ============================================================

double GetVolumeMultiplier(
    int level)
{
    if (level == 1)
        return 2.0;

    if (level == 2)
        return 3.5;

    if (level == 3)
        return 5.0;

    return 1.0;
}

// ============================================================
// GENERATE AUDIO
// ============================================================

bool GenerateAudio(
    const std::string& text,
    double tempo,
    int pitch,
    double volumeMultiplier,
    std::vector<short>& samples)
{
    std::vector<NoteEvent> notes;
    std::vector<DrumEvent> drums;

    double fileTempo;
    double loopLength;

    LoadSongText(
        text,
        notes,
        drums,
        fileTempo,
        loopLength
    );

    tempo =
        std::max(
            MIN_TEMPO,
            std::min(
                MAX_TEMPO,
                tempo
            )
        );

    pitch =
        std::max(
            MIN_PITCH,
            std::min(
                MAX_PITCH,
                pitch
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

    double duration =
        loopLength *
        secondsPerBeat;

    int totalSamples =
        static_cast<int>(
            std::round(
                duration *
                SAMPLE_RATE
            )
        );

    if (totalSamples <= 0)
        return false;

    std::vector<double> audio(
        totalSamples,
        0.0
    );

    // --------------------------------------------------------
    // NOTES
    // --------------------------------------------------------

    for (const NoteEvent& note : notes)
    {
        int start =
            static_cast<int>(
                std::round(
                    note.startBeat *
                    secondsPerBeat *
                    SAMPLE_RATE
                )
            );

        int count =
            static_cast<int>(
                std::round(
                    note.durationBeats *
                    secondsPerBeat *
                    SAMPLE_RATE
                )
            );

        double frequency =
            note.frequency *
            pitchMultiplier;

        for (int i = 0; i < count; ++i)
        {
            int index =
                start + i;

            if (index < 0)
                continue;

            if (index >= totalSamples)
                break;

            double time =
                static_cast<double>(i) /
                SAMPLE_RATE;

            double envelope = 1.0;

            if (time < 0.01)
                envelope = time / 0.01;

            double remaining =
                note.durationBeats *
                secondsPerBeat -
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

            audio[index] +=
                instrumentWave(
                    note.instrument,
                    frequency,
                    time
                )
                *
                envelope
                *
                0.55;
        }
    }

    // --------------------------------------------------------
    // DRUMS
    // --------------------------------------------------------

    for (const DrumEvent& drum : drums)
    {
        int start =
            static_cast<int>(
                std::round(
                    drum.startBeat *
                    secondsPerBeat *
                    SAMPLE_RATE
                )
            );

        int count =
            static_cast<int>(
                2.0 *
                SAMPLE_RATE
            );

        double volume = 1.5;

        if (
            drum.type == "KICK" ||
            drum.type == "BASS_DRUM")
            volume = 2.25;

        else if (drum.type == "SNARE")
            volume = 1.75;

        else if (
            drum.type == "HIHAT" ||
            drum.type == "CLOSED_HIHAT")
            volume = 1.10;

        else if (drum.type == "OPEN_HIHAT")
            volume = 1.30;

        for (int i = 0; i < count; ++i)
        {
            int index =
                start + i;

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
                volume;
        }
    }

    // --------------------------------------------------------
    // FINAL MIX
    // --------------------------------------------------------

    samples.resize(
        totalSamples
    );

    for (int i = 0;
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
// PLAY A SONG
// ============================================================

void PlaySong(
    const std::string& songText)
{
    if (songText.empty())
        return;

    if (playing)
        return;

    playing = true;
    stopRequested = false;

    WAVEFORMATEX format = {};

    format.wFormatTag =
        WAVE_FORMAT_PCM;

    format.nChannels = 1;

    format.nSamplesPerSec =
        SAMPLE_RATE;

    format.wBitsPerSample = 16;

    format.nBlockAlign =
        format.nChannels *
        format.wBitsPerSample /
        8;

    format.nAvgBytesPerSec =
        format.nSamplesPerSec *
        format.nBlockAlign;

    HWAVEOUT device = nullptr;

    if (
        waveOutOpen(
            &device,
            WAVE_MAPPER,
            &format,
            0,
            0,
            CALLBACK_NULL
        )
        != MMSYSERR_NOERROR)
    {
        playing = false;
        return;
    }

    while (!stopRequested)
    {
        std::vector<short> samples;

        GenerateAudio(
            songText,
            currentTempo.load(),
            currentPitch.load(),
            GetVolumeMultiplier(
                volumeBoost.load()
            ),
            samples
        );

        if (samples.empty())
            break;

        WAVEHDR header = {};

        header.lpData =
            reinterpret_cast<LPSTR>(
                samples.data()
            );

        header.dwBufferLength =
            static_cast<DWORD>(
                samples.size() *
                sizeof(short)
            );

        if (
            waveOutPrepareHeader(
                device,
                &header,
                sizeof(header)
            )
            != MMSYSERR_NOERROR)
        {
            break;
        }

        if (
            waveOutWrite(
                device,
                &header,
                sizeof(header)
            )
            != MMSYSERR_NOERROR)
        {
            waveOutUnprepareHeader(
                device,
                &header,
                sizeof(header)
            );

            break;
        }

        while (
            !(header.dwFlags &
              WHDR_DONE))
        {
            if (stopRequested)
                break;

            Sleep(2);
        }

        waveOutUnprepareHeader(
            device,
            &header,
            sizeof(header)
        );

        if (!looping)
            break;
    }

    waveOutReset(device);

    waveOutClose(device);

    playing = false;

    PostMessage(
        mainWindow,
        WM_USER + 1,
        0,
        0
    );
}

// ============================================================
// UPDATE TEMPO
// ============================================================

void UpdateTempoDisplay()
{
    if (!tempoLabel)
        return;

    char text[64];

    std::snprintf(
        text,
        sizeof(text),
        "TEMPO: %d",
        static_cast<int>(
            std::round(
                currentTempo.load()
            )
        )
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

    char text[64];

    int pitch =
        currentPitch.load();

    if (pitch > 0)
    {
        std::snprintf(
            text,
            sizeof(text),
            "PITCH: +%d",
            pitch
        );
    }
    else
    {
        std::snprintf(
            text,
            sizeof(text),
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

    char text[64];

    std::snprintf(
        text,
        sizeof(text),
        "BOOST: %.1fx",
        GetVolumeMultiplier(
            volumeBoost.load()
        )
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
    int trackCount =
        static_cast<int>(
            savedTracks.size()
        );

    int trackRows =
        trackCount;

    if (trackRows < 1)
        trackRows = 1;

    int tracksBottom =
        TRACK_TOP +
        trackRows *
        (
            TRACK_BUTTON_HEIGHT +
            TRACK_GAP
        ) +
        BOTTOM_MARGIN;

    contentHeight =
        tracksBottom;

    if (contentHeight < rect.bottom)
    {
        contentHeight =
            rect.bottom;
    }
}

// ============================================================
// UPDATE SCROLL BAR
// ============================================================

void UpdateScrollBar(
    HWND window)
{
    RECT rect;

    GetClientRect(
        window,
        &rect
    );

    CalculateContentHeight(
        rect
    );

    int visibleHeight =
        rect.bottom;

    if (visibleHeight < 1)
        visibleHeight = 1;

    int maximum =
        contentHeight -
        visibleHeight;

    if (maximum < 0)
        maximum = 0;

    if (scrollY > maximum)
        scrollY = maximum;

    SCROLLINFO si = {};

    si.cbSize =
        sizeof(SCROLLINFO);

    si.fMask =
        SIF_RANGE |
        SIF_PAGE |
        SIF_POS;

    si.nMin = 0;

    si.nMax =
        contentHeight;

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

    int width =
        rect.right -
        LEFT_MARGIN * 2;

    if (width < 300)
        width = 300;

    int rowY =
        TRACK_TOP -
        scrollY;

    int buttonWidth =
        150;

    for (
        size_t i = 0;
        i < trackButtons.size();
        ++i)
    {
        int y =
            rowY +
            static_cast<int>(i) *
            (
                TRACK_BUTTON_HEIGHT +
                TRACK_GAP
            );

        MoveWindow(
            trackButtons[i],
            LEFT_MARGIN,
            y,
            buttonWidth,
            TRACK_BUTTON_HEIGHT,
            TRUE
        );

        MoveWindow(
            loadButtons[i],
            LEFT_MARGIN +
            buttonWidth +
            8,
            y,
            80,
            TRACK_BUTTON_HEIGHT,
            TRUE
        );
    }
}

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
    // TOP BUTTONS
    // --------------------------------------------------------

    MoveWindow(
        playButton,
        LEFT_MARGIN,
        12 - scrollY,
        80,
        30,
        TRUE
    );

    MoveWindow(
        loopButton,
        LEFT_MARGIN + 85,
        12 - scrollY,
        90,
        30,
        TRUE
    );

    MoveWindow(
        boostButton,
        LEFT_MARGIN + 180,
        12 - scrollY,
        90,
        30,
        TRUE
    );

    // --------------------------------------------------------
    // INSTRUMENTS / CHORDS
    // --------------------------------------------------------

    MoveWindow(
        instrumentsButton,
        LEFT_MARGIN + 275,
        12 - scrollY,
        100,
        30,
        TRUE
    );

    MoveWindow(
        chordsButton,
        LEFT_MARGIN + 380,
        12 - scrollY,
        85,
        30,
        TRUE
    );

    // --------------------------------------------------------
    // TEMPO
    // --------------------------------------------------------

    MoveWindow(
        tempoMinusButton,
        LEFT_MARGIN,
        55 - scrollY,
        35,
        28,
        TRUE
    );

    MoveWindow(
        tempoLabel,
        LEFT_MARGIN + 40,
        55 - scrollY,
        120,
        28,
        TRUE
    );

    MoveWindow(
        tempoPlusButton,
        LEFT_MARGIN + 165,
        55 - scrollY,
        35,
        28,
        TRUE
    );

    // --------------------------------------------------------
    // PITCH
    // --------------------------------------------------------

    MoveWindow(
        pitchMinusButton,
        LEFT_MARGIN,
        88 - scrollY,
        35,
        28,
        TRUE
    );

    MoveWindow(
        pitchLabel,
        LEFT_MARGIN + 40,
        88 - scrollY,
        120,
        28,
        TRUE
    );

    MoveWindow(
        pitchPlusButton,
        LEFT_MARGIN + 165,
        88 - scrollY,
        35,
        28,
        TRUE
    );

    // --------------------------------------------------------
    // SAVE
    // --------------------------------------------------------

    MoveWindow(
        saveTrackButton,
        LEFT_MARGIN,
        125 - scrollY,
        180,
        32,
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

    ResizeTrackButtons();

    UpdateScrollBar(
        mainWindow
    );
}

// ============================================================
// ADD TEXT TO EDITOR
// ============================================================

void AddTextToEditor(
    const std::string& text)
{
    if (!songEditor)
        return;

    int length =
        GetWindowTextLengthA(
            songEditor
        );

    if (length > 0)
    {
        SendMessageA(
            songEditor,
            EM_SETSEL,
            length,
            length
        );

        SendMessageA(
            songEditor,
            EM_REPLACESEL,
            FALSE,
            reinterpret_cast<LPARAM>(
                "\r\n"
            )
        );
    }

    SendMessageA(
        songEditor,
        EM_SETSEL,
        -1,
        -1
    );

    SendMessageA(
        songEditor,
        EM_REPLACESEL,
        FALSE,
        reinterpret_cast<LPARAM>(
            text.c_str()
        )
    );
}

// ============================================================
// INSTRUMENT LIST
// ============================================================

std::vector<std::string> GetInstruments()
{
    return
    {
        "BASS",
        "BELL",
        "FLUTE",
        "GUITAR",
        "ORGAN",
        "PIANO",
        "SYNTH",
        "TRUMPET"
    };
}

// ============================================================
// NOTE LIST
// ============================================================

std::vector<std::string> GetNotes()
{
    std::vector<std::string> notes;

    const char* names[] =
    {
        "C",
        "C#",
        "D",
        "D#",
        "E",
        "F",
        "F#",
        "G",
        "G#",
        "A",
        "A#",
        "B"
    };

    for (int octave = 2;
         octave <= 6;
         ++octave)
    {
        for (int i = 0; i < 12; ++i)
        {
            notes.push_back(
                std::string(names[i]) +
                std::to_string(octave)
            );
        }
    }

    return notes;
}

// ============================================================
// CHORD INFORMATION
// ============================================================

struct ChordDefinition
{
    std::string root;
    std::string type;
    std::vector<int> intervals;
};

// ============================================================
// GET CHORDS
// ============================================================

std::vector<ChordDefinition>
GetChords()
{
    std::vector<ChordDefinition> chords;

    const char* roots[] =
    {
        "A",
        "A#",
        "B",
        "C",
        "C#",
        "D",
        "D#",
        "E",
        "F",
        "F#",
        "G",
        "G#"
    };

    for (const char* root : roots)
    {
        chords.push_back(
            {
                root,
                "MAJOR",
                { 0, 4, 7 }
            }
        );

        chords.push_back(
            {
                root,
                "MINOR",
                { 0, 3, 7 }
            }
        );

        chords.push_back(
            {
                root,
                "MINOR7",
                { 0, 3, 7, 10 }
            }
        );

        chords.push_back(
            {
                root,
                "MAJOR7",
                { 0, 4, 7, 11 }
            }
        );
    }

    return chords;
}

// ============================================================
// NOTE FROM SEMITONE
// ============================================================

std::string NoteFromSemitone(
    int value)
{
    const char* names[] =
    {
        "C",
        "C#",
        "D",
        "D#",
        "E",
        "F",
        "F#",
        "G",
        "G#",
        "A",
        "A#",
        "B"
    };

    value %= 12;

    if (value < 0)
        value += 12;

    return names[value];
}

// ============================================================
// ROOT TO SEMITONE
// ============================================================

int RootSemitone(
    const std::string& root)
{
    if (root == "C")  return 0;
    if (root == "C#") return 1;
    if (root == "D")  return 2;
    if (root == "D#") return 3;
    if (root == "E")  return 4;
    if (root == "F")  return 5;
    if (root == "F#") return 6;
    if (root == "G")  return 7;
    if (root == "G#") return 8;
    if (root == "A")  return 9;
    if (root == "A#") return 10;
    if (root == "B")  return 11;

    return 0;
}

// ============================================================
// SHOW INSTRUMENTS
// ============================================================

void ShowInstruments()
{
    std::string message;

    message +=
        "INSTRUMENTS\r\n"
        "============\r\n\r\n";

    std::vector<std::string> instruments =
        GetInstruments();

    for (const std::string& instrument : instruments)
    {
        message +=
            instrument +
            "\r\n";
    }

    message +=
        "\r\nNOTES\r\n"
        "======\r\n\r\n";

    std::vector<std::string> notes =
        GetNotes();

    for (const std::string& note : notes)
    {
        message +=
            note +
            "\r\n";
    }

    // --------------------------------------------------------
    // SHOW THE LIST
    // --------------------------------------------------------

    MessageBoxA(
        mainWindow,
        message.c_str(),
        "Instruments & Notes",
        MB_OK | MB_ICONINFORMATION
    );
}
// ============================================================
// INSERT NOTE / INSTRUMENT
// ============================================================

void ShowInstrumentMenu()
{
    HMENU menu =
        CreatePopupMenu();

    std::vector<std::string>
        instruments =
            GetInstruments();

    std::vector<std::string>
        notes =
            GetNotes();

    int id = 5000;

    for (const std::string& instrument :
         instruments)
    {
        AppendMenuA(
            menu,
            MF_STRING,
            id++,
            instrument.c_str()
        );
    }

    AppendMenuA(
        menu,
        MF_SEPARATOR,
        0,
        nullptr
    );

    for (const std::string& note :
         notes)
    {
        AppendMenuA(
            menu,
            MF_STRING,
            id++,
            note.c_str()
        );
    }

    POINT point;

    GetCursorPos(
        &point
    );

    int result =
        TrackPopupMenu(
            menu,
            TPM_RETURNCMD |
            TPM_LEFTALIGN |
            TPM_TOPALIGN,
            point.x,
            point.y,
            0,
            mainWindow,
            nullptr
        );

    DestroyMenu(menu);

    if (result < 5000)
        return;

    int index =
        result - 5000;

    if (
        index >= 0 &&
        index <
        static_cast<int>(
            instruments.size()
        ))
    {
        AddTextToEditor(
            instruments[index] +
            " C4 0 1"
        );

        return;
    }

    index -=
        static_cast<int>(
            instruments.size()
        );

    if (
        index >= 0 &&
        index <
        static_cast<int>(
            notes.size()
        ))
    {
        AddTextToEditor(
            "NOTE " +
            notes[index] +
            " 0 1"
        );
    }
}

// ============================================================
// CHORD MENU
// ============================================================

void ShowChordMenu()
{
    HMENU menu =
        CreatePopupMenu();

    std::vector<ChordDefinition>
        chords =
            GetChords();

    int id = 6000;

    for (
        const ChordDefinition& chord :
        chords)
    {
        std::string name =
            chord.root +
            " " +
            chord.type;

        AppendMenuA(
            menu,
            MF_STRING,
            id++,
            name.c_str()
        );
    }

    POINT point;

    GetCursorPos(
        &point
    );

    int result =
        TrackPopupMenu(
            menu,
            TPM_RETURNCMD |
            TPM_LEFTALIGN |
            TPM_TOPALIGN,
            point.x,
            point.y,
            0,
            mainWindow,
            nullptr
        );

    DestroyMenu(menu);

    if (result < 6000)
        return;

    int index =
        result - 6000;

    if (
        index < 0 ||
        index >=
        static_cast<int>(
            chords.size()
        ))
        return;

    const ChordDefinition&
        chord =
            chords[index];

    int root =
        RootSemitone(
            chord.root
        );

    std::string text =
        "# " +
        chord.root +
        " " +
        chord.type +
        "\r\n";

    int beat = 0;

    for (int interval :
         chord.intervals)
    {
        int semitone =
            root + interval;

        int octave =
            4 +
            semitone / 12;

        std::string note =
            NoteFromSemitone(
                semitone
            ) +
            std::to_string(
                octave
            );

        text +=
            "PIANO " +
            note +
            " " +
            std::to_string(beat) +
            " 1\r\n";
    }

    AddTextToEditor(
        text
    );
}

// ============================================================
// SAVE TRACK
// ============================================================

void SaveTrack()
{
    std::string text =
        GetEditorText();

    if (text.empty())
    {
        MessageBoxA(
            mainWindow,
            "The editor is empty.",
            "SAVE TRACK",
            MB_OK |
            MB_ICONINFORMATION
        );

        return;
    }

    savedTracks.push_back(
        text
    );

    // Clear editor after saving.
    SetWindowTextA(
        songEditor,
        ""
    );

    // --------------------------------------------------------
    // CREATE PLAY BUTTON
    // --------------------------------------------------------

    int index =
        static_cast<int>(
            savedTracks.size()
        ) - 1;

    HWND track =
        CreateWindowA(
            "BUTTON",
            ("TRACK " +
             std::to_string(index + 1))
                .c_str(),
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            150,
            TRACK_BUTTON_HEIGHT,
            mainWindow,
            (HMENU)(INT_PTR)
                (ID_TRACK_BASE + index),
            GetModuleHandleA(nullptr),
            nullptr
        );

    // --------------------------------------------------------
    // CREATE LOAD BUTTON
    // --------------------------------------------------------

    HWND load =
        CreateWindowA(
            "BUTTON",
            "LOAD",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            80,
            TRACK_BUTTON_HEIGHT,
            mainWindow,
            (HMENU)(INT_PTR)
                (ID_LOAD_BASE + index),
            GetModuleHandleA(nullptr),
            nullptr
        );

    trackButtons.push_back(
        track
    );

    loadButtons.push_back(
        load
    );

    ResizeControls();

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// LOAD SAVED TRACK
// ============================================================

void LoadTrack(
    int index)
{
    if (
        index < 0 ||
        index >=
        static_cast<int>(
            savedTracks.size()
        ))
        return;

    SetWindowTextA(
        songEditor,
        savedTracks[index].c_str()
    );
}

// ============================================================
// PLAY SAVED TRACK
// ============================================================

void PlaySavedTrack(
    int index)
{
    if (
        index < 0 ||
        index >=
        static_cast<int>(
            savedTracks.size()
        ))
        return;

    if (playing)
    {
        stopRequested = true;
        return;
    }

    std::string text =
        savedTracks[index];

    std::thread(
        PlaySong,
        text
    ).detach();
}

// ============================================================
// DRAW PANELS
// ============================================================

void DrawPanels(
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

    DeleteObject(
        background
    );

    // --------------------------------------------------------
    // EDITOR PANEL
    // --------------------------------------------------------

    RECT editorPanel =
    {
        LEFT_MARGIN - 5,
        EDITOR_TOP - 8 - scrollY,
        rect.right - LEFT_MARGIN + 5,
        EDITOR_TOP +
        EDITOR_HEIGHT +
        8 -
        scrollY
    };

    HBRUSH panelBrush =
        CreateSolidBrush(
            PANEL
        );

    FillRect(
        dc,
        &editorPanel,
        panelBrush
    );

    DeleteObject(
        panelBrush
    );

    // --------------------------------------------------------
    // TRACK PANEL
    // --------------------------------------------------------

    RECT trackPanel =
    {
        LEFT_MARGIN - 5,
        TRACK_TOP - 8 - scrollY,
        rect.right - LEFT_MARGIN + 5,
        contentHeight - 10 - scrollY
    };

    HBRUSH trackBrush =
        CreateSolidBrush(
            PANEL
        );

    FillRect(
        dc,
        &trackPanel,
        trackBrush
    );

    DeleteObject(
        trackBrush
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
                reinterpret_cast<HDC>(
                    wParam
                );

            SetTextColor(
                dc,
                BUTTON_TEXT
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            return reinterpret_cast<LRESULT>(
                GetStockObject(
                    NULL_BRUSH
                )
            );
        }

        // ====================================================
        // EDIT COLOR
        // ====================================================

        case WM_CTLCOLOREDIT:
        {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                );

            SetTextColor(
                dc,
                EDIT_TEXT
            );

            SetBkColor(
                dc,
                EDIT_BACKGROUND
            );

            return reinterpret_cast<LRESULT>(
                editBrush
            );
        }

        // ====================================================
        // STATIC COLOR
        // ====================================================

        case WM_CTLCOLORSTATIC:
        {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                );

            SetTextColor(
                dc,
                TEXT_COLOR
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            return reinterpret_cast<LRESULT>(
                GetStockObject(
                    NULL_BRUSH
                )
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
            // PLAY / STOP
            // ------------------------------------------------

            if (id == ID_PLAY)
            {
                if (!playing)
                {
                    std::string text =
                        GetEditorText();

                    if (text.empty())
                        return 0;

                    stopRequested = false;

                    SetWindowTextA(
                        playButton,
                        "STOP"
                    );

                    std::thread(
                        PlaySong,
                        text
                    ).detach();
                }
                else
                {
                    stopRequested = true;

                    SetWindowTextA(
                        playButton,
                        "PLAY"
                    );
                }

                return 0;
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

                return 0;
            }

            // ------------------------------------------------
            // BOOST
            // ------------------------------------------------

            if (id == ID_BOOST)
            {
                int level =
                    volumeBoost.load();

                ++level;

                if (level > 3)
                    level = 0;

                volumeBoost =
                    level;

                UpdateBoostDisplay();

                return 0;
            }

            // ------------------------------------------------
            // TEMPO -
            // ------------------------------------------------

            if (id == ID_TEMPO_MINUS)
            {
                double tempo =
                    currentTempo.load();

                tempo -= TEMPO_STEP;

                if (tempo < MIN_TEMPO)
                    tempo = MIN_TEMPO;

                currentTempo =
                    tempo;

                UpdateTempoDisplay();

                return 0;
            }

            // ------------------------------------------------
            // TEMPO +
            // ------------------------------------------------

            if (id == ID_TEMPO_PLUS)
            {
                double tempo =
                    currentTempo.load();

                tempo += TEMPO_STEP;

                if (tempo > MAX_TEMPO)
                    tempo = MAX_TEMPO;

                currentTempo =
                    tempo;

                UpdateTempoDisplay();

                return 0;
            }

            // ------------------------------------------------
            // PITCH -
            // ------------------------------------------------

            if (id == ID_PITCH_MINUS)
            {
                int pitch =
                    currentPitch.load();

                pitch -= PITCH_STEP;

                if (pitch < MIN_PITCH)
                    pitch = MIN_PITCH;

                currentPitch =
                    pitch;

                UpdatePitchDisplay();

                return 0;
            }

            // ------------------------------------------------
            // PITCH +
            // ------------------------------------------------

            if (id == ID_PITCH_PLUS)
            {
                int pitch =
                    currentPitch.load();

                pitch += PITCH_STEP;

                if (pitch > MAX_PITCH)
                    pitch = MAX_PITCH;

                currentPitch =
                    pitch;

                UpdatePitchDisplay();

                return 0;
            }

            // ------------------------------------------------
            // SAVE
            // ------------------------------------------------

            if (id == ID_SAVE)
            {
                SaveTrack();

                return 0;
            }

            // ------------------------------------------------
            // INSTRUMENTS
            // ------------------------------------------------

            if (id == ID_INSTRUMENTS)
            {
                ShowInstrumentMenu();

                return 0;
            }

            // ------------------------------------------------
            // CHORDS
            // ------------------------------------------------

            if (id == ID_CHORDS)
            {
                ShowChordMenu();

                return 0;
            }

            // ------------------------------------------------
            // TRACK PLAY BUTTONS
            // ------------------------------------------------

            if (
                id >= ID_TRACK_BASE &&
                id <
                ID_TRACK_BASE +
                static_cast<int>(
                    savedTracks.size()
                ))
            {
                int index =
                    id -
                    ID_TRACK_BASE;

                PlaySavedTrack(
                    index
                );

                return 0;
            }

            // ------------------------------------------------
            // LOAD BUTTONS
            // ------------------------------------------------

            if (
                id >= ID_LOAD_BASE &&
                id <
                ID_LOAD_BASE +
                static_cast<int>(
                    savedTracks.size()
                ))
            {
                int index =
                    id -
                    ID_LOAD_BASE;

                LoadTrack(
                    index
                );

                return 0;
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

            int newPos =
                si.nPos;

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
                    newPos = 0;
                    break;

                case SB_BOTTOM:
                    newPos =
                        contentHeight;
                    break;
            }

            int maximum =
                contentHeight -
                static_cast<int>(
                    si.nPage
                );

            if (maximum < 0)
                maximum = 0;

            if (newPos < 0)
                newPos = 0;

            if (newPos > maximum)
                newPos = maximum;

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

            return 0;
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

            if (delta > 0)
            {
                scrollY -= 80;
            }
            else
            {
                scrollY += 80;
            }

            RECT rect;

            GetClientRect(
                window,
                &rect
            );

            // IMPORTANT:
            // This function returns void.
            // It updates contentHeight itself.
            CalculateContentHeight(
                rect
            );

            int maximum =
                contentHeight -
                rect.bottom;

            if (maximum < 0)
            {
                maximum = 0;
            }

            if (scrollY < 0)
            {
                scrollY = 0;
            }

            if (scrollY > maximum)
            {
                scrollY = maximum;
            }

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

            return 0;
        }

        // ====================================================
        // SIZE
        // ====================================================

        case WM_SIZE:
        {
            ResizeControls();

            InvalidateRect(
                window,
                nullptr,
                TRUE
            );

            return 0;
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

            DrawPanels(
                dc,
                rect
            );

            // ------------------------------------------------
            // TOP BAR
            // ------------------------------------------------

            RECT topBar =
            {
                0,
                0,
                rect.right,
                180
            };

            HBRUSH topBrush =
                CreateSolidBrush(
                    RGB(10, 10, 14)
                );

            FillRect(
                dc,
                &topBar,
                topBrush
            );

            DeleteObject(
                topBrush
            );

            EndPaint(
                window,
                &ps
            );

            return 0;
        }

        // ====================================================
        // DESTROY
        // ====================================================

        case WM_DESTROY:
        {
            stopRequested = true;

            if (editBrush)
            {
                DeleteObject(
                    editBrush
                );

                editBrush = nullptr;
            }

            PostQuitMessage(
                0
            );

            return 0;
        }
    }

    return DefWindowProcA(
        window,
        message,
        wParam,
        lParam
    );
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
        "CppSongMaker";

    // --------------------------------------------------------
    // EDIT BRUSH
    // --------------------------------------------------------

    editBrush =
        CreateSolidBrush(
            EDIT_BACKGROUND
        );

    // --------------------------------------------------------
    // WINDOW CLASS
    // --------------------------------------------------------

    WNDCLASSA wc = {};

    wc.lpfnWndProc =
        WindowProcedure;

    wc.hInstance =
        instance;

    wc.lpszClassName =
        CLASS_NAME;

    wc.hCursor =
        LoadCursorA(
            nullptr,
            IDC_ARROW
        );

    wc.hbrBackground =
        CreateSolidBrush(
            BACKGROUND
        );

    RegisterClassA(
        &wc
    );

    // --------------------------------------------------------
    // MAIN WINDOW
    // --------------------------------------------------------

    HWND window =
        CreateWindowExA(
            WS_EX_COMPOSITED,
            CLASS_NAME,
            "C++ Song Maker",
            WS_OVERLAPPEDWINDOW |
            WS_VSCROLL,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1000,
            800,
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
    // CREATE CONTROLS
    // ========================================================

    // --------------------------------------------------------
    // PLAY
    // --------------------------------------------------------

    playButton =
        CreateWindowA(
            "BUTTON",
            "PLAY",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            80,
            30,
            window,
            (HMENU)(INT_PTR)ID_PLAY,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // LOOP
    // --------------------------------------------------------

    loopButton =
        CreateWindowA(
            "BUTTON",
            "LOOP: OFF",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            90,
            30,
            window,
            (HMENU)(INT_PTR)ID_LOOP,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // BOOST
    // --------------------------------------------------------

    boostButton =
        CreateWindowA(
            "BUTTON",
            "BOOST: 1.0x",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            90,
            30,
            window,
            (HMENU)(INT_PTR)ID_BOOST,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // INSTRUMENTS
    // --------------------------------------------------------

    instrumentsButton =
        CreateWindowA(
            "BUTTON",
            "INSTRUMENTS",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            100,
            30,
            window,
            (HMENU)(INT_PTR)ID_INSTRUMENTS,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // CHORDS
    // --------------------------------------------------------

    chordsButton =
        CreateWindowA(
            "BUTTON",
            "CHORDS",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            85,
            30,
            window,
            (HMENU)(INT_PTR)ID_CHORDS,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // TEMPO -
    // --------------------------------------------------------

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
            28,
            window,
            (HMENU)(INT_PTR)ID_TEMPO_MINUS,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // TEMPO LABEL
    // --------------------------------------------------------

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
            28,
            window,
            nullptr,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // TEMPO +
    // --------------------------------------------------------

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
            28,
            window,
            (HMENU)(INT_PTR)ID_TEMPO_PLUS,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // PITCH -
    // --------------------------------------------------------

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
            28,
            window,
            (HMENU)(INT_PTR)ID_PITCH_MINUS,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // PITCH LABEL
    // --------------------------------------------------------

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
            28,
            window,
            nullptr,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // PITCH +
    // --------------------------------------------------------

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
            28,
            window,
            (HMENU)(INT_PTR)ID_PITCH_PLUS,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // SAVE
    // --------------------------------------------------------

    saveTrackButton =
        CreateWindowA(
            "BUTTON",
            "SAVE TRACK",
            WS_VISIBLE |
            WS_CHILD |
            BS_PUSHBUTTON,
            0,
            0,
            180,
            32,
            window,
            (HMENU)(INT_PTR)ID_SAVE,
            instance,
            nullptr
        );

    // --------------------------------------------------------
    // SONG EDITOR
    // --------------------------------------------------------

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
            500,
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

    // --------------------------------------------------------
    // INITIAL LAYOUT
    // --------------------------------------------------------

    ResizeControls();

    // --------------------------------------------------------
    // SHOW
    // --------------------------------------------------------

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

    MSG msg = {};

    while (
        GetMessageA(
            &msg,
            nullptr,
            0,
            0
        ))
    {
        TranslateMessage(
            &msg
        );

        DispatchMessageA(
            &msg
        );
    }

    return 0;
}
