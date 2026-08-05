// メガCD版シルフィード ムービープレイヤー「見るフィード」 for X68000
// Sega CD Version Silpheed Movie Player "Milpheed" for X68000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <iocslib.h>
#include <doslib.h>
#include "pcm8a.h"

#define CRTC_BASE (0xE80000)
#define CRTC_R20 (CRTC_BASE+0x28)
volatile short *crtc_r20 = (short*)CRTC_R20;

#define VIDCON_BASE	(0xE82000)
#define GRAPHICS_PAL	(VIDCON_BASE)	/* graphics layers*/
#define TEXT_PAL	(VIDCON_BASE+0x200)	/* text, sprite, bg*/
#define VIDCON_R0	(VIDCON_BASE+0x400)	
#define VIDCON_R1	(VIDCON_BASE+0x500)	
#define VIDCON_R2	(VIDCON_BASE+0x600)	

short *vidcon_r0 = (short*)VIDCON_R0;	/* screen mode*/
short *vidcon_r1 = (short*)VIDCON_R1;	/* priority control*/
short *vidcon_r2 = (short*)VIDCON_R2;	/* on/off control / special priority*/

short *gpal = (short*)GRAPHICS_PAL;

#define GVRAM_BASE (0xC00000)
#define GVRAM0 (GVRAM_BASE)
#define GVRAM1 (GVRAM_BASE+0x80000)
#define GVRAM2 (GVRAM_BASE+0x100000)
#define GVRAM3 (GVRAM_BASE+0x180000)
unsigned short *gvram[2] = {(short*)GVRAM0, (short*)GVRAM1};

static short wait_count = 0;
static short page;
static int crt_mode;
static int ssp;


// Convert RF5C68 signed 8bit PCM -> standard signed 8bit table
signed char rf5c68_to_pcm[256] = {
	0x00,0xFF,0xFE,0xFD,0xFC,0xFB,0xFA,0xF9,
	0xF8,0xF7,0xF6,0xF5,0xF4,0xF3,0xF2,0xF1,
	0xF0,0xEF,0xEE,0xED,0xEC,0xEB,0xEA,0xE9,
	0xE8,0xE7,0xE6,0xE5,0xE4,0xE3,0xE2,0xE1,
	0xE0,0xDF,0xDE,0xDD,0xDC,0xDB,0xDA,0xD9,
	0xD8,0xD7,0xD6,0xD5,0xD4,0xD3,0xD2,0xD1,
	0xD0,0xCF,0xCE,0xCD,0xCC,0xCB,0xCA,0xC9,
	0xC8,0xC7,0xC6,0xC5,0xC4,0xC3,0xC2,0xC1,
	0xC0,0xBF,0xBE,0xBD,0xBC,0xBB,0xBA,0xB9,
	0xB8,0xB7,0xB6,0xB5,0xB4,0xB3,0xB2,0xB1,
	0xB0,0xAF,0xAE,0xAD,0xAC,0xAB,0xAA,0xA9,
	0xA8,0xA7,0xA6,0xA5,0xA4,0xA3,0xA2,0xA1,
	0xA0,0x9F,0x9E,0x9D,0x9C,0x9B,0x9A,0x99,
	0x98,0x97,0x96,0x95,0x94,0x93,0x92,0x91,
	0x90,0x8F,0x8E,0x8D,0x8C,0x8B,0x8A,0x89,
	0x88,0x87,0x86,0x85,0x84,0x83,0x82,0x81,

	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
	0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
	0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
	0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
	0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F
};


// Video Format
//  0: 8*8 Uncompressed	7.5 fps	(Files: A00,A09)
//  1: 8*8 Compressed	15 fps 	(Files: A01-A08,A10,A11,and A20)
//  2: 4*4 Compressed	15 fps 	(Files: Other A##)
// -1: None			(file : A14)
int file_format[25] = {
// 	A00 A01 A02 A03 A04 A05 A06 A07 A08 A09 A10 A11 A12
	  0,  1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  2,
//	A13 A14 A15 A16 A17 A18 A19 A20 A21 A22 A23 A24
	  2, -1,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2 };



