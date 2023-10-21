/* Based on node-formidable by Felix Geisendörfer 
 * Igor Afonov - afonov@gmail.com - 2012
 * MIT License - http://www.opensource.org/licenses/mit-license.php
 */
#ifndef _multipart_parser_h
#define _multipart_parser_h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <ctype.h>

typedef struct multipart_parser_settings multipart_parser_settings;
typedef struct multipart_parser_state multipart_parser_state;

typedef struct multipart_parser {
  void * data;

  size_t index;
  size_t boundary_length;

  unsigned char state;

  const multipart_parser_settings* settings;

  // https://www.w3.org/Protocols/rfc1341/7_2_Multipart.html
  char multipart_boundary[73];
  char lookbehind[75];
} multipart_parser;

typedef int (*multipart_data_cb) (multipart_parser*, const char *at, size_t length);
typedef int (*multipart_notify_cb) (multipart_parser*);

struct multipart_parser_settings {
  multipart_data_cb on_header_field;
  multipart_data_cb on_header_value;
  multipart_data_cb on_part_data;

  multipart_notify_cb on_part_data_begin;
  multipart_notify_cb on_headers_complete;
  multipart_notify_cb on_part_data_end;
  multipart_notify_cb on_body_end;
};

void multipart_parser_init(multipart_parser* p, const char* boundary, const multipart_parser_settings* settings);

size_t multipart_parser_execute(multipart_parser* p, const char *buf, size_t len);

void multipart_parser_set_data(multipart_parser* p, void* data);
void * multipart_parser_get_data(multipart_parser* p);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif