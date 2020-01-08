/*
 * screen.h
 *
 */

#ifndef _INCLUDE_SCREEN_H_
#define _INCLUDE_SCREEN_H_

#include "playlist.h"

void initialise_screen(void);
void update_header(char *hdr, char *ftr);

void screen_on_meta(char* meta);
void screen_on_fifo_buffer(int current_file, int size);
void screen_on_entry_changed(playlist_entry_t* entry);
void screen_refreshTime(void);

#endif /* _INCLUDE_SCREEN_H_ */