FILE *fp;
int file_pointer;
unsigned char *read_buffer;

short name_table[768];
unsigned short pattern_table[1024][64];
int video_format;

struct pcm_chain {
  void *start_address;
  unsigned short pcm_length;
  void *next_address;
};

struct pcm_chain pcm_ring_buffer[8];

short initialize_PCM_flag;
signed char *pcm_buffer;
unsigned short pcm_buffer_offset;
short pcm_volume;


__attribute__((interrupt)) static void Timer_D_Function(void)
{
	wait_count++;
}


void draw()
{
	short i, j, k, y;
	unsigned short *p;
	unsigned short *c;

	k = 0;

	for(i = 4; i < 28 ; i++)
	{
		for(j = 0; j < 32; j++)
		{
			p = &pattern_table[name_table[k]][0];
			c = gvram[page] + (j * 8) + (i * 512 * 8);

			for(y = 0; y < 8 ;y++)
			{
				c[0] = *p++;
				c[1] = *p++;
				c[2] = *p++;
				c[3] = *p++;
				c[4] = *p++;
				c[5] = *p++;
				c[6] = *p++;
				c[7] = *p++;
				c += 512;
			}

			k++;
		}
	}

	*vidcon_r2 = page + 1;

	page++;
	page &= 1;
}

void pattern_init()
{
	short i, j;

	for(i = 0;i < 16;i++)
	{
		for(j = 0;j < 64;j++)
		{
			pattern_table[i][j] = i;
		}
	}
}


int pcm_buffer_init()
{
	pcm_buffer = (unsigned char *)MALLOC(65536);

	if(0x81000000 == ((unsigned int)pcm_buffer & 0x81000000) || 0x82000000 == ((unsigned int)pcm_buffer & 0x82000000))
	{
		return -1;
	}

	pcm_ring_buffer[0].start_address = pcm_buffer;
	pcm_ring_buffer[0].pcm_length = 8192;
	pcm_ring_buffer[0].next_address = &pcm_ring_buffer[1];
	pcm_ring_buffer[1].start_address = pcm_buffer + 8192;
	pcm_ring_buffer[1].pcm_length = 8192;
	pcm_ring_buffer[1].next_address = &pcm_ring_buffer[2];
	pcm_ring_buffer[2].start_address = pcm_buffer + 16384;
	pcm_ring_buffer[2].pcm_length = 8192;
	pcm_ring_buffer[2].next_address = &pcm_ring_buffer[3];
	pcm_ring_buffer[3].start_address = pcm_buffer + 24576;
	pcm_ring_buffer[3].pcm_length = 8192;
	pcm_ring_buffer[3].next_address = &pcm_ring_buffer[4];
	pcm_ring_buffer[4].start_address = pcm_buffer + 32768;
	pcm_ring_buffer[4].pcm_length = 8192;
	pcm_ring_buffer[4].next_address = &pcm_ring_buffer[5];
	pcm_ring_buffer[5].start_address = pcm_buffer + 40960;
	pcm_ring_buffer[5].pcm_length = 8192;
	pcm_ring_buffer[5].next_address = &pcm_ring_buffer[6];
	pcm_ring_buffer[6].start_address = pcm_buffer + 49152;
	pcm_ring_buffer[6].pcm_length = 8192;
	pcm_ring_buffer[6].next_address = &pcm_ring_buffer[7];
	pcm_ring_buffer[7].start_address = pcm_buffer + 57344;
	pcm_ring_buffer[7].pcm_length = 8192;
	pcm_ring_buffer[7].next_address = &pcm_ring_buffer[0];

	return 0;
}


