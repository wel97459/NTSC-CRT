/*****************************************************************************/
/*
 * NTSC/CRT - integer-only NTSC video signal encoding / decoding emulation
 * 
 *   by EMMIR 2018-2023
 *   
 *   YouTube: https://www.youtube.com/@EMMIR_KC/videos
 *   Discord: https://discord.com/invite/hdYctSmyQJ
 */
/*****************************************************************************/
//#define CRT_DO_BLOOM 1
#include "crt.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "ppm_rw.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define CMD_LINE_VERSION 0

#if CMD_LINE_VERSION

#define DRV_HEADER "NTSC/CRT by EMMIR 2018-2023\n"

static int dooverwrite = 1;
static int docolor = 1;
static int field = 0;
static int progressive = 0;
static int raw = 0;
static int phase_offset = 0;

static int
stoint(char *s, int *err)
{
    char *tail;
    long val;
    
    errno = 0;
    *err = 0;
    val = strtol(s, &tail, 10);
    if (errno == ERANGE) {
        printf("integer out of integer range\n");
        *err = 1;
    } else if (errno != 0) {
        printf("bad string: %s\n", strerror(errno));
        *err = 1;
    } else if (*tail != '\0') {
        printf("integer contained non-numeric characters\n");
        *err = 1;
    }
    return val;
}

static void
usage(char *p)
{
    printf(DRV_HEADER);
    printf("usage: %s -m|o|f|p|r|h outwidth outheight noise phase_offset infile outfile\n", p);
    printf("sample usage: %s -op 640 480 24 3 in.ppm out.ppm\n", p);
    printf("sample usage: %s - 832 624 0 2 in.ppm out.ppm\n", p);
    printf("-- NOTE: the - after the program name is required\n");
    printf("\tphase_offset is [0, 1, 2, or 3] +1 means a color phase change of 90 degrees\n");
    printf("------------------------------------------------------------\n");
    printf("\tm : monochrome\n");
    printf("\to : do not prompt when overwriting files\n");
    printf("\tf : odd field (only meaningful in progressive mode)\n");
    printf("\tp : progressive scan (rather than interlaced)\n");
    printf("\tr : raw image (needed for images that use artifact colors)\n");
    printf("\th : print help\n");
    printf("\n");
    printf("by default, the image will be full color, interlaced, and scaled to the output dimensions\n");
}

static int
process_args(int argc, char **argv)
{
    char *flags;

    flags = argv[1];
    if (*flags == '-') {
        flags++;
    }
    for (; *flags != '\0'; flags++) {
        switch (*flags) {
            case 'm': docolor = 0;     break;
            case 'o': dooverwrite = 0; break;
            case 'f': field = 1;       break;
            case 'p': progressive = 1; break;
            case 'r': raw = 1;         break;
            case 'h': usage(argv[0]); return 0;
            default:
                fprintf(stderr, "Unrecognized flag '%c'\n", *flags);
                return 0;
        }
    }
    return 1;
}

