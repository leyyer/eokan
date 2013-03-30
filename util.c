/* 
 * Copyright (c) 2013, Renyi su <surenyi@gmail.com> All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list 
 * of conditions and the following disclaimer. Redistributions in binary form must 
 * reproduce the above copyright notice, this list of conditions and the following 
 * disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include "util.h"

int utf16_to_utf8(const wchar_t *utf16,size_t is,unsigned char *utfc,size_t os)
{
	const wchar_t *input=utf16;
	unsigned char *output=utfc;
 
	if (is==0) {
		return 0;
	}
 
	do {
		if (os==0) {
			return -1;
		}
		if ((unsigned)*input < 128) {
			*output++ = (unsigned char)*input++;
			--is;
			--os;
		}
		else {
			int c=(unsigned)*input++;
			--is;
 
			unsigned char hi = (unsigned char)(c >> 8);
			unsigned char low = (unsigned char)(c&0xff);
			if (hi>=0x20 && hi<=0x9f && hi!=0x3f) {
				if (os<2) {
					return -1;
				}
				*output++ = (unsigned char)(hi + 0x60);
				*output++ = low;
				os-=2;
			}
			else {
				if (os<3) {
					return -1;
				}
				*output++ = 0x9f;
				*output++ = hi;
				*output++ = low;
				os-=3;
			}
		}
	} while (is!=0);
 
	return output-utfc;
}

int utf8_to_utf16(const unsigned char *utfc,size_t is,wchar_t *utf16,size_t os)
{
	const unsigned char *input=utfc;
	wchar_t *output=utf16;
 
	if (is==0) {
		return 0;
	}
 
	do {
		if (os==0) {
			return -1;
		}
		if (*input < 128) {
			*output++ = (wchar_t)*input++;
			--is;
			--os;
		}
		else {
			int c;
			if (*input==0x9f) {
				if (is<3) {
					c=0xffff;
					is=0;
				}
				else {
					c=input[1]<<8 | input[2];
					is-=3;
					input+=3;
				}
			}
			else {
				if (is<2) {
					c=0xffff;
					is=0;
				}
				else {
					c=(input[0] - 0x60)<<8 | input[1];
					is-=2;
					input+=2;
				}
			}
			*output++ =(wchar_t)c;
		}
	} while (is!=0);
 
	return output-utf16;
}