void decode1()
{
	int i, j;
	unsigned char k;
	int header_offset;
	int namedata_offset;
	int pattern_offset;
	int pcm_offset;
	unsigned char msb, lsb;

	// Header
	header_offset = 4;
	k = read_buffer[2];
	if (k & 0x80)
	{
		i = 1;
		do
		{
			msb = read_buffer[header_offset++];
			lsb = read_buffer[header_offset++];

			unsigned short green = (lsb & 0xf0) << 8;
			unsigned short red = (lsb & 0x0f) << 7;
			unsigned short blue = (msb & 0x0f) << 2;

			gpal[i] = green + red + blue + 1;
		} while(++i < 16);
	}
	if (k & 0x40)
	{
		header_offset += 378;
	}
	if (k & 0x20)
	{
		header_offset += 48;
	}
	if (k & 0x10)
	{
		pcm_offset =	((read_buffer[header_offset + 1] + 1) << 8) +
				(read_buffer[header_offset + 6] << 8) +
				read_buffer[header_offset + 7] + 0x7ff;
		pcm_offset &= 0xfffff800;
		header_offset += 8;
	}
	else
	{
		pcm_offset = 0;
	}

	// Name Table
	namedata_offset = header_offset + 96;
	j = ((k << 8) + read_buffer[3]) & 0x0fff;
	i = 0;
	do
	{
		k = read_buffer[header_offset + i];
		short *name_ptr = &name_table[i * 8];
		name_ptr[0] = (k & 0x80) ? read_buffer[namedata_offset++] : j++;
		name_ptr[1] = (k & 0x40) ? read_buffer[namedata_offset++] : j++;
		name_ptr[2] = (k & 0x20) ? read_buffer[namedata_offset++] : j++;
		name_ptr[3] = (k & 0x10) ? read_buffer[namedata_offset++] : j++;
		name_ptr[4] = (k & 0x8) ? read_buffer[namedata_offset++] : j++;
		name_ptr[5] = (k & 0x4) ? read_buffer[namedata_offset++] : j++;
		name_ptr[6] = (k & 0x2) ? read_buffer[namedata_offset++] : j++;
		name_ptr[7] = (k & 0x1) ? read_buffer[namedata_offset++] : j++;
	} while(++i < 96);
	namedata_offset = (namedata_offset + 1) & ~1;

	// Pattern
	pattern_offset = namedata_offset + 31;
	pattern_offset &= 0xffffffe0;
	int last_offset = read_buffer[1] * 2048;
	i = 0;
	while (pattern_offset < last_offset)
	{
		// 8*8 uncompressed
		for(j = 0;j < 64; j+=2)
		{
			k = read_buffer[pattern_offset];
			pattern_table[i][j] = (k >> 4);
			pattern_table[i][j + 1] = (k & 0xf);
			pattern_offset++;
		}
		i++;
	}

	// PCM
	if(pcm_offset)
	{
		// i have no idea how to play the audio.
		/*if(initialize_PCM_flag == 1)
		{
			for(i = pattern_offset;i < pcm_offset;i++)
			{

			}
		}*/
		file_pointer += pcm_offset;
	}
	else
	{
		file_pointer += pattern_offset;
	}
}


