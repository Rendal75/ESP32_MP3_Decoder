/*
 * icy_filter_stream.h
 *
 *  Created on: Jan 6, 2020
 *      Author: mzmarzlo
 */

#ifndef ICY_FILTER_STREAM_H_
#define ICY_FILTER_STREAM_H_

#define ICY_FILTER_STREAM_META_BUFFER_SIZE	256

typedef struct {
	// config fields
	void (*out)(const char* ptr, int size);
	void (*on_meta)(char* meta);
	int icy_interval;

	// instance fields
	// if < 0, we are inside a meta, and abs values gives remaining bytes inside meta
	int bytes_before_next_meta;
	char meta_buffer[ICY_FILTER_STREAM_META_BUFFER_SIZE];
	int meta_buffer_pos;
} icy_filter_stream;


void icy_filter_stream_init(icy_filter_stream* stream);
void icy_filter_stream_write(icy_filter_stream* stream, const char* ptr, int size);

#endif /* ICY_FILTER_STREAM_H_ */
