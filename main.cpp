// main.cpp
// A simple SDL2 program demonstrating a button that changes the background color
// and plays a sound when clicked. Includes hover and pressed states with comments
// explaining each step for learning purposes.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdio>
#include <random>
#include <vector>
#include <cmath>

// Represents a UI button and its states
struct Button {
    SDL_Rect rect{};       // Position and size of the button
    bool hovered{false};   // True if mouse is currently over the button
    bool pressed{false};   // True if visually pressed (mouse held down inside)
    bool activePress{false}; // True if the click began inside the button
};

// Draw the button with visual states (idle, hover, pressed)
static void render_button(SDL_Renderer* r, const Button& b, TTF_Font* font, const char* label) {
    // Background fill color depends on state
    if (b.pressed) {
        SDL_SetRenderDrawColor(r, 30, 30, 30, 255);   // pressed: darkest
    } else if (b.hovered) {
        SDL_SetRenderDrawColor(r, 70, 70, 70, 255);   // hover: medium gray
    } else {
        SDL_SetRenderDrawColor(r, 40, 40, 40, 255);   // idle: default gray
    }
    SDL_RenderFillRect(r, &b.rect);

    // Border color is lighter when hovered/pressed
    if (b.pressed) SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    else if (b.hovered) SDL_SetRenderDrawColor(r, 215, 215, 215, 255);
    else SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &b.rect);

    // Render the button label text centered inside
    SDL_Color labelCol{255,255,255,255};
    SDL_Surface* surf = TTF_RenderText_Blended(font, label, labelCol);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;

    int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    SDL_Rect dst{ b.rect.x + (b.rect.w - tw)/2, b.rect.y + (b.rect.h - th)/2, tw, th };
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// Utility: check if point (x,y) is inside a rect
static bool point_in_rect(int x, int y, const SDL_Rect& r) {
    return (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
}

int main(int, char**) {
    // Initialize SDL video and audio subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    // Initialize SDL_ttf for font rendering
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow("SDL2 Button",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 900, 600, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit(); return 1;
    }

    // Create renderer (accelerated with vsync)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit(); return 1;
    }

    // Load font (path may need adjusting per system)
    TTF_Font* font = TTF_OpenFont("./assets/fonts/MotivaSansBold.woff.ttf", 28);
    if (!font) {
        std::fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
        TTF_Quit(); SDL_Quit(); return 1;
    }

    // Setup audio: 48kHz, stereo, float format
    SDL_AudioSpec want{}, have{};
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev == 0) {
        std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(dev, 0); // start audio playback
    }

    // Lambda to generate and queue a short sine-wave beep
    auto play_beep = [&](float freq = 880.0f, float sec = 0.12f) {
        if (dev == 0) return; // no audio available
        const int frames = static_cast<int>(sec * static_cast<float>(have.freq));
        const std::size_t bufsize = static_cast<std::size_t>(frames) * static_cast<std::size_t>(have.channels);
        std::vector<float> buf(bufsize);
        float phase = 0.0f;
        const float inc = 2.0f * static_cast<float>(M_PI) * freq / static_cast<float>(have.freq);
        for (int i=0;i<frames;i++) {
            float s = std::sin(phase) * 0.25f; // sine wave amplitude scaled down
            phase += inc;
            for (int c=0;c<have.channels;c++)
                buf[static_cast<std::size_t>(i*have.channels + c)] = s;
        }
        SDL_QueueAudio(dev, buf.data(), static_cast<Uint32>(buf.size()*sizeof(float)));
    };

    // Random number generator for background colors
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(40, 220);

    // Initial background color (dark gray)
    Uint8 bgR = 20, bgG = 24, bgB = 28;

    // Button setup
    Button button;
    auto layout = [&](){
        // Center button in window
        int ww, wh; SDL_GetWindowSize(window, &ww, &wh);
        int bw = 200, bh = 60;
        button.rect = { (ww - bw)/2, (wh - bh)/2, bw, bh };
    };
    layout();

    // Main loop variables
    bool running = true;
    bool mouseDown = false;
    while (running) {
        SDL_Event e;
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        // Update hover state every frame
        button.hovered = point_in_rect(mouseX, mouseY, button.rect);

        // Update visual pressed state
        button.pressed = (button.activePress && mouseDown && button.hovered);

        // Process events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) layout();
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                mouseDown = true;
                // Only start click if mouse down inside button
                button.activePress = point_in_rect(e.button.x, e.button.y, button.rect);
                button.pressed = (button.activePress && button.hovered);
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                // Confirm click: must begin inside and release still inside
                bool releaseOver = point_in_rect(e.button.x, e.button.y, button.rect);
                if (button.activePress && releaseOver) {
                    // Change background to random color + play beep
                    bgR = static_cast<Uint8>(dist(rng));
                    bgG = static_cast<Uint8>(dist(rng));
                    bgB = static_cast<Uint8>(dist(rng));
                    play_beep();
                }
                // Reset press state regardless
                mouseDown = false;
                button.activePress = false;
                button.pressed = false;
            }
            else if (e.type == SDL_MOUSEMOTION) {
                // Update hover/pressed states when moving
                button.hovered = point_in_rect(e.motion.x, e.motion.y, button.rect);
                button.pressed = (button.activePress && mouseDown && button.hovered);
            }
        }

        // Draw background
        SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, 255);
        SDL_RenderClear(renderer);

        // Draw button
        render_button(renderer, button, font, "Click me!");

        // Present frame
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    if (dev) SDL_CloseAudioDevice(dev);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
