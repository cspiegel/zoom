//
//  ZoomZMachine.m
//  ZoomCocoa
//
//  Created by Andrew Hunter on Wed Sep 10 2003.
//  Copyright (c) 2003 Andrew Hunter. All rights reserved.
//

#import "ZoomZMachine.h"
#import "ZoomServer.h"

#include "sys/time.h"


#include "zmachine.h"
#include "interp.h"
#include "random.h"
#include "file.h"
#include "zscii.h"
#include "display.h"
#include "rc.h"
#include "stream.h"
#include "blorb.h"
#include "v6display.h"
#include "state.h"

@implementation ZoomZMachine

- (id) init {
    self = [super init];

    if (self) {
        display = nil;
        machineFile = NULL;

        inputBuffer = [[NSMutableString allocWithZone: [self zone]] init];
        outputBuffer = [[ZBuffer allocWithZone: [self zone]] init];
        lastFile = nil;

        windows[0] = windows[1] = windows[2] = nil;

        int x;
        for(x=0; x<3; x++) {
            windowBuffer[x] = [[NSMutableAttributedString alloc] init];
        }
		
		[[NSNotificationCenter defaultCenter] addObserver: self
												 selector: @selector(flushBuffers)
													 name: ZBufferNeedsFlushingNotification
												   object: nil];
    }

    return self;
}

- (void) dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver: self];
	
    if (windows[0])
        [windows[0] release];
    if (windows[1])
        [windows[1] release];
    if (windows[2])
        [windows[2] release];

    int x;
    for (x=0; x<3; x++) {
        [windowBuffer[x] release];
    }

    [display release];
    [inputBuffer release];
    [outputBuffer release];

    mainMachine = nil;
    
    if (lastFile) [lastFile release];

    if (machineFile) {
        close_file(machineFile);
    }
    
    [super dealloc];
}

- (NSString*) description {
    return @"Zoom 1.0.2 ZMachine object";
}

// = Setup =
- (void) loadStoryFile: (NSData*) storyFile {
    // Create the machine file
    ZDataFile* file = [[ZDataFile alloc] initWithData: storyFile];
    machineFile = open_file_from_object([file autorelease]);
	
	// Start initialising the Z-Machine
	// (We do this so that we can load a save state at any time after this call)
	
	wasRestored = NO;
	
    // RNG
    struct timeval tv;
    gettimeofday(&tv, NULL);
    random_seed(tv.tv_sec^tv.tv_usec);
	
    // Some default options
	rc_load(); // DELETEME: TEST FOR BUG
	rc_hash = hash_create();
	
	rc_defgame = malloc(sizeof(rc_game));
	rc_defgame->name = "";
	rc_defgame->interpreter = 3;
	rc_defgame->revision = 'Z';
	rc_defgame->fonts = NULL;
	rc_defgame->n_fonts = 0;
	rc_defgame->colours = NULL;
	rc_defgame->n_colours = 0;
	rc_defgame->gamedir = rc_defgame->savedir = rc_defgame->sounds = rc_defgame->graphics = NULL;
	rc_defgame->xsize = 80;
	rc_defgame->ysize = 25;
	rc_defgame->antialias = 1;
	
	hash_store(rc_hash, "default", 7, rc_defgame);
	
    // Load the story
    machine.story_length = get_size_of_file(machineFile);
    zmachine_load_file(machineFile, &machine);
}

- (BOOL) loadResourcesFromData: (in bycopy NSData*) resources {
    return NO; // IMPLEMENT ME
}

- (BOOL) loadResourcesFromFile: (in bycopy NSFileHandle*) file {
    return NO; // IMPLEMENT ME
}

- (BOOL) loadResourcesFromZFile: (in byref NSObject<ZFile>*) file {
    return NO; // IMPLEMENT ME
}

// = Running =
- (void) startRunningInDisplay: (in byref NSObject<ZDisplay>*) disp {
    NSAutoreleasePool* mainPool = [[NSAutoreleasePool alloc] init];
    /*
    {
        int x;
        for (x=0; x<10; x++) {
            NSLog(@"...%i...", x);
            sleep(1);
        }
    }
     */
    
    display = [disp retain];

    // OK, we can now set up the ZMachine and get running

    // Setup the display
    windows[0] = NULL;
    windows[1] = NULL;
    windows[2] = NULL;

    // Cycle the autorelease pool
    displayPool = [[NSAutoreleasePool alloc] init];
    
    switch (machine.header[0]) {
        case 3:
            // Status window

        case 4:
        case 5:
        case 7:
        case 8:
            // Upper/lower window
            windows[0] = [[display createLowerWindow] retain];
            windows[1] = [[display createUpperWindow] retain];
            windows[2] = [[display createUpperWindow] retain];
            break;

        case 6:
            // Implement me
            break;
    }

    int x;
    for (x=0; x<3; x++) {
        [windows[x] setProtocolForProxy: @protocol(ZVendor)];
    }

    // Setup the display, etc
    rc_set_game(zmachine_get_serial(), Word(ZH_release), Word(ZH_checksum));
    display_initialise();

    // Start running the machine
    switch (machine.header[0])
    {
#ifdef SUPPORT_VERSION_3
        case 3:
            display_split(1, 1);

            display_set_colour(0, 7); display_set_font(0);
            display_set_window(0);
            if (!wasRestored) zmachine_run(3, NULL); else zmachine_runsome(3, machine.zpc);
            break;
#endif
#ifdef SUPPORT_VERSION_4
        case 4:
            if (!wasRestored) zmachine_run(4, NULL); else zmachine_runsome(4, machine.zpc);
            break;
#endif
#ifdef SUPPORT_VERSION_5
        case 5:
            if (!wasRestored) zmachine_run(5, NULL); else zmachine_runsome(5, machine.zpc);
            break;
        case 7:
            if (!wasRestored) zmachine_run(7, NULL); else zmachine_runsome(7, machine.zpc);
            break;
        case 8:
            if (!wasRestored) zmachine_run(8, NULL); else zmachine_runsome(8, machine.zpc);
            break;
#endif
#ifdef SUPPORT_VERSION_6
        case 6:
            v6_startup();
            v6_set_cursor(1,1);
            if (!wasRestored) zmachine_run(6, NULL); else zmachine_runsome(6, machine.zpc);
            break;
#endif

        default:
            zmachine_fatal("Unsupported ZMachine version %i", machine.header[0]);
            break;
    }

    display_finalise();
    [mainPool release];
	
	display_exit(0);
}

// = Debugging =
- (NSData*) staticMemory {
}

// = Autosave =
- (NSData*) createGameSave {
	// Create a save game, for autosave purposes
	int len;
	
	if (!machine.can_autosave) return nil;
	
	void* gameData = state_compile(&machine.stack, machine.zpc, &len, 1);
	
	NSData* result = [NSData dataWithBytes: gameData length: len];
	
	free(gameData);
	
	return result;
}

- (void) restoreSaveState: (NSData*) saveData {
	const ZByte* gameData = [saveData bytes];
	
	// NOTE: suppresses a warning (but it should be OK)
	state_decompile((ZByte*)gameData, &machine.stack, &machine.zpc, [saveData length]);
	wasRestored = YES;
}

// = Receiving text/characters =
- (void) inputText: (NSString*) text {
    [inputBuffer appendString: text];
}

//- (void) inputChar: (int) character {
//}

// = Receiving files =
- (void) filePromptCancelled {
    if (lastFile) {
        [lastFile release];
        lastFile = nil;
        lastSize = -1;
    }
    
    filePromptFinished = YES;
}

- (void) promptedFileIs: (NSObject<ZFile>*) file
                   size: (int) size {
    if (lastFile) [lastFile release];
    
    lastFile = [file retain];
    lastSize = size;
    
    filePromptFinished = YES;
}

