/*
 * icy_parcer.c
 *
 *  Created on: Jan 17, 2020
 *      Author: mzmarzlo
 */

#include "icy_parser.h"

void icy_parser_init(const char* data, icy_parser_t* parser) {
	parser->data = data;
	parser->pos = 0;
}

/**
 * @returns true if found next, false if over
 */
bool icy_parser_next(icy_parser_t* parser) {
	if(parser->pos==-1) {
		return false;
	}

	int key_len=-1;
	int value_len=-1;
	int pos = parser->pos;
	const char* data = parser->data;
	parser->key_start = pos;
	// search for key, by searching '='
	for(;;) {
		char c = data[pos];
		if(c=='\0') {
			parser->pos=-1;
			return false;
		}
		if(key_len==-1) {
			// still  searching for key
			if(c!='=') {
				pos++;
				continue;
			}
			// '=' found

			key_len = pos - parser->pos;
			pos++;
			// we expect "'"
			if(data[pos] != '\'') {
				return false;
			}
			pos++;
			parser->value_start = pos;
		}
		else if(value_len==-1) {
			// still  searching for end of value ( "'" )
			if(c!='\'') {
				pos++;
				continue;
			}
			parser->value_len = pos - parser->value_start;
			return true;
		}
		else {
			return false;
		}
	}

}
