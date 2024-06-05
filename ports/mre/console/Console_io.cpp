#include "common.h"
#include "Console.h"
#include "T2Input.h"

#include "Console_io.h"

extern Console console;
extern T2Input t2input;

extern "C" void console_char_in(char ch){
	console.put_c(ch);
}

extern "C" void console_str_in(const char* str){
	console.putstr(str);
}

extern "C" void console_str_with_length_in(const char* str, int length){
	console.putstr(str, length);
}

extern "C" void console_char_out(char ch){
	upy_repl_rx(ch);
}

extern "C" void console_str_out(const char* str){
	for(unsigned int i = 0; i < strlen(str); i++){
		console_char_out(str[i]);
	}
}

extern "C" void console_str_with_length_out(const char* str, int length){
	for (int i = 0; i < length; i++) {
		console_char_out(str[i]);
	}
}
