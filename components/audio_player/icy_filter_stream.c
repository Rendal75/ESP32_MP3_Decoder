/*
 * icy_filter_stream.c
 *
 *  Created on: Jan 6, 2020
 *      Author: mzmarzlo
 */

#include <string.h>
#include <assert.h>

#include "icy_filter_stream.h"
#include "esp_log.h"

#define TAG "icy_filter_stream"

#define min(a, b)                                                              \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a < _b ? _a : _b;                                                     \
    })


void icy_filter_stream_init(icy_filter_stream* s) {
	assert(s);
	s->bytes_before_next_meta = s->icy_interval;
	ESP_LOGW(TAG, "Init with icy_interval=%d", s->icy_interval);
}

void icy_filter_stream_write(icy_filter_stream* s, const char* ptr, int size) {

	assert(s);
	assert(ptr);
	assert(size>0);

    while(size>0) {

		if(s->bytes_before_next_meta<0) {
			ESP_LOGI(TAG, "Processing meta ");
			// we are inside a meta, abs value gives remaining
			int remaining = - s->bytes_before_next_meta;

			// TODO: handle buffer overflow
			if(size<remaining) {
				// meta will continue on next write call
				strncpy(&s->meta_buffer[s->meta_buffer_pos], ptr , size);
				s->bytes_before_next_meta += size;
				s->meta_buffer_pos +=size;
				break;
			}
			else {
				strncpy(&s->meta_buffer[s->meta_buffer_pos], ptr , remaining);
				ptr += remaining;
				size -= remaining;
				s->meta_buffer[s->meta_buffer_pos+remaining] = '\0';
				s->bytes_before_next_meta = s->icy_interval;
				s->meta_buffer_pos = 0;
				s->on_meta(s->meta_buffer);
			}
		}

		if(size==0) {
			break;
		}

		// will we start a meta tag in this call ?
		int raw_size;
		bool meta_is_following = s->icy_interval && s->bytes_before_next_meta<size;
		if(meta_is_following) {
			raw_size = s->bytes_before_next_meta;
		}
		else{
			raw_size = size;
		}

		if(raw_size) {
			s->out(ptr, raw_size);
			ptr += raw_size;
			s->bytes_before_next_meta -= raw_size;
			size -= raw_size;
		}

		if(meta_is_following && size>0) {
			int meta_length = *ptr * 16;
			if(meta_length) {
				s->bytes_before_next_meta = -meta_length;
			}
			else {
				// Not tag
				s->bytes_before_next_meta = s->icy_interval;
			}
			ptr++;
			size--;
		}
    }

}
