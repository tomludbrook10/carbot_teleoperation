#include <SDL2/SDL.h>
#include <iostream>
#include <chrono>

// Axis 5 is right trigger
// Axis 4 is left trigger

int main() {
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    SDL_GameController *ctrl = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            ctrl = SDL_GameControllerOpen(i);
            std::cout << "Opened: " << SDL_GameControllerName(ctrl) << "\n";
            break;
        }
    }

    SDL_Event e;
    using clock = std::chrono::high_resolution_clock;
    auto last = clock::now();


    while (true) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERAXISMOTION) {
                int axis = (int)e.caxis.axis;
                if (axis == 4 || axis == 5 || axis == 0) {
                    int value = e.caxis.value;
                    if (abs(value) < 2000) value = 0; // deadzone
                    float normalized = value / 32767.0f; // normalize to -1 to 1
                    std::cout << "Axis " << axis << " = " << normalized << "\n";
                    auto now = clock::now();
                    std::chrono::duration<float> elapsed = now - last;
                    std::cout << "Elapsed time: " << elapsed.count() << " seconds\n";
                    last = now;
                } 
            }
            if (e.type == SDL_CONTROLLERBUTTONDOWN)
                std::cout << "Button " << (int)e.cbutton.button << " pressed\n";
        }
        SDL_Delay(10);
    }
    return 0;
}