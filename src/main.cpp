#include <iostream>
#include <vector>
#include <span>
#include <mutex>
#include <cstring>
#include <raylib.h>

#include <core.h>

constexpr double TARGET_FPS = 59.94;
constexpr double FRAME_TIME = 1.0 / TARGET_FPS;

// ============================================================================
// Thread-Safe Audio Ring Buffer for Raylib's Audio Stream
// ============================================================================
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacitySamples = 16384)
        : data(capacitySamples, 0), capacity(capacitySamples) {}

    void push(std::span<const int16_t> samples) {
        std::lock_guard<std::mutex> lock(mtx);
        for (int16_t sample : samples) {
            size_t next = (head + 1) % capacity;
            if (next == tail) break; // Buffer full: drop samples to prevent desync
            data[head] = sample;
            head = next;
        }
    }

    void pop(int16_t* out, size_t sampleCount) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t samplesRead = 0;
        while (samplesRead < sampleCount && tail != head) {
            out[samplesRead++] = data[tail];
            tail = (tail + 1) % capacity;
        }
        // Pad underflows with silence
        if (samplesRead < sampleCount) {
            std::memset(out + samplesRead, 0, (sampleCount - samplesRead) * sizeof(int16_t));
        }
    }

private:
    std::vector<int16_t> data;
    size_t capacity;
    size_t head = 0;
    size_t tail = 0;
    std::mutex mtx;
};

static AudioRingBuffer g_audioBuffer(16384);

void audioCallback(void* bufferData, unsigned int frames) {
    // 2 channels (stereo), 16-bit
    g_audioBuffer.pop(static_cast<int16_t*>(bufferData), frames * 2);
}

// ============================================================================
// Clean Input Mapping (Keyboard Physical Keys + Gamepad merged)
// ============================================================================
struct ButtonMapping {
    KeyboardKey key;
    GamepadButton gamepadBtn;
    Button coreBtn;
};

constexpr ButtonMapping MAPPINGS[] = {
    { KEY_UP,    GAMEPAD_BUTTON_LEFT_FACE_UP,    Button::Up },
    { KEY_DOWN,  GAMEPAD_BUTTON_LEFT_FACE_DOWN,  Button::Down },
    { KEY_LEFT,  GAMEPAD_BUTTON_LEFT_FACE_LEFT,  Button::Left },
    { KEY_RIGHT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT, Button::Right },

    { KEY_Z,     GAMEPAD_BUTTON_RIGHT_FACE_DOWN, Button::Cross },    // A / Cross
    { KEY_X,     GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,Button::Circle },   // B / Circle
    { KEY_A,     GAMEPAD_BUTTON_RIGHT_FACE_LEFT, Button::Square },   // X / Square
    { KEY_S,     GAMEPAD_BUTTON_RIGHT_FACE_UP,   Button::Triangle }, // Y / Triangle

    { KEY_ENTER, GAMEPAD_BUTTON_MIDDLE_RIGHT,    Button::Start },
    { KEY_SPACE, GAMEPAD_BUTTON_MIDDLE_LEFT,     Button::Select },
    { KEY_Q,     GAMEPAD_BUTTON_LEFT_TRIGGER_1,  Button::L1 },
    { KEY_W,     GAMEPAD_BUTTON_RIGHT_TRIGGER_1, Button::R1 },
};

void updateInput(Core& core) {
    bool gamepadAvailable = IsGamepadAvailable(0);

    for (const auto& map : MAPPINGS) {
        bool pressed = IsKeyDown(map.key);
        if (gamepadAvailable && IsGamepadButtonDown(0, map.gamepadBtn)) {
            pressed = true;
        }
        core.setButton(0, map.coreBtn, pressed);
    }

    if (gamepadAvailable) {
        core.setButton(0, Button::L3, IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_THUMB));
        core.setButton(0, Button::R3, IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_THUMB));

        core.setAxis(0, Axis::LeftX,  GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X));
        core.setAxis(0, Axis::LeftY,  GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y));
        core.setAxis(0, Axis::RightX, GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X));
        core.setAxis(0, Axis::RightY, GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y));
    }
}

// ============================================================================
// Main Application
// ============================================================================
int main() {
    Core core;

    // Window Setup
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(640, 480, "ps1emu");
    SetWindowMinSize(320, 240);

    // Audio Setup
    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(2048);
    AudioStream audioStream = LoadAudioStream(44100, 16, 2);
    SetAudioStreamCallback(audioStream, audioCallback);
    PlayAudioStream(audioStream);

    // Framebuffer Setup (PS1 16-bit 1555 VRAM: 1024x512)
    Image blank = GenImageColor(1024, 512, BLANK);
    ImageFormat(&blank, PIXELFORMAT_UNCOMPRESSED_R5G5B5A1);
    Texture2D fbTexture = LoadTextureFromImage(blank);
    UnloadImage(blank);
    SetTextureFilter(fbTexture, TEXTURE_FILTER_POINT);

    double accumulator = 0.0;

    while (!WindowShouldClose()) {
        updateInput(core);

        // Frame Timing & Emulation Loop
        double delta = GetFrameTime();
        if (delta > 0.25) delta = 0.25; // Clamp spiral of death
        accumulator += delta;

        bool newFrameEmulated = false;
        while (accumulator >= FRAME_TIME) {
            core.stepFrame();
            accumulator -= FRAME_TIME;
            newFrameEmulated = true;

            // Push audio samples generated by the core
            g_audioBuffer.push(core.getAudioSamples());
            core.clearAudioSamples();
        }

        // Only upload texture if a frame was actually stepped
        if (newFrameEmulated) {
            UpdateTexture(fbTexture, core.getFramebuffer());
        }

        // Render
        BeginDrawing();
        ClearBackground(BLACK);

        // Draw stretched to window (or adjust src rect to crop to PS1 active display)
        DrawTexturePro(
            fbTexture,
            Rectangle{ 0.0f, 0.0f, 1024.0f, 512.0f },
            Rectangle{ 0.0f, 0.0f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) },
            Vector2{ 0.0f, 0.0f },
            0.0f,
            WHITE
        );

        EndDrawing();
    }

    // Teardown
    UnloadTexture(fbTexture);
    UnloadAudioStream(audioStream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}