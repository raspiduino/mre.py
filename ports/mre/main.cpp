// MRE headers
#include "vmtimer.h"

// MRE console headers
#include "console/common.h"
#include "console/Console.h"
#include "console/Console_io.h"
#include "console/T2Input.h"

// Micropython
extern "C" {
#include "upymain.h"
}

// Global variables
int scr_w = 0, scr_h =0;
VMUINT8 *layer_bufs[2] = {0,0};
VMINT layer_hdls[2] = {-1,-1};

Console console;
T2Input t2input;

int main_timer_id = -1;

// MRE callback functions prototype
void handle_sysevt(VMINT message, VMINT param);
void handle_keyevt(VMINT event, VMINT keycode);
void handle_penevt(VMINT event, VMINT x, VMINT y);

// Main MRE entry
// Real main() is in mre-makefile/src/gccmain.c
void vm_main(void) {
    // Get screen width and height
    scr_w = vm_graphic_get_screen_width(); 
	scr_h = vm_graphic_get_screen_height();

    // Init console & input
	console.init();
	t2input.init();

    // Register event handlers
	vm_reg_sysevt_callback(handle_sysevt);
	vm_reg_keyboard_callback(handle_keyevt);
	vm_reg_pen_callback(handle_penevt);
}

// Function to re-render the terminal
void draw(int tid){
	vm_graphic_fill_rect(layer_bufs[1], 0, 0, scr_w, scr_h, tr_color, tr_color);
	vm_graphic_line(layer_bufs[1], console.cursor_x*char_width, (console.cursor_y+1)*char_height,
		(console.cursor_x+1)*char_width, (console.cursor_y+1)*char_height, console.cur_textcolor);
	t2input.draw();
	vm_graphic_flush_layer(layer_hdls, 2);
}

// MRE system event handler
void handle_sysevt(VMINT message, VMINT param) {
	switch (message) {
	case VM_MSG_CREATE:
	case VM_MSG_ACTIVE:
		layer_hdls[0] = vm_graphic_create_layer(0, 0, scr_w, scr_h, -1);
		layer_hdls[1] = vm_graphic_create_layer(0, 0, scr_w, scr_h, tr_color);
		
		vm_graphic_set_clip(0, 0, scr_w, scr_h);

		layer_bufs[0] = vm_graphic_get_layer_buffer(layer_hdls[0]);
		layer_bufs[1] = vm_graphic_get_layer_buffer(layer_hdls[1]);

		vm_switch_power_saving_mode(turn_off_mode);

		console.scr_buf = layer_bufs[0];
		console.draw_all();

		t2input.scr_buf = layer_bufs[1];
		t2input.layer_handle = layer_hdls[1];

        // Init main_timer on the first run, to render terminal
		if(main_timer_id == -1) {
            main_timer_id = vm_create_timer(1000/15, draw); //15 fps
			
			// Init Micropython & start REPL
    		upy_init();
			upy_repl_init();
        }
		break;
		
	case VM_MSG_PAINT:
		draw(0);
		break;
		
	case VM_MSG_INACTIVE:
		vm_switch_power_saving_mode(turn_on_mode);
		if (layer_hdls[0] != -1) {
			vm_graphic_delete_layer(layer_hdls[0]);
			vm_graphic_delete_layer(layer_hdls[1]);
		}
		
        if (main_timer_id != -1) {
            vm_delete_timer(main_timer_id);
            upy_deinit();
        }
		break;	
	case VM_MSG_QUIT:
		if (layer_hdls[0] != -1) {
			vm_graphic_delete_layer(layer_hdls[0]);
			vm_graphic_delete_layer(layer_hdls[1]);
		}

		if (main_timer_id != -1) {
            vm_delete_timer(main_timer_id);
            upy_deinit();
        }
		break;	
	}
}

void handle_keyevt(VMINT event, VMINT keycode) {
#ifdef WIN32
	if(keycode>=VM_KEY_NUM1&&keycode<=VM_KEY_NUM3)
		keycode+=6;
	else if(keycode>=VM_KEY_NUM7&&keycode<=VM_KEY_NUM9)
		keycode-=6;
#endif
	t2input.handle_keyevt(event, keycode);
}

void handle_penevt(VMINT event, VMINT x, VMINT y){
	t2input.handle_penevt(event, x, y);
	draw(0);
}