- (void) filePromptStarted {
    filePromptFinished = NO;
    if (lastFile) {
        [lastFile release];
        lastFile = nil;
    }
}

- (BOOL) filePromptFinished {
    return filePromptFinished;
}

- (NSObject<ZFile>*) lastFile {
    return lastFile;
}

- (int) lastSize {
    return lastSize;
}

- (void) clearFile {
    if (lastFile) {
        [lastFile release];
        lastFile = nil;
    }
}

// = Our own functions =
- (NSObject<ZWindow>*) windowNumber: (int) num {
    if (num < 0 || num > 2) {
        NSLog(@"*** BUG - window %i does not exist", num);
        return nil;
    }
    
    return windows[num];
}

- (NSObject<ZDisplay>*) display {
    return display;
}

- (NSMutableString*) inputBuffer {
    return inputBuffer;
}

// = Buffering =

- (ZBuffer*) buffer {
    return outputBuffer;
}

- (void) flushBuffers {
    [display flushBuffer: outputBuffer];
    [outputBuffer release];
    outputBuffer = [[ZBuffer allocWithZone: [self zone]] init];
}

// = Display size =
- (void) displaySizeHasChanged {
    zmachine_resize_display(display_get_info());
}

@end

// = Fatal errors and warnings =
void  zmachine_fatal(char* format, ...) {
	char fatalBuf[512];
	va_list  ap;
	
	va_start(ap, format);
	vsnprintf(fatalBuf, 512, format, ap);
	va_end(ap);
	
	fatalBuf[511] = 0;
	
	[[mainMachine display] displayFatalError: [NSString stringWithCString: fatalBuf]];
	
	display_exit(1);
}

void  zmachine_warning(char* format, ...) {
	char fatalBuf[512];
	va_list  ap;
	
	va_start(ap, format);
	vsnprintf(fatalBuf, 512, format, ap);
	va_end(ap);
	
	fatalBuf[511] = 0;
	
	[[mainMachine display] displayWarning: [NSString stringWithCString: fatalBuf]];
}

// Various Zoom C functions (not yet implemented elsewhere)
#include "file.h"
#include "display.h"

// = V6 display =

extern int   display_init_pixmap    (int width, int height) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern void  display_plot_rect      (int x, int y,
                                     int width, int height) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern void  display_scroll_region   (int x, int y,
                                      int width, int height,
                                      int xoff, int yoff) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern void  display_pixmap_cols     (int fg, int bg) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern int   display_get_pix_colour  (int x, int y) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern void  display_plot_gtext      (const int* buf, int len,
                                      int style, int x, int y) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern void  display_plot_image      (BlorbImage* img, int x, int y) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern float display_measure_text    (const int* buf, int len, int style) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern float display_get_font_width  (int style) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern float display_get_font_height (int style) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern float display_get_font_ascent (int style) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern float display_get_font_descent(int style) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern void  display_wait_for_more   (void) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }

extern void  display_read_mouse      (void) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern int   display_get_pix_mouse_b (void) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern int   display_get_pix_mouse_x (void) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern int   display_get_pix_mouse_y (void) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }

extern void  display_set_input_pos   (int style, int x, int y, int width) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
extern void  display_set_mouse_win   (int x, int y, int width, int height) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }

// = Images =
image_data*    image_load       (ZFile* file,
                                 int offset,
                                 int len,
                                 image_data* palimg) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
void           image_unload     (image_data* img) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
void           image_unload_rgb (image_data* img) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }

int            image_cmp_palette(image_data* img1, image_data* img2) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }

int            image_width      (image_data* img) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
int            image_height     (image_data* img) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
unsigned char* image_rgb        (image_data* img) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }

void           image_resample   (image_data* img, int n, int d) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }

void           image_set_data   (image_data* img, void* data,
                                 void (*destruct)(image_data*, void*)) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
void*          image_get_data   (image_data* img) { NSLog(@"Function not implemented: %s %i", __FILE__, __LINE__); }