static int
fileexist(char *n)
{
    FILE *fp = fopen(n, "r");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

static int
promptoverwrite(char *fn)
{
    if (dooverwrite && fileexist(fn)) {
        do {
            char c = 0;
            printf("\n--- file (%s) already exists, overwrite? (y/n)\n", fn);
            scanf(" %c", &c);
            if (c == 'y' || c == 'Y') {
                return 1;
            }
            if (c == 'n' || c == 'N') {
                return 0;
            }
        } while (1);
    }
    return 1;
}

int
main(int argc, char **argv)
{
    struct NTSC_SETTINGS ntsc;
    struct CRT crt;
    int *img;
    int imgw, imgh;
    int *output = NULL;
    int outw = 832;
    int outh = 624;
    int noise = 24;
    char *input_file;
    char *output_file;
    int err = 0;
    int phase_ref[4] = { 0, 1, 0, -1 };

    if (argc < 8) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    if (!process_args(argc, argv)) {
        return EXIT_FAILURE;
    }

    printf(DRV_HEADER);
        
    outw = stoint(argv[2], &err);
    if (err) {
        return EXIT_FAILURE;
    }

    outh = stoint(argv[3], &err);
    if (err) {
        return EXIT_FAILURE;
    }

    noise = stoint(argv[4], &err);
    if (err) {
        return EXIT_FAILURE;
    }
    
    if (noise < 0) noise = 0;

    phase_offset = stoint(argv[5], &err);
    if (err) {
        return EXIT_FAILURE;
    }
    phase_offset &= 3;
    
    output = calloc(outw * outh, sizeof(int));
    if (output == NULL) {
        printf("out of memory\n");
        return EXIT_FAILURE;
    }
    
    input_file = argv[6];
    output_file = argv[7];

    if (!ppm_read24(input_file, &img, &imgw, &imgh, calloc)) {
        printf("unable to read image\n");
        return EXIT_FAILURE;
    }
    printf("loaded %d %d\n", imgw, imgh);

    if (!promptoverwrite(output_file)) {
        return EXIT_FAILURE;
    }
  
    crt_init(&crt, outw, outh, output);

    ntsc.rgb = img;
    ntsc.w = imgw;
    ntsc.h = imgh;
    ntsc.as_color = docolor;
    ntsc.field = field & 1;
    ntsc.raw = raw;
    ntsc.cc[0] = phase_ref[(phase_offset + 0) & 3];
    ntsc.cc[1] = phase_ref[(phase_offset + 1) & 3];
    ntsc.cc[2] = phase_ref[(phase_offset + 2) & 3];
    ntsc.cc[3] = phase_ref[(phase_offset + 3) & 3];
    
    printf("converting to %dx%d...\n", outw, outh);
    err = 0;
    /* accumulate 4 frames */
    while (err < 4) {
        crt_2ntsc(&crt, &ntsc);
        crt_draw(&crt, noise);
        if (!progressive) {
            ntsc.field ^= 1;
            crt_2ntsc(&crt, &ntsc);
            crt_draw(&crt, noise);
        }
        err++;
    }
    if (!ppm_write24(output_file, output, outw, outh)) {
        printf("unable to write image\n");
        return EXIT_FAILURE;
    }
    printf("done\n");
    return EXIT_SUCCESS;
}
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#if 0
#define XMAX 624
#define YMAX 832
#else
#define XMAX 832
#define YMAX 624
#endif
static int *video = NULL;

static struct CRT crt;

static stbi_uc *img = NULL;
static stbi_uc *imgload = NULL;
static int imgw;
static int imgh;

static int color = 1;
static int noise = 0;
static int field = 0;
static int progressive = 1;
static int raw = 0;
static int roll = 0;
static int hs = 4;
static int vs = 100;
static int frame = 1;
static int play = 0;
SDL_Renderer *renderer;
SDL_Renderer *renderer_test;
SDL_Texture *vidTex;
SDL_Texture *texTarget;
SDL_Texture* Message;
SDL_Surface* surfaceMessage;
SDL_Surface* pScreenShot = NULL;
SDL_Rect vidDest;
SDL_Window *window;
TTF_Font *font;
static int load_frame(char *loc)
{
    char name[256];
    sprintf(name, "../SMPTE_Color_Bars.png", frame);

    int n;
    if(imgload != NULL) stbi_image_free(imgload);
    imgload = stbi_load(name, &imgw, &imgh, &n, 4);
    img = imgload;

    if(frame<500){
        SDL_Rect Message_rect;
        Message_rect.x = imgw-(surfaceMessage->clip_rect.w+16);  //controls the rect's x coordinate 
        Message_rect.y = 16; // controls the rect's y coordinte
        Message_rect.w = surfaceMessage->clip_rect.w; // controls the width of the rect
        Message_rect.h = surfaceMessage->clip_rect.h; // controls the height of the rect

        
        if(pScreenShot == NULL){
            // Create an empty RGB surface that will be used to create the screenshot bmp file
            pScreenShot = SDL_CreateRGBSurface(0, imgw, imgh, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            texTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, imgw, imgh);
        }

        SDL_SetRenderTarget(renderer, texTarget);
        SDL_UpdateTexture(texTarget, NULL, imgload, imgw * sizeof(Uint32));
        SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
        // Read the pixels from the current render target and save them onto the surface
        SDL_RenderReadPixels(renderer, NULL, SDL_GetWindowPixelFormat(window), pScreenShot->pixels, pScreenShot->pitch);
	    //Detach the texture
	    SDL_SetRenderTarget(renderer, NULL);

        img = pScreenShot->pixels;
    }


    if(imgload == NULL){
        printf("name: %s, err:%s\n",name, stbi_failure_reason());
        return -1;
    }
    return n;
}

static void save_frame(char *loc, int f){
    char name[256];
    sprintf(name, "/home/winston/Downloads/youtube-dl/frames_out/img%04i.png", f);
    stbi_write_png(name, XMAX, YMAX, 4, video, XMAX*4);
}

static void fade_phosphors(void)
{
    int i, *v;
    unsigned int c;
    
    v = video;
    
    for (i = 0; i < XMAX * YMAX; i++) {
        c = v[i] & 0xffffff;
        v[i] = (c >> 1 & 0x7f7f7f) +
               (c >> 2 & 0x3f3f3f) +
               (c >> 3 & 0x1f1f1f) +
               (c >> 4 & 0x0f0f0f);
    }
}

static void displaycb(void)
{
    static struct NTSC_SETTINGS ntsc;

    if(play){
        load_frame(NULL);
        frame++;
    }

    //fade_phosphors();
    field ^= 1;
    ntsc.rgb = (int *)img;
    ntsc.w = imgw;
    ntsc.h = imgh;
    ntsc.as_color = color;
    ntsc.field = field & 1;
    ntsc.raw = raw;
    ntsc.cc[0] = 1;
    ntsc.cc[1] = 0;
    ntsc.cc[2] = -1;
    ntsc.cc[3] = 0;
    roll+=10;
    crt_2ntsc(&crt, &ntsc);
    crt_draw(&crt, noise, roll, vs, hs);
    if(play){
        save_frame(NULL, frame-1);
    }
}

int handleInput()
{
	SDL_Event event;
	//event handling, check for close window, escape key and mouse clicks
	//return -1 when exit requested
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			return -1;

		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE)
				return -1;

            if (event.key.keysym.sym == SDLK_1) {
                    crt.saturation -= 1;
                    printf("%d\n", crt.saturation);
            }
            if (event.key.keysym.sym == SDLK_2) {
                crt.saturation += 1;
                printf("%d\n", crt.saturation);
            }

            if (event.key.keysym.sym == SDLK_q) {
                crt.black_point += 1;
                printf("crt.black_point   %d\n", crt.black_point);
            }
            if (event.key.keysym.sym == SDLK_a) {
                crt.black_point -= 1;
                printf("crt.black_point   %d\n", crt.black_point);
            }

            if (event.key.keysym.sym == SDLK_w) {
                crt.white_point += 1;
                printf("crt.white_point   %d\n", crt.white_point);
            }
            if (event.key.keysym.sym == SDLK_s) {
                crt.white_point -= 1;
                printf("crt.white_point   %d\n", crt.white_point);
            }

            if (event.key.keysym.sym == SDLK_UP) {
                crt.brightness += 1;
                printf("%d\n", crt.brightness);
            }
            if (event.key.keysym.sym == SDLK_DOWN) {
                crt.brightness -= 1;
                printf("%d\n", crt.brightness);
            }
            if (event.key.keysym.sym == SDLK_LEFT) {
                crt.contrast -= 1;
                printf("%d\n", crt.contrast);
            }
            if (event.key.keysym.sym == SDLK_RIGHT) {
                crt.contrast += 1;
                printf("%d\n", crt.contrast);
            }
            if (event.key.keysym.sym == SDLK_3) {
                noise -= 1;
                if (noise < 0) {
                    noise = 0;
                }
                printf("%d\n", noise);
            }
            if (event.key.keysym.sym == SDLK_4) {
                noise += 1;
                printf("%d\n", noise);
            }
            if (event.key.keysym.sym == SDLK_SPACE) {
                color ^= 1;
            }
            if (event.key.keysym.sym == SDLK_r) {
                crt_reset(&crt);
                color = 1;
                //noise = 0;
                field = 0;
                progressive = 1;
                raw = 0;
                frame = 1;
            }
            if (event.key.keysym.sym == SDLK_f) {
                field ^= 1;
                printf("field: %d\n", field);
            }
            if (event.key.keysym.sym == SDLK_e) {
                progressive ^= 1;
                printf("progressive: %d\n", progressive);
            }
            if (event.key.keysym.sym == SDLK_t) {
                raw ^= 1;
                printf("raw: %d\n", raw);
            }

            if (event.key.keysym.sym == SDLK_p) {
                play ^= 1;
                printf("play: %d\n", raw);
            }

            if (event.key.keysym.sym == SDLK_h) {
                hs -= 1;
                if (hs < 0) {
                    hs = 0;
                }
                printf("%d\n", hs);
            }
            if (event.key.keysym.sym == SDLK_y) {
                hs += 1;
                printf("%d\n", hs);
            }

            if (event.key.keysym.sym == SDLK_j) {
                vs -= 1;
                if (vs < 0) {
                    vs = 0;
                }
                printf("%d\n", vs);
            }
            if (event.key.keysym.sym == SDLK_u) {
                vs += 1;
                printf("%d\n", vs);
            }

            if (event.key.keysym.sym == SDLK_o) {
                save_frame(NULL, 1);
            }

            if (event.key.keysym.sym == SDLK_COMMA) {
                frame -= 1;
                if (frame < 1) {
                    frame = 1;
                }
                printf("%d\n", frame);
            }
            if (event.key.keysym.sym == SDLK_PERIOD) {
                frame += 1;
                printf("%d\n", frame);
            }

            if(event.key.keysym.sym == SDLK_PERIOD || event.key.keysym.sym == SDLK_COMMA){
                load_frame(NULL);
            }

            if (!progressive) {
                field ^= 1;
            }
        }
	}

	const Uint8* keystates = SDL_GetKeyboardState(NULL);

	return 0;
}