void decode2()
{
	int i, j, k;
	unsigned char l;
	int header_offset;
	int namedata_offset;
	int pattern_offset;
	int name_number;
	int pcm_offset;
	unsigned char msb, lsb;
	int offset = 0;

	// Header
	header_offset = 4;
	l = read_buffer[2];
	if (l & 0x80)
	{
		i = 1;
		do
		{
			msb = read_buffer[header_offset++];
			lsb = read_buffer[header_offset++];

			unsigned short green = (lsb & 0xf0) << 8;
			unsigned short red = (lsb & 0x0f) << 7;
			unsigned short blue = (msb & 0x0f) << 2;

			gpal[i] = green + red + blue + 1;
		} while(++i < 16);
	}
	if (l & 0x40)
	{
		header_offset += 378;
	}
	if (l & 0x20)
	{
		header_offset += 48;
	}
	if (l & 0x10)
	{
		pcm_offset =	((read_buffer[header_offset + 1] + 1) << 8) +
				(read_buffer[header_offset + 6] << 8) +
				read_buffer[header_offset + 7] + 0x7ff;
		pcm_offset &= 0xfffff800;
		header_offset += 8;
	}
	else
	{
		pcm_offset = 0;
	}

	// Name Table
	namedata_offset = header_offset + 96;
	j = ((l << 8) + read_buffer[3]) & 0x0fff;
	i = 0;
	do
	{
		l = read_buffer[header_offset + i];
		short *name_ptr = &name_table[i * 8];
		name_ptr[0] = (l & 0x80) ? read_buffer[namedata_offset++] : j++;
		name_ptr[1] = (l & 0x40) ? read_buffer[namedata_offset++] : j++;
		name_ptr[2] = (l & 0x20) ? read_buffer[namedata_offset++] : j++;
		name_ptr[3] = (l & 0x10) ? read_buffer[namedata_offset++] : j++;
		name_ptr[4] = (l & 0x8) ? read_buffer[namedata_offset++] : j++;
		name_ptr[5] = (l & 0x4) ? read_buffer[namedata_offset++] : j++;
		name_ptr[6] = (l & 0x2) ? read_buffer[namedata_offset++] : j++;
		name_ptr[7] = (l & 0x1) ? read_buffer[namedata_offset++] : j++;
	} while(++i < 96);
	namedata_offset = (namedata_offset + 1) & ~1;

	// Pattern
	pattern_offset = (read_buffer[namedata_offset++] << 8);
	pattern_offset += read_buffer[namedata_offset++];
	name_number = 16;
	int loop = read_buffer[1];
	int loop2;
	for (i=0; i<loop; i++)
	{
		loop2 =  (read_buffer[pattern_offset++] << 8);
		loop2 += read_buffer[pattern_offset++];
		for (j=0; j<loop2; j++)
		{
			l = read_buffer[namedata_offset++];
			if (!l)
			{
				// 8*8 uncompressed
				for(k = 0;k < 64; k += 2)
				{
					l = read_buffer[pattern_offset];
					pattern_table[name_number][k] = (l >> 4);
					pattern_table[name_number][k + 1] = (l & 0xf);
					pattern_offset++;
				}
			}
			else
			{
				// 8*8 2 colors (The Sega CD version used the "Font Bit" function.)
				msb = (l & 0xf0) >> 4;
				lsb = (l & 0x0f);
				for(k = 0;k < 64; k += 8)
				{
					l = read_buffer[pattern_offset++];
					unsigned short *ptr = &pattern_table[name_number][k];
					ptr[0] = ((l & 0x80) ? msb : lsb);
					ptr[1] = ((l & 0x40) ? msb : lsb);
					ptr[2] = ((l & 0x20) ? msb : lsb);
					ptr[3] = ((l & 0x10) ? msb : lsb);
					ptr[4] = ((l & 0x8) ? msb : lsb);
					ptr[5] = ((l & 0x4) ? msb : lsb);
					ptr[6] = ((l & 0x2) ? msb : lsb);
					ptr[7] = ((l & 0x1) ? msb : lsb);
				}
			}
			name_number++;
		}
		offset += 0x800;
		pattern_offset = offset;
	}

	// PCM
	if(pcm_offset)
	{
		// i have no idea how to play the audio.
		/*if(initialize_PCM_flag == 1)
		{
			for(i = pattern_offset;i < pcm_offset;i++)
			{

			}
		}*/
		file_pointer += pcm_offset;
	}
	else
	{
		file_pointer += pattern_offset;
	}
}


