#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SAMPLE_RATE 44100

/* Used to define current state of Chip-8 object */
typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emu_state;

/* CHip-8 Object */
typedef struct {
    uint8_t ram[4096];      // CHIP-8 has access to up to 4kb of RAM 
    bool display [SCREEN_WIDTH * SCREEN_HEIGHT]; // Each pixel is either on (1) or off (0); 64x32 is the original CHIP-8 resolution 
    uint16_t stack[16];     // Used to store addresses of subroutines; Allows up to 16 levels of nested subroutines 
    int stack_size;         // Stack "pointer"
    uint8_t V[16];          // V0 - VF data registers; VF is a flag register 
    uint16_t I;             // I register; Commonly used for storing memory addresses 
    uint8_t delay_timer;    // Decremented by 60Hz (60 times/sec) until 0 
    uint8_t sound_timer;    // Decremented by 60Hz until 0; Plays CHIP-8 beeping sound if non-zero 
    uint16_t PC;            // Program counter 
    bool keypad[16];        // Each key can be pressed (1) or not (0). See handle_input() for more
    emu_state state;        // Can be RUNNING, PAUSED, or STOPPED      
    uint32_t volume;        // How loud emulation audio is; defaults to 1500; min 0, max 3000  
} chip8_t;

/* Initializes all necessary fields in CHIP-8 struct; Loads font into RAM */
bool initialize_chip8(chip8_t *chip8, const char rom_name[]) {
    const uint32_t entry = 0x200; // Standard starting point in memory for all CHIP-8 programs
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    // Set default fields for CHIP-8 object 
    chip8->state = RUNNING;         
    chip8->volume = 1500;
    chip8->PC = entry;                          
    memcpy(&chip8->ram[0], font, sizeof(font)); 

    // Open user-given ROM file 
    FILE *rom = fopen(rom_name, "rb");   
    if (rom == false) {
        printf("ROM file %s does not exist or is invalid\n", rom_name);
        return false;
    }

    // Find length of ROM file; confirm file can fit in memory
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof(chip8->ram) - entry;
    fseek(rom, 0, SEEK_SET);
    
    if (rom_size > max_size) {
        printf("ROM File %s is too large. File size: %zu, Max Size: %zu\n", rom_name, rom_size, max_size);
        return false;
    }

    // Read contents of ROM file into CHIP-8 RAM
    if (fread(&chip8->ram[entry], sizeof(uint8_t), rom_size, rom) != rom_size) { /* TODO: != 1? */
        printf("Failure ocurred when writing ROM file into CHIP-8 memory\n");
        return false;
    }
    fclose(rom);

    return true;
}

/* Fills audio stream buffer with data */
void audio_callback(void *userdata, uint8_t *audio_buf, int len) {
    
    int16_t *audio_buf16 = (uint16_t *)audio_buf;
    int32_t volume = *(int32_t *)userdata;
    static uint32_t running_sample_idx = 0;
    const int32_t square_wave_period = SAMPLE_RATE / 440 ;
    const int32_t half_square_wave_period = square_wave_period / 2;

    // Fill audio buffer
    // -----       Since we are dealing with square waves (see "drawing"), 
    //     |       if we are at a crest we simply add volume to the buffer,
    //     |       but if we are at a trough we will add -volume to buffer
    //     |       so that sound will still play
    //     -----
    for (int i = 0; i < len / 2; i++) { // len / 2 because audio_buf (uint8_t) was casted to uint16_t
        audio_buf16[i] = ((running_sample_idx++ / half_square_wave_period) % 2) ? volume : -volume;
    }

}