int main(int argc, char **argv)
{
    //setup SDL with title, 640x480, and load font
	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Unable to initialize SDL: %s\n", SDL_GetError());
		return 0;
	}
    window = SDL_CreateWindow("NTSC - SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, XMAX, YMAX, 0);
	if (!window) {
		printf("Can't create window: %s\n", SDL_GetError());
		return 0;
	}

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    /* Initialize the TTF library */
    if (TTF_Init() < 0) {
            fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
            SDL_Quit();
            return 0;
    }

    font = TTF_OpenFont("../VCR_OSD_MONO_1.001.ttf", 50);
    if(!font) {
        printf("TTF_OpenFont: %s\n", TTF_GetError());
        // handle error
    }
    assert(font);

    vidDest.x = (XMAX/2);
	vidDest.y = (YMAX/2);
	vidDest.w = XMAX;
	vidDest.h = YMAX;

    SDL_Color White = {255, 255, 255};

    // as TTF_RenderText_Solid could only be used on
    // SDL_Surface then you have to create the surface first
    const Uint16 text[]=u"PLAY \u25ba";
    surfaceMessage = TTF_RenderUNICODE_Solid(font, text, White);

    // now you can convert it into a texture
    Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);


	vidTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING, XMAX, YMAX);

    frame = 200;
    int n = load_frame(NULL);

    video = malloc(XMAX * sizeof(int) * YMAX);
    crt_init(&crt, XMAX, YMAX, video);

    //SDL_UpdateTexture(vidTex, NULL, img, width * sizeof(unsigned char) * n);
    int fps = 0;
    Uint32 t = SDL_GetTicks()+1000, T, TT = SDL_GetTicks();
    do{
        T = SDL_GetTicks();
        //if(T > TT){
            displaycb();
            SDL_UpdateTexture(vidTex, NULL, video, XMAX * sizeof(Uint32));
            //clear screen, draw each element, then flip the buffer
            // Clear the screen
            //SDL_RenderClear(renderer);
            // Render the texture
            SDL_RenderCopy(renderer, vidTex, NULL, NULL);
            // Update the screen
            SDL_RenderPresent(renderer);
            fps++;
            if(T > t){
                printf("FPS:%i\n", fps);
                fps = 0;
                t = T + 1000;
            }
            //TT = SDL_GetTicks() + 4;
        //}
    //run until exit requested
    } while (handleInput() >= 0);

    //stbi_image_free(img);
    free(vidTex);
    free(window);
    free(renderer);
    free(video);
    return EXIT_SUCCESS;
}

#endif