void decode3()
{
	int i, j, k, l;
	unsigned char m;
	int header_offset;
	int namedata_offset;
	int pattern_offset;
	int name_number;
	unsigned char msb, lsb;
	int font_bit;
	int coord;
	int pcm_sector;
	int offset = 0;

	// Header
	header_offset = 4;
	m = read_buffer[2];
	if (m & 0x80)
	{
		i = 1;
		do
		{
			msb = read_buffer[header_offset++];
			lsb = read_buffer[header_offset++];

			unsigned short green = (lsb & 0xf0) << 8;
			unsigned short red = (lsb & 0x0f) << 7;
			unsigned short blue = (msb & 0x0f) << 2;

			gpal[i] = green + red + blue + 1;
		} while(++i < 16);
	}
	if (m & 0x40)
	{
		header_offset += 378;
	}
	if (m & 0x20)
	{
		header_offset += 48;
	}
	if (m & 0x10)
	{
		header_offset += 8;
	}

	// Name Table
	namedata_offset = header_offset + 96;
	j = ((m << 8) + read_buffer[3]) & 0x0fff;
	i = 0;
	do
	{
		m = read_buffer[header_offset + i];
		short *name_ptr = &name_table[i * 8];
		name_ptr[0] = (m & 0x80) ? read_buffer[namedata_offset++] : j++;
		name_ptr[1] = (m & 0x40) ? read_buffer[namedata_offset++] : j++;
		name_ptr[2] = (m & 0x20) ? read_buffer[namedata_offset++] : j++;
		name_ptr[3] = (m & 0x10) ? read_buffer[namedata_offset++] : j++;
		name_ptr[4] = (m & 0x8) ? read_buffer[namedata_offset++] : j++;
		name_ptr[5] = (m & 0x4) ? read_buffer[namedata_offset++] : j++;
		name_ptr[6] = (m & 0x2) ? read_buffer[namedata_offset++] : j++;
		name_ptr[7] = (m & 0x1) ? read_buffer[namedata_offset++] : j++;
	} while(++i < 96);
	namedata_offset = (namedata_offset + 1) & ~1;

	// Pattern
	pattern_offset = namedata_offset;
	name_number = 0;
	int loop = read_buffer[1];
	int loop2;
	for (i=0; i<loop; i++) {
		loop2 =  (read_buffer[pattern_offset++] << 8);
		loop2 += read_buffer[pattern_offset++];
		for (j=0; j<loop2; j++)
		{
			font_bit = pattern_offset;
			pattern_offset += 4;

			k = 0;
			do
			{
				m = read_buffer[font_bit++];
				coord = ((k & 2) << 4) + ((k & 1) << 2);
				if(m == 1) // 4*4 uncompressed
				{
					l = 0;
					do
					{
						unsigned short *ptr = &pattern_table[name_number][coord];
						m = read_buffer[pattern_offset++];
						ptr[0] = (m >> 4) & 0xf;
						ptr[1] = m & 0xf;
						m = read_buffer[pattern_offset++];
						ptr[2] = (m >> 4) & 0xf;
						ptr[3] = m & 0xf;
						coord += 8;
					} while(++l < 4);
				}
				else
				{
					msb = (m & 0xf0) >> 4;
					lsb = (m & 0x0f);

					if (lsb == msb) // 4*4 monochrome
					{
						unsigned short *ptr = &pattern_table[name_number][coord];
						m &= 0xf;
						ptr[0] = m;
						ptr[1] = m;
						ptr[2] = m;
						ptr[3] = m;
						ptr[8] = m;
						ptr[9] = m;
						ptr[10] = m;
						ptr[11] = m;
						ptr[16] = m;
						ptr[17] = m;
						ptr[18] = m;
						ptr[19] = m;
						ptr[24] = m;
						ptr[25] = m;
						ptr[26] = m;
						ptr[27] = m;
					}
					else // 4*4 2 colors (The Sega CD version used the "Font Bit" function.)
					{

						unsigned short *ptr = &pattern_table[name_number][coord];
						m = read_buffer[pattern_offset++];
						ptr[0] = (m & 0x80) ? msb : lsb;
						ptr[1] = (m & 0x40) ? msb : lsb;
						ptr[2] = (m & 0x20) ? msb : lsb;
						ptr[3] = (m & 0x10) ? msb : lsb;
						ptr[8] = (m & 0x8) ? msb : lsb;
						ptr[9] = (m & 0x4) ? msb : lsb;
						ptr[10] = (m & 0x2) ? msb : lsb;
						ptr[11] = (m & 0x1) ? msb : lsb;
						m = read_buffer[pattern_offset++];
						ptr[16] = (m & 0x80) ? msb : lsb;
						ptr[17] = (m & 0x40) ? msb : lsb;
						ptr[18] = (m & 0x20) ? msb : lsb;
						ptr[19] = (m & 0x10) ? msb : lsb;
						ptr[24] = (m & 0x8) ? msb : lsb;
						ptr[25] = (m & 0x4) ? msb : lsb;
						ptr[26] = (m & 0x2) ? msb : lsb;
						ptr[27] = (m & 0x1) ? msb : lsb;
					}
				}
			} while(++k < 4);
		    name_number++;
		}
		file_pointer += 0x800;
		offset += 0x800;
		pattern_offset = offset;
	}

	// PCM
	pcm_sector = read_buffer[0];
	if( (pcm_sector != 0) && (initialize_PCM_flag == 1) )
	{
		unsigned char *ptr = &read_buffer[offset];
		unsigned short sum;
		i = 0;
		j = 0;
		k = 0;
		l = pcm_sector * 1024;
		do
		{
			m = ptr[k + j];
			if(m == 0xff)
			{
				break;
			}
			sum = rf5c68_to_pcm[m];
			sum += rf5c68_to_pcm[ptr[k + j + 1024]];
			sum /= 2;
			pcm_buffer[pcm_buffer_offset] = sum;
			k++;
			pcm_buffer_offset++;
			pcm_buffer_offset&=0xffff;
			if(k == 1024)
			{
				k = 0;
				j += 2048;
			}
		} while(++i < l);
	}

	file_pointer += pcm_sector * 2048;
}


