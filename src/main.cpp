#include <iostream>
#include <vector>
#include <span>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <raylib.h>

#include "core.h"

constexpr double TARGET_FPS = 59.94;
constexpr double FRAME_TIME = 1.0 / TARGET_FPS;

// ============================================================================
// Thread-Safe Audio Ring Buffer
// ============================================================================
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacitySamples = 16384)
        : data(capacitySamples, 0), capacity(capacitySamples) {}

    void push(std::span<const int16_t> samples) {
        std::lock_guard<std::mutex> lock(mtx);
        for (int16_t sample : samples) {
            size_t next = (head + 1) % capacity;
            if (next == tail) break; // Drop if full
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
    g_audioBuffer.pop(static_cast<int16_t*>(bufferData), frames * 2);
}

// ============================================================================
// Input Mapping
// ============================================================================
struct ButtonMapping {
    KeyboardKey key;
    GamepadButton gamepadBtn;
    Button coreBtn;
};

// Full mapping matching your Core's enum
constexpr ButtonMapping MAPPINGS[] = {
    { KEY_UP,        GAMEPAD_BUTTON_LEFT_FACE_UP,       Button::Up },
    { KEY_DOWN,      GAMEPAD_BUTTON_LEFT_FACE_DOWN,     Button::Down },
    { KEY_LEFT,      GAMEPAD_BUTTON_LEFT_FACE_LEFT,     Button::Left },
    { KEY_RIGHT,     GAMEPAD_BUTTON_LEFT_FACE_RIGHT,    Button::Right },

    { KEY_Z,         GAMEPAD_BUTTON_RIGHT_FACE_DOWN,    Button::Cross },
    { KEY_X,         GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,   Button::Circle },
    { KEY_A,         GAMEPAD_BUTTON_RIGHT_FACE_LEFT,    Button::Square },
    { KEY_S,         GAMEPAD_BUTTON_RIGHT_FACE_UP,      Button::Triangle },

    { KEY_ENTER,     GAMEPAD_BUTTON_MIDDLE_RIGHT,       Button::Start },
    { KEY_SPACE,     GAMEPAD_BUTTON_MIDDLE_LEFT,        Button::Select },

    { KEY_Q,         GAMEPAD_BUTTON_LEFT_TRIGGER_1,     Button::L1 },
    { KEY_W,         GAMEPAD_BUTTON_RIGHT_TRIGGER_1,    Button::R1 },
    { KEY_E,         GAMEPAD_BUTTON_LEFT_TRIGGER_2,     Button::L2 },
    { KEY_R,         GAMEPAD_BUTTON_RIGHT_TRIGGER_2,    Button::R2 },
};

void updateInput(Core& core) {
    constexpr int PORT = 0;
    bool gamepadAvailable = IsGamepadAvailable(0);

    for (const auto& map : MAPPINGS) {
        bool pressed = IsKeyDown(map.key);
        if (gamepadAvailable && IsGamepadButtonDown(0, map.gamepadBtn)) {
            pressed = true;
        }
        core.setButton(PORT, map.coreBtn, pressed);
    }

    if (gamepadAvailable) {
        core.setButton(PORT, Button::L3, IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_THUMB));
        core.setButton(PORT, Button::R3, IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_THUMB));

        core.setAxis(PORT, Axis::LeftX,  GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X));
        core.setAxis(PORT, Axis::LeftY,  GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y));
        core.setAxis(PORT, Axis::RightX, GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X));
        core.setAxis(PORT, Axis::RightY, GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y));
    }
}

// ============================================================================
// Main Application
// ============================================================================
int main() {
    Core core;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(640, 480, "ps1emu");
    SetWindowMinSize(320, 240);

    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(2048);
    AudioStream audioStream = LoadAudioStream(44100, 16, 2);
    SetAudioStreamCallback(audioStream, audioCallback);
    PlayAudioStream(audioStream);

    // 1024x512 16-bit texture for PS1 VRAM
    Image blank = GenImageColor(1024, 512, BLANK);
    ImageFormat(&blank, PIXELFORMAT_UNCOMPRESSED_R5G5B5A1);
    Texture2D fbTexture = LoadTextureFromImage(blank);
    UnloadImage(blank);
    SetTextureFilter(fbTexture, TEXTURE_FILTER_POINT);

    double accumulator = 0.0;

    while (!WindowShouldClose()) {
        updateInput(core);

        double delta = GetFrameTime();
        if (delta > 0.25) delta = 0.25;
        accumulator += delta;

        bool newFrame = false;
        while (accumulator >= FRAME_TIME) {
            core.stepFrame();
            accumulator -= FRAME_TIME;
            newFrame = true;

            g_audioBuffer.push(core.getAudioSamples());
            core.clearAudioSamples();
        }

        if (newFrame) {
            UpdateTexture(fbTexture, core.getFramebuffer());
        }

        // Display Region & Aspect Ratio calculation
        DisplayRegion disp = core.getDisplayRegion();
        
        // Guard against uninitialized GPU registers on startup
        if (disp.width <= 0 || disp.height <= 0) {
            disp.x = 0;
            disp.y = 0;
            disp.width = 320;
            disp.height = 240;
        }

        Rectangle sourceRec = {
            static_cast<float>(disp.x),
            static_cast<float>(disp.y),
            static_cast<float>(disp.width),
            static_cast<float>(disp.height)
        };

        // Standard 4:3 letterboxing
        float screenW = static_cast<float>(GetScreenWidth());
        float screenH = static_cast<float>(GetScreenHeight());
        constexpr float TARGET_ASPECT = 4.0f / 3.0f;
        
        float targetW, targetH;
        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if ((screenW / screenH) > TARGET_ASPECT) {
            targetH = screenH;
            targetW = screenH * TARGET_ASPECT;
            offsetX = (screenW - targetW) * 0.5f;
        } else {
            targetW = screenW;
            targetH = screenW / TARGET_ASPECT;
            offsetY = (screenH - targetH) * 0.5f;
        }

        Rectangle destRec = { offsetX, offsetY, targetW, targetH };

        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(fbTexture, sourceRec, destRec, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);

        EndDrawing();
    }

    UnloadTexture(fbTexture);
    UnloadAudioStream(audioStream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}