/* Initializes necessary SDL features */
bool initialize_SDL(SDL_Window **window, SDL_Renderer **renderer, 
    SDL_AudioSpec *want, SDL_AudioSpec *have, SDL_AudioDeviceID *dev, chip8_t *chip8) {

    // Initializes necessary SDL subsystems 
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0) {
        printf("SDL subsytsems failed to initialize. Error: %s\n", SDL_GetError());
        return false;
    }

    // Initializes SDL Window; width and height are scaled by 20
    *window = SDL_CreateWindow("CHIP-8 Emulator", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH * 20, SCREEN_HEIGHT * 20, 0);
    if (window == NULL) { 
        printf("SDL window failed to initialize. Error: %s\n", SDL_GetError());
        return false;
    }

    // Initializes SDL Renderer 
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("SDL renderer failed to initialize. Error: %s\n", SDL_GetError());
        return false;
    }

    // Initialize SDL Audio
    SDL_zero(*want);
    want->freq = SAMPLE_RATE;         // Standard CD quality; number of samples per second
    want->format = AUDIO_S16LSB;      // Signed 16-bit little endian samples
    want->channels = 1;               // Mono audio
    want->samples = 2;                // Size of the audio buffer in sample frames 
    want->callback = audio_callback;  // Pointer to audio callback function
    want->userdata = &chip8->volume;
    
    *dev = SDL_OpenAudioDevice(NULL, 0, want, have, 0);

    if (dev == 0) {
        printf("SDL audio failed to initialize. Error: %s\n", SDL_GetError());
        return false;
    }

    if (want->format != have->format || want->channels != have->channels) {
        printf("Could not get desired audio specifications.\n");
        return false;
    }

    return true;
}

/* Clears SDL window to background color */
void clear_screen(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 22, 9, 31, SDL_ALPHA_OPAQUE); // Hex: 0x16091F 
    SDL_RenderClear(renderer);
}

/* Handles any user and keypad input;
   Original CHIP-8 Keypad -> QWERTY
   1 2 3 C                   1 2 3 4
   4 5 6 D                   Q W E R
   7 8 9 E                   A S D F
   A 0 B F                   Z X C V */
void handle_input(chip8_t *chip8) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                chip8->state = QUIT;
                break;
            
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        break;
                    case SDLK_SPACE:
                        if (chip8->state == RUNNING) {   
                            chip8->state = PAUSED; // Pause 
                            printf("PAUSED\n");
                        } else {                        
                            chip8->state = RUNNING; // Unpause 
                            printf("RESUMED\n");
                        }
                        break;

                    case SDLK_MINUS:
                        // If the volume > 0, decrement by 100
                        if (chip8->volume > 0) {
                            chip8->volume -= 100;
                        } 
                        break;

                    case SDLK_EQUALS:
                        // If the volume < 3000, increment by 100
                        if (chip8->volume < 3000) {
                            chip8->volume += 100;
                        }
                        break;

                    // For CHIP-8 Keypad inputs, set corresponding index 
                    // of Chip-8 object's keypad field to true
                    // 1 2 3 C -> 1 2 3 4
                    case SDLK_1: chip8->keypad[0x1] = true; break;
                    case SDLK_2: chip8->keypad[0x2] = true; break;
                    case SDLK_3: chip8->keypad[0x3] = true; break;
                    case SDLK_4: chip8->keypad[0xC] = true; break;
                    // 4 5 6 D -> Q W E R
                    case SDLK_q: chip8->keypad[0x4] = true; break;
                    case SDLK_w: chip8->keypad[0x5] = true; break;
                    case SDLK_e: chip8->keypad[0x6] = true; break;
                    case SDLK_r: chip8->keypad[0xD] = true; break;
                    // 7 8 9 E -> A S D F
                    case SDLK_a: chip8->keypad[0x7] = true; break;
                    case SDLK_s: chip8->keypad[0x8] = true; break;
                    case SDLK_d: chip8->keypad[0x9] = true; break;
                    case SDLK_f: chip8->keypad[0xE] = true; break;
                    // A 0 B F -> Z X C V
                    case SDLK_z: chip8->keypad[0xA] = true; break;
                    case SDLK_x: chip8->keypad[0x0] = true; break;
                    case SDLK_c: chip8->keypad[0xB] = true; break;
                    case SDLK_v: chip8->keypad[0xF] = true; break;

                    default: break;
                }
                break;
            
            case SDL_KEYUP:
                switch(event.key.keysym.sym) {
                    // 1 2 3 C -> 1 2 3 4
                    case SDLK_1: chip8->keypad[0x1] = false; break;
                    case SDLK_2: chip8->keypad[0x2] = false; break;
                    case SDLK_3: chip8->keypad[0x3] = false; break;
                    case SDLK_4: chip8->keypad[0xC] = false; break;
                    // 4 5 6 D -> Q W E R
                    case SDLK_q: chip8->keypad[0x4] = false; break;
                    case SDLK_w: chip8->keypad[0x5] = false; break;
                    case SDLK_e: chip8->keypad[0x6] = false; break;
                    case SDLK_r: chip8->keypad[0xD] = false; break;
                    // 7 8 9 E -> A S D F
                    case SDLK_a: chip8->keypad[0x7] = false; break;
                    case SDLK_s: chip8->keypad[0x8] = false; break;
                    case SDLK_d: chip8->keypad[0x9] = false; break;
                    case SDLK_f: chip8->keypad[0xE] = false; break;
                    // A 0 B F -> Z X C V
                    case SDLK_z: chip8->keypad[0xA] = false; break;
                    case SDLK_x: chip8->keypad[0x0] = false; break;
                    case SDLK_c: chip8->keypad[0xB] = false; break;
                    case SDLK_v: chip8->keypad[0xF] = false; break;

                    default: break;
                }
                break;

            default:
                break;
        }
    }
}

