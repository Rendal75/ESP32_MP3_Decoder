/*
 * icy_parser.h
 *
 *  Created on: Jan 17, 2020
 *      Author: mzmarzlo
 */

#ifndef COMPONENTS_WEB_RADIO_INCLUDE_ICY_PARSER_H_
#define COMPONENTS_WEB_RADIO_INCLUDE_ICY_PARSER_H_

#include "stdbool.h"

typedef struct icy_parser {
	const char* data;
	int key_start;
	int key_len;
	int value_start;
	int value_len;
	int pos;
} icy_parser_t;

void icy_parser_init(const char* data, icy_parser_t* parser);

/**
 * If true is returned, key_start, key_len, value_start, value_len have correct values
 *
 * @returns true if found next, false if over
 *
 */
bool icy_parser_next(icy_parser_t* parser);

#endif /* COMPONENTS_WEB_RADIO_INCLUDE_ICY_PARSER_H_ */