int main(int argc, char *argv[])
{
	int i;
	unsigned char key;
	int change_format_frame;
	char filename[256];
	int filesize;
	fpos_t fsize;

	printf("\nメガCD版シルフィード ムービープレイヤー「見るフィード」 for X68000\n");

	pcm_volume = 72;
	video_format = -1;
	change_format_frame = 0;

	for(i = 1;i < argc;i++)
	{
		if ( (strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "-H") == 0) )
		{
			printf("\n使い方: milpheed 動画ファイル名\n[オプションコマンド] -c 動画アルゴリズム選択(0～2) -v 音量(0～72)\n");
			return 0;
		}
		else if( (strcmp(argv[i], "-c") == 0) || (strcmp(argv[i], "-C") == 0) )
		{
			if(i + 1 < argc)
			{
				video_format = atoi(argv[++i]);
			}
		}
		else if( (strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "-V") == 0) )
		{
			if(i + 1 < argc)
			{
				pcm_volume = atoi(argv[++i]);
			}
			if(pcm_volume <= 0)
			{
				pcm_volume = 0;
			}
		}
		else
		{
			strcpy(filename, argv[i]);
		}

	}

	if(pcm_volume > 72)
	{
		pcm_volume = 72;
	}

	int len;
	len = strlen(filename);

	if(len == 0)
	{
		printf("ファイル名が指定されていません\n");
		printf("\n使い方: milpheed 動画ファイル名\n[オプションコマンド] -c 動画アルゴリズム選択(0～2) -v 音量(0～72)\n");
		return -1;
	}

	i = len - 1;
	while (i >= 0 && isdigit(filename[i])) {
		i--;
	}

	if(video_format == -1)
	{
		int num;
		if((filename[i] == 'A') || (filename[i] == 'a'))
		{
			num = atoi(&filename[i + 1]);
		}
		if( (num >= 0) && (num <= 24) )
		{
			if(num == 0)
			{
				change_format_frame = 225;
			}
			else if(num == 9)
			{
				change_format_frame = 900;
			}

			video_format = file_format[num];
		}
	}

	if(video_format == -1)
	{
		printf("シルフィードデフォルト以外の動画ファイルのようです\n");
		printf("動画アルゴリズムが指定されていません\n");
		printf("\n使い方: milpheed 動画ファイル名\n[オプションコマンド] -c 動画アルゴリズム選択(0～2) -v 音量(0～72)\n");
		return -1;
	}

	fp = fopen( filename, "rb" );
	if(fp == NULL)
	{
		printf("ファイルが開けませんでした\n");
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	fgetpos(fp, &fsize);
	filesize = fsize;
	fseek(fp, 0, SEEK_SET);

	read_buffer = (unsigned char *)MALLOC(32768);

	i = TIMERDST((unsigned char *)Timer_D_Function, 7, 166);
	if(i != 0)
	{
		printf("Dタイマーの初期化に失敗しました\n");
		fclose(fp);
		MFREE((unsigned int)read_buffer);
		return 1;
	}

	file_pointer = 0;

	if(pcm_volume != 0)
	{
		if(pcm8a_check() < 0)
		{
			printf("PCM8Aが常駐されていません。\n");
			initialize_PCM_flag = 0;
		}
		else
		{
			if(pcm_buffer_init() == -1)
			{
				printf("PCMバッファの確保に失敗しました。\n");
				initialize_PCM_flag = 0;
			}
			else
			{
				initialize_PCM_flag = 1;
			}
		}
	}
	else
	{
		initialize_PCM_flag = 0;
	}

	SKEY_MOD(0, 0, 0);
	C_CUROFF();
	ssp = B_SUPER(0);
	crt_mode = CRTMOD(-1);
	CRTMOD(6);
	G_CLR_ON();
	page = 0;

	pattern_init();

	if(video_format == 2)
	{
		file_pointer = 0x1000;
		if(initialize_PCM_flag == 1)
		{
			fread(read_buffer, 1, 4096, fp);

#define PCM_BUFFER_START 16384
			memset(&pcm_buffer[0], 0, PCM_BUFFER_START);

			short sum;
			for(i = 0;i < 2;i++)
			{
				for(int j = 0;j < 1024;j++)
				{
					sum = rf5c68_to_pcm[read_buffer[j + (i * 2048)]];
					sum += rf5c68_to_pcm[read_buffer[j + (i * 2048)] + 1024];
					sum /= 2;
					pcm_buffer[PCM_BUFFER_START + j + (i * 1024)] = sum;
				}
			}
			i =(pcm_volume << 16);
			pcm8a_play_link_ally_chain(0, 0x400603 + i, &pcm_ring_buffer[0]);

			pcm_buffer_offset = PCM_BUFFER_START + 2048;
		}
	}
	else
	{
		file_pointer = 0;
	}

	int frame = 0;
	int wait_flag;
	wait_count = 0;

	while((file_pointer < filesize))
	{
		fseek(fp, file_pointer, SEEK_SET);

		frame++;

		switch(video_format)
		{
			case 1:
				fread(read_buffer, 1, 16384, fp);
				decode2();
				wait_flag = 8;
				break;
			case 2:
				fread(read_buffer, 1, 16384, fp);
				decode3();
				wait_flag = 8;
				break;
			default:
				fread(read_buffer, 1, 20480, fp);
				decode1();
				wait_flag = 16;
				if(frame == change_format_frame)
				{
					video_format = 1;
				}
				break;
		}

		draw();

		// Timer wait
		while(wait_count < wait_flag)
		{
			key = (BITSNS(0) & 0x2) >> 1;
		}
		wait_count = 0;
		if(key == 1)
		{
			break;
		}
	}

	if(initialize_PCM_flag == 1)
	{
		pcm8a_stop();
		MFREE((unsigned int)pcm_buffer);
	}

	CRTMOD(crt_mode);
	C_CURON();
	SKEY_MOD(-1, 0, 0);

	TIMERDST((unsigned char *)0, 0, 0);

	MFREE((unsigned int)read_buffer);

	__asm__ volatile (	"subq.l #8,sp\n"
				"move.l sp,usp\n"
				"addq.l #4,sp\n"
				"move.l %0,(sp)\n"
				"jsr (%1)\n"
				"addq.l #4,sp" :: "d"(ssp), "a"(B_SUPER));

	return 0;
}