/* Returns the next instruction contained in the ROM. Each instruction is 2 bytes. Increments PC by 2 */
uint16_t fetch_instruction(chip8_t *chip8) {
    uint16_t instr = ((chip8->ram[chip8->PC] << 8) | (chip8->ram[chip8->PC + 1]));
    chip8->PC += 2; // Increment PC for next instruction execution cycle 
    return instr;
}

#ifdef DEBUG
    void print_debugging(chip8_t *chip8, uint16_t opcode) {

        uint16_t NNN = opcode & 0x0FFF;     
        uint8_t NN = opcode & 0x00FF;       
        uint8_t N = opcode & 0x000F;        
        uint8_t X = (opcode >> 8) & 0x000F; 
        uint8_t Y = (opcode >> 4) & 0x000F; 

        switch (opcode >> 12) {
        case 0x0000: 
            switch (NN) {
                case 0xE0:      // 00E0: Clear the display 
                    printf("00E0: Clear the display\n");
                    break;

                case 0xEE:      // 00EE: Return from a subroutine
                    printf("00EE: Return from a subroutine to address 0x%04X. Stack size is now %d\n", 
                        chip8->stack[chip8->stack_size - 1], chip8->stack_size - 1);
                    break;

                default:
                    break;
            }
            break;

        case 0x0001:            // 1NNN: Jump to location NNN 
            printf("1NNN: Jump to location 0x%04X\n", NNN);
            break;

        case 0x0002:            // 2NNN: Call subroutine at NNN 
            printf("2NNN: Call subroutine at 0x%04X. Stack size is now %d\n", NNN, chip8->stack_size);
            break;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              

        case 0x0003:            // 3XNN: Skip next instruction if VX = NN 
            printf("3XNN: Skip next instruction if V%X (0x%02X) = NN (0x%02X)",
                X, chip8->V[X], NN);
            break;

        case 0x0004:            // 4XNN: Skip next instruction if VX != NN 
            printf("4XNN: Skip next instruction if V%X (0x%02X) != NN (0x%02X)",
                X, chip8->V[X], NN);
            break;

        case 0x0005:            // 5XY0: Skip next instruction if VX = VY 
            printf("5XY0: Skip next instruction if V%X (0x%02X) = V%X (0x%02X)\n",
                X, chip8->V[X], Y, chip8->V[Y]);
            break;

        case 0x0006:            // 6XNN: Set VX = NN 
            printf("6XNN: Set V%X = NN (0x%02X)\n", X, NN);
            break;

        case 0x0007:            // 7XNN: Set VX = VX + NN 
            printf("7XNN: Set V%X += NN (0x%02X). Result: 0x%02X\n", X, NN, 
                chip8->V[X] + NN);
            break;

        case 0x0008:
            switch(N) {
                case 0x0:       // 8XY0: Set VX = VY 
                    printf("8XY0: Set V%X = V%X\n", X, Y);
                    break;

                case 0x1:       // 8XY1: Set VX = VX OR VY 
                    printf("8XY1: Set V%X |= V%X. Result: 0x%02X\n", 
                        X, Y, chip8->V[X] | chip8->V[Y]);
                    break;

                case 0x2:       // 8XY2: Set VX = VX AND VY 
                    printf("8XY2: Set V%X &= V%X. Result: 0x%02X\n", 
                        X, Y, chip8->V[X] & chip8->V[Y]);
                    break;

                case 0x3:       // 8XY3: Set VX = VX XOR VY 
                     printf("8XY2: Set V%X ^= V%X. Result: 0x%02X\n", 
                        X, Y, chip8->V[X] ^ chip8->V[Y]);
                    break;

                case 0x4:       // 8XY4: Set VX += VY, set VF = carry
                    printf("8XY4: Set V%X (0x%02X) += V%X (0x%02X), set VF = carry. Results: 0x%02X, VF = %X\n",
                        X, chip8->V[X], Y, chip8->V[Y], chip8->V[X] + chip8->V[Y], 
                        ((uint16_t)(chip8->V[X] + chip8->V[Y]) > 255));
                    break;

                case 0x5:       // 8XY5: Set VX -= VY, set VF = NOT borrow
                    printf("8XY5: Set V%X (0x%02X) -= V%X (0x%02X), set VF = NOT borrow. Results: 0x%02X, VF = %X\n",
                        X, chip8->V[X], Y, chip8->V[Y], chip8->V[X] - chip8->V[Y], chip8->V[X] >= chip8->V[Y]);
                    break;

                case 0x6:       // 8XY6: Set VX = VX SHR 1. Set VF = 1 if MSB is 1 */
                    printf("8XY6: Set V%X >>= 1. Set VF = 1 if shifted bit is 1. Results: 0x%02X, VF = %X\n",
                        X, chip8->V[X] >> 1, chip8->V[X] & 1);
                    break;

                case 0x7:       // 8XY7: Set VX = VY - VX, set VF = NOT borrow 
                    printf("8XY7: Set V%X = V%X - V%X, set VF = NOT borrow. Results: 0x%02X, VF = %X\n",
                        X, Y, X, chip8->V[X] = chip8->V[Y], (chip8->V[Y] >= chip8->V[X]));
                    break;

                case 0xE:       // 8XYE: Set VX = VX SHL 1. Set VF = 1 if MSB is 1 
                    printf("8XYE: Set V%X <<= 1. Set VF = 1 if MSB is 1. Results: 0x%02X, VF = %X\n",
                        X, chip8->V[X] << 1, chip8->V[X] >> 7);
                    break;

                default:
                    break;
            }
            break;

        case 0x0009:            // 9XY0: Skip next instruction if VX != VY 
            printf("9XY0: Skip next instruction if V%X (0x%02X) != V%X (0x%02X)\n",
                X, chip8->V[X], Y, chip8->V[Y]);
            break;

        case 0x000A:            // ANNN: Set I = NNN 
            printf("ANNN: Set I = 0x%04X\n", NNN);
            break;
        
        case 0x000B:            // BNNN: Jump to location NNN + V0 
            printf("Jump to location 0x%04X + V0 (0x%02X). Result: 0x%04X\n",
                NNN, chip8->V[0], NNN + chip8->V[0]);
            break;

        case 0x000C:            // CXNN: Set VX = rand() % 256 & NN
            printf("CXNN: Set V%X = rand() %% 256 & NN (0x%02X)\n", X, NN);
            break;

        case 0x000D:            // DXYN: Display N-byte sprite starting at memory location I at (X, Y), set VF = collision 
            printf("DXYN: Draw N (%u) height sprite at X,Y (0x%02X, 0x%02X), starting at I (0x%04X). Set VF = collision.\n", 
                N, chip8->V[X] % SCREEN_WIDTH, chip8->V[Y] % SCREEN_HEIGHT, chip8->I);
            break;

        case 0x000E:
            switch (NN) {
                case 0x9E:      // EX9E: Skip next instruction if key stored in VX is pressed
                    printf("EX9E: Skip next instruction if key stored in V%X is pressed. Result: VX == 0x%02X\n",
                        X, chip8->V[X]);
                    break;

                case 0xA1:      // EXA1: Skip next instruction if key stored in VX is not pressed
                    printf("EXA1: Skip next instruction if key stored in V%X is not pressed. Result: VX == 0x%02X\n",
                        X, chip8->V[X]);
                    break;

                default:
                    break;
            }
            break;

        case 0x000F:
            switch (NN) {
                case 0x07:      // FX07: Sets VX to the delay timer
                    printf("FX07: Sets V%X to the delay timer (%u)\n", X, chip8->delay_timer);
                    break;

                case 0x0A:      // FX0A: Stop all execution until a key is pressed AND released. Store in VX
                    printf("FX0A: Stop all execution until a key is pressed AND released. Store in V%X\n", X);
                    break;

                case 0x15:      // FX15: Sets the delay timer to VX
                    printf("FX15: Sets the delay timer to V%X (0x%02X)\n", X, chip8->V[X]);
                    break;

                case 0x18:      // FX18: Sets the sound timer to VX
                    printf("FX18: Sets the sound timer to V%X (0x%02X)\n", X, chip8->V[X]);
                    break;

                case 0x1E:      // FX1E: Set I += VX
                    printf("FX1E: Set I (0x%04X) += V%X (0x%02X). Result: 0x%04X\n",
                        chip8->I, X, chip8->V[X], chip8->I + chip8->V[X]);
                    break;

                case 0x29:      // FX29: Set I = location of sprite for the character in VX
                    printf("FX29: Set I (0x%04X) = location of sprite for the character in V%X (0x%02X). Result: I = 0x%04X\n",
                        chip8->I, X, chip8->V[X], chip8->V[X] * 5);
                    break;

                case 0x33:      // FX33: Extracts hundreds, tens, and ones digits of an
                                // 8-bit number in VX to I, I + 1, I + 2
                    printf("FX33: Extracts hundreds, tens, and ones digits of an 8-bit number in V%X (0x%02X) to I, I + 1, I + 2\n",
                        X, chip8->V[X]);
                    break;
                    
                case 0x55:      // FX55: Store registers V0 through VX in memory starting at location I
                    printf("FX55: Store registers V0 through V%X in memory starting at location I (0x%04X)\n",
                        X, chip8->I);
                    break;
                    
                case 0x65:      // FX65: Read registers V0 through VX from memory starting at location I
                    printf("FX65: Read registers V0 through V%X from memory starting at location I (0x%04X)\n",
                        X, chip8->I);
                    break;
                    
                default:
                    break;
            }
            break;
        
        default:
            printf("Unimplemented opcode: 0x%04X\n", opcode);
        }
    }
