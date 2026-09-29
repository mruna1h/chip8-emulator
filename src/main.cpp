#include "chip8.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <cstdint>
#include <iostream>

const int SCALE = 10; // Each pixel is 10x10 screen pixels
const int WIDTH = 64*SCALE;
const int HEIGHT = 32*SCALE;
const double FRAME_MS = 1000.0/60.0;

// Keyboard mapping
const SDL_Keycode keymap[16] = {
    SDLK_x, // 0
    SDLK_1, // 1
    SDLK_2, // 2
    SDLK_3, // 3
    SDLK_q, // 4
    SDLK_w, // 5
    SDLK_e, // 6
    SDLK_a, // 7
    SDLK_s, // 8
    SDLK_d, // 9
    SDLK_z, // A
    SDLK_c, // B
    SDLK_4, // C
    SDLK_r, // D
    SDLK_f, // E
    SDLK_v  // F
};

void audio_callback(void* userdata, uint8_t* stream, int len){
    static uint32_t sample_index = 0;
    int16_t* audio_buffer = (int16_t*) stream;
    int samples = len/2;

    bool* beeping = (bool*) userdata;
    for(int i=0; i<samples; i++){
        if(*beeping){
            // Generating 440Hz sqaure wave
            int16_t value = ((sample_index++ / 50) % 2) ? 3000 : -3000;
            // Earlier the wave was flipped at every 100 samples that give 200 samples full cycles. In main loop it wants 44100 samples per sec that gives 44100/200 = 220 Hz not 440. So for 440 it should filp after every 50 samples 
            audio_buffer[i] = value;
        }
        else{
            audio_buffer[i] = 0; // Silence
            sample_index = 0;
        }
    }
}

void draw_graphics(SDL_Renderer* renderer, Chip8& chip8, int color_theme){
    // Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    // Drawing white pixels
    // Setting color dynamically  (0 = white, 1 = amber, 2 = green)
    if(color_theme == 0) SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    else if(color_theme == 1) SDL_SetRenderDrawColor(renderer, 255, 176, 0, 255);
    else if(color_theme == 2) SDL_SetRenderDrawColor(renderer, 51, 255, 0, 255);
    else SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    
    for(int y=0; y<32; y++){
        for(int x=0; x<64; x++){
            if(chip8.display[x + (y*64)] == 1){
                // Here the white pixel allocation starts from above but we need is bottom to up
                SDL_Rect rect = {x*SCALE, y*SCALE, SCALE, SCALE};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

void handle_input(Chip8& chip8, bool& running, int& speed, int& color_theme){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        if(event.type == SDL_QUIT) running = false;
        if(event.type == SDL_KEYDOWN){
            if(event.key.keysym.sym == SDLK_ESCAPE) running = false;
            //Added for save states
            if(event.key.keysym.sym == SDLK_F5) chip8.save_state("save.ch8");
            if(event.key.keysym.sym == SDLK_F9) chip8.load_state("save.ch8");
            
            // Press UP arrow to speed up, DOWN arrow to slow down
            if(event.key.keysym.sym == SDLK_UP) speed += 2;
            if(event.key.keysym.sym == SDLK_DOWN && speed > 2) speed -= 2;
            
            //Color updation
            if(event.key.keysym.sym == SDLK_t) {
                color_theme = (color_theme + 1) % 4; 
                chip8.draw_flag = true;
            }
            
            // Check which Chip-8 key was pressed
            for(int i=0; i<16; i++){
                if(event.key.keysym.sym == keymap[i]) chip8.key[i] = 1;
            }
        }
        if(event.type == SDL_KEYUP){
            for(int i=0; i<16; i++){
                if(event.key.keysym.sym == keymap[i]) chip8.key[i] = 0;
            }
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        std::cerr << "Usage: " << argv[0] << " <ROM file>" << std::endl;
        return 1;
    }
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0){
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    // Audio setup
    bool beeping = false;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;
    want.userdata = &beeping;

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if(audio_device == 0) std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
    else SDL_PauseAudioDevice(audio_device, 0);

    SDL_Window* window = SDL_CreateWindow("Chip-8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if(!window){
        std::cerr << "Window error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(!renderer){
        std::cerr << "Renderer error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Chip8 chip8;
    chip8.load_rom(argv[1]);
    const double freq = (double)SDL_GetPerformanceFrequency();
    int speed = 10;
    int color_theme = 0;
    bool running = true;
    
    while(running){
        handle_input(chip8, running, speed, color_theme);
        Uint64 start = SDL_GetPerformanceCounter();
        for(int i=0; i<speed; i++){ // Here in for loop after every steps it's taking some time or delay's which make the game slower. The delay is about per CPU step  
            chip8.emulate_cycle();
            //SDL_Delay(16);-> This was reson the game was slow // 60 FPS with 16ms per frame
        }
        chip8.tick_timers();
        beeping = (chip8.get_sound_timer() > 0);
        if(chip8.draw_flag)
        {
            draw_graphics(renderer, chip8, color_theme);
            chip8.draw_flag = false;
        }
        double elapsed_ms = (SDL_GetPerformanceCounter() - start)*1000.0/freq;
        if(elapsed_ms<FRAME_MS)
        {
            SDL_Delay((Uint32)(FRAME_MS-elapsed_ms));
        }
    }
    if(audio_device != 0) SDL_CloseAudioDevice(audio_device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