#endif 

/* Emulates the execution of one opcode */
void execute_instruction(chip8_t *chip8) {
    uint16_t opcode = fetch_instruction(chip8); 

    #ifdef DEBUG
        print_debugging(chip8, opcode);
    #endif

    // Get (maybe) necessary parts of the opcode 
    uint16_t NNN = opcode & 0x0FFF;     // NNN is a 12-bit address
    uint8_t NN = opcode & 0x00FF;       // NN is a 8-bit constant
    uint8_t N = opcode & 0x000F;        // N is a 4-bit constant
    uint8_t X = (opcode >> 8) & 0x000F; // X is a 4-bit register identifier
    uint8_t Y = (opcode >> 4) & 0x000F; // Y is a 4-bit register identifier

    // Emulate opcode
    switch (opcode >> 12) {
        case 0x0000: 
            switch (NN) {
                case 0xE0:      // 00E0: Clear the display 
                    memset(&chip8->display[0], false, sizeof(chip8->display));
                    break;

                case 0xEE:      // 00EE: Return from a subroutine
                    chip8->PC = chip8->stack[chip8->stack_size - 1];
                    chip8->stack_size--;
                    break;

                default:
                    break;
            }
            break;
        
        case 0x0001:            // 1NNN: Jump to location NNN
            chip8->PC = NNN;
            break;

        case 0x0002:            // 2NNN: Call subroutine at NNN
            chip8->stack[chip8->stack_size++] = chip8->PC;
            chip8->PC = NNN;
            break;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              

        case 0x0003:            // 3XNN: Skip next instruction if VX = NN
            if (chip8->V[X] == NN) {
                chip8->PC += 2;
            }
            break;

        case 0x0004:            // 4XNN: Skip next instruction if VX != NN 
            if (chip8->V[X] != NN) {
                chip8->PC += 2;
            }
            break;

        case 0x0005:            // 5XY0: Skip next instruction if VX = VY 
            if (chip8->V[X] == chip8->V[Y]) {
                chip8->PC += 2;
            }
            break;

        case 0x0006:            // 6XNN: Set VX = NN 
            chip8->V[X] = NN;
            break;

        case 0x0007:            // 7XNN: Set VX = VX + NN 
            chip8->V[X] += NN;
            break;

        case 0x0008:
            switch(N) {
                case 0x0:       // 8XY0: Set VX = VY 
                    chip8->V[X] = chip8->V[Y];
                    break;

                case 0x1:       // 8XY1: Set VX = VX OR VY 
                    chip8->V[X] = (chip8->V[X] | chip8->V[Y]);
                    break;

                case 0x2:       // 8XY2: Set VX = VX AND VY 
                    chip8->V[X] = (chip8->V[X] & chip8->V[Y]);
                    break;

                case 0x3:       // 8XY3: Set VX = VX XOR VY 
                    chip8->V[X] = (chip8->V[X] ^ chip8->V[Y]);
                    break;

                case 0x4:       // 8XY4: Set VX = VX + VY, set VF = carry
                    { 
                    bool carry = ((uint16_t)(chip8->V[X] + chip8->V[Y]) > 255);
                    chip8->V[X] += chip8->V[Y];
                    chip8->V[0xF] = carry;
                    break;
                    }

                case 0x5:       // 8XY5: Set VX = VX - VY, set VF = NOT borrow
                    {
                    bool carry = (chip8->V[X] >= chip8->V[Y]);
                    chip8->V[X] -= chip8->V[Y];
                    chip8->V[0xF] = carry;
                    break;
                    }

                case 0x6:       // 8XY6: Set VX = VX SHR 1. Set VF = 1 if shifted bit is 1
                    {
                    bool shifted_bit = chip8->V[X] & 1;
                    chip8->V[X] >>= 1;
                    chip8->V[0xF] = shifted_bit;
                    break;
                    }

                case 0x7:       // 8XY7: Set VX = VY - VX, set VF = NOT borrow 
                    {
                    bool no_underflow = (chip8->V[Y] >= chip8->V[X]);
                    chip8->V[X] = chip8->V[Y] - chip8->V[X];
                    chip8->V[0xF] = no_underflow;
                    break;
                    }

                case 0xE:       // 8XYE: Set VX = VX SHL 1. Set VF = 1 if MSB is 1 
                    {
                    bool MSB = ((chip8->V[X] & 0x80) == 0x80);
                    chip8->V[X] <<= 1;
                    chip8->V[0xF] = MSB;
                    break;
                    }

                default:
                    break;
            }
            break;

        case 0x0009:            // 9XY0: Skip next instruction if VX != VY 
            if (chip8->V[X] != chip8->V[Y]) {
                chip8->PC += 2;
            }
            break;

        case 0x000A:            // ANNN: Set I = NNN 
            chip8->I = NNN;
            break;
        
        case 0x000B:            // BNNN: Jump to location NNN + V0 
            chip8->PC = chip8->V[0] + NNN;
            break;

        case 0x000C:            // Set VX = rand() % 256 & NN
            chip8->V[X] = (rand() % 256) & NN;
            break;

        case 0x000D:            // DXYN: Display N-byte sprite starting at memory location I at (X, Y), set VF = collision 
            {
            uint8_t x_coord = chip8->V[X] % SCREEN_WIDTH;
            uint8_t y_coord = chip8->V[Y] % SCREEN_HEIGHT;
            const uint8_t orig_x_coord = x_coord;
            chip8->V[0xF] = 0;

            // Loop through all N rows of the sprite 
            for (uint8_t j = 0; j < N; j++) {
                const uint8_t sprite_data = chip8->ram[chip8->I + j]; // Get next byte of sprite data 
                x_coord = orig_x_coord; // Reset x_coord for next row 

                // Checks each bit from left to right of the current row 
                for (int8_t k = 7; k >= 0; k--) {
                    // Set VF (carry flag) to 1 if any pixel is erased from the screen 
                    bool *pixel = &chip8->display[(y_coord * SCREEN_WIDTH) + x_coord];
                    const bool sprite_bit = (sprite_data & (1 << k));

                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;
                    }

                    // XOR sprite onto the existing display 
                    *pixel ^= sprite_bit;

                    // Stop drawing current row if you reach right edge of screen 
                    if (++x_coord >= SCREEN_WIDTH) {
                        break;
                    }
                }

                // Stop ALL drawing if you reach bottom of screen 
                if (++y_coord >= SCREEN_HEIGHT) {
                    break;
                }
            }
            break;
            }

        case 0x000E:
            switch (NN) {
                case 0x9E:      // EX9E: Skip next instruction if key stored in VX is pressed
                    if (chip8->keypad[chip8->V[X]]) {
                        chip8->PC += 2;
                    }
                    break;

                case 0xA1:      // EXA1: Skip next instruction if key stored in VX is not pressed
                    if (!chip8->keypad[chip8->V[X]]) {
                        chip8->PC += 2;
                    }
                    break;

                default:
                    break;
            }
            break;

        case 0x000F:
            switch (NN) {
                case 0x07:      // FX07: Sets VX to the delay timer
                    chip8->V[X] = chip8->delay_timer;
                    break;

                case 0x0A:      // FX0A: Stop all execution until a key is pressed AND released. Store in VX
                    {
                    static bool key_press = false;
                    static uint8_t key = 0xFF; // Garbage value until a key is pressed

                    for (uint8_t i = 0; key == 0xFF && i < sizeof(chip8->keypad); i++) {
                        if (chip8->keypad[i]) { // Check if key at current index is pressed
                            key = i;
                            key_press = true;
                            break;
                        }
                    }

                    if (!key_press) {
                        chip8->PC -= 2; // Repeats this instruction until a key is pressed
                    } else {
                        if (chip8->keypad[key]) { // Key is still pressed
                            chip8->PC -= 2;
                        } else { // Key has been pressed and released
                            chip8->V[X] = key;
                            key = 0xFF;
                            key_press = false;
                        }
                    }
                    break;
                    }

                case 0x15:      // FX15: Sets the delay timer to VX
                    chip8->delay_timer = chip8->V[X];
                    break;

                case 0x18:      // FX18: Sets the sound timer to VX
                    chip8->sound_timer = chip8->V[X];
                    break;

                case 0x1E:      // FX1E: Set I += VX
                    chip8->I += chip8->V[X];
                    break;

                case 0x29:      // FX29: Set I = location of sprite for the character in VX
                    chip8->I = chip8->V[X] * 5;
                    break;

                case 0x33:      // FX33: Extracts hundreds, tens, and ones digits of an
                                // 8-bit number in VX to I, I + 1, I + 2
                    {
                    uint8_t num = chip8->V[X];
                    chip8->ram[chip8->I + 2] = num % 10; // Ones place
                    num /= 10;
                    chip8->ram[chip8->I + 1] = num % 10; // Tens place
                    num /= 10;
                    chip8->ram[chip8->I] = num % 10;     // Hundreds place
                    break;
                    }   

                case 0x55:      // FX55: Store registers V0 through VX in memory starting at location I
                    {
                    int8_t hex = 0x0;
                    for (int8_t i = 0; i <= X; i++) {
                        chip8->ram[chip8->I + i] = chip8->V[hex++];
                    }
                    break;
                    }

                case 0x65:      // FX65: Read registers V0 through VX from memory starting at location I
                    {
                    int8_t hex = 0x0;
                    for (int8_t i = 0; i <= X; i++) {
                        chip8->V[hex++] = chip8->ram[chip8->I + i];
                    }
                    break;
                    }

                default:
                    break;
            }
            break;

        default:
            printf("Unimplemnted or Invalid opcode.\n");
            break;
    }
    
}

/* Update SDL window with any changes */
void update_screen(SDL_Renderer *renderer, const chip8_t *chip8) {
    // Background Color (0x16091F): R = 22, G = 9, B = 31
    // Foreground Color (0x8B7F94): R = 139, G = 127, B = 148 
    SDL_Rect rect = {.x = 0, .y = 0, .w = 20, .h = 20};

    // Loop through each display pixel, drawing each pixel as a rectangle 
    for (uint32_t i = 0; i < sizeof(chip8->display); i++) {

        // Translates 1D index to (X,Y) coordinates 
        rect.x = (i % SCREEN_WIDTH) * 20;
        rect.y = (i / SCREEN_WIDTH) * 20;

        if (chip8->display[i]) { // Draw foregrond color 
            SDL_SetRenderDrawColor(renderer, 139, 127, 148, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(renderer, &rect);
        } else { // Draw background color 
            SDL_SetRenderDrawColor(renderer, 22, 9, 31, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    SDL_RenderPresent(renderer);
}

/* Decrements timers by 60Hz if > 0 */
void update_timers(chip8_t *chip8, SDL_AudioDeviceID dev) {
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }

    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        SDL_PauseAudioDevice(dev, 0); // Play audio
    } else {
        SDL_PauseAudioDevice(dev, 1); // Pause Audio
    }
}

/* Shuts down all initialized SDL subsytems, renderer, window; frees any dynamically allocated memory */
void cleanup(SDL_Window *window, SDL_Renderer *renderer) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char *argv[]) {
    SDL_Window *window = {0};
    SDL_Renderer *renderer = {0};
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
    chip8_t chip8 = {0};

    // Check to see if user provided a ROM 
    if (argc != 2) {
        printf("Usage: ./main.exe <ROM/PATH.ch8>\n");
        exit(EXIT_FAILURE);
    } 

    // Initialize SDL
    if (!initialize_SDL(&window, &renderer, &want, &have, &dev, &chip8)) {
        exit(EXIT_FAILURE);
    } else { // Clear screen to background color 0x231130 
        clear_screen(renderer);
    }

    // Initialize CHIP-8 
    initialize_chip8(&chip8, argv[1]);

    // Seed random number generator 
    srand(time(NULL));

    // Game Loop 
    while (chip8.state != QUIT) { 
        handle_input(&chip8);

        if (chip8.state == PAUSED) {
            continue; // Do nothing until unpaused
        }

        // Get time before running instructions
        const uint64_t start = SDL_GetPerformanceCounter();
        
        // Run ~500 instructions per second
        for (uint32_t i = 0; i < 500 / 60; i++) {
            execute_instruction(&chip8);
        }

        // Get time after running instructions
        const uint64_t end = SDL_GetPerformanceCounter();
        
        // Delay for approximately 60Hz/60fps (16.67ms) or continue if instructions took longer
        const double elapsed_time = (double)((end - start) * 1000) / SDL_GetPerformanceFrequency();
        SDL_Delay(16.67 > elapsed_time ? 16.67 - elapsed_time : 0); 
        
        update_screen(renderer, &chip8);
        update_timers(&chip8, dev);
    } 

    // Cleanup before exit
    cleanup(window, renderer); 
    
    exit(EXIT_SUCCESS);
}