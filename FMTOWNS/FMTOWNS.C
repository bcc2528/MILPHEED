// メガCD版シルフィード ムービープレイヤー「見るフィード」 for FM TOWNS
// Sega CD Version Silpheed Movie Player "Milpheed" for FM TOWNS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <egb.h>
#include <snd.h>
#include <dos.h>
#include <his.h>


// The PCM format for the Sega CD(Mega CD) version of Silpheed is sign 8-bit, 16.272 Hz(Unconfirmed), stereo.
// FM TOWNS and Sega CD use the same PCM chip, but their frequencies differ.(TOWNS is 8.0 MHz / 384 = 20.833 KHz, Sega CD is 12.5 MHz / 384 = 32.552 KHz)
// When tested using a ring buffer method, the audio played back normally at this(0x620 = 16 KHz) rate on FM TOWNS.
#define PCM_FREQ 0x629

// Video Format
//  0: 8*8 Uncompressed	7.5 fps	(Files: A00,A09)
//  1: 8*8 Compressed	15 fps	(Files: A01-A08,A10,A11,and A20)
//  2: 4*4 Compressed	15 fps	(Files: Other A##)
// -1: None			(file : A14)
int file_format[25] = {
// 	A00 A01 A02 A03 A04 A05 A06 A07 A08 A09 A10 A11 A12
	  0,  1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  2,
//	A13 A14 A15 A16 A17 A18 A19 A20 A21 A22 A23 A24
	  2, -1,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2 };


#define stackSize 1000
char EGB_stack[ stackSize ];
#define VSYNCclear 0x05ca
#define VSYNCintNumber 11
int VsyncCount;

char *egb_work;
char *snd_work;

FILE *fp;
int file_pointer;
unsigned char *read_buffer;

int *name_table;
unsigned char font_bit_table[1024];
unsigned char pattern_table[1024][32];
int video_format;

int initialize_PCM_flag;
char *pcm_left_buffer;
char *pcm_right_buffer;
int pcm_buffer_offset;
int pcm_volume;

void VSYNChandler( void )
{
	VsyncCount++;

	/******** ＶＳＹＮＣ割り込み原因クリアレジスタへの書き込み ********/
	_outb( VSYNCclear, 0 );
}


void Write_CRTC_register(int address, int data)
{
	_outb( 0x0440, address );
	_outw( 0x0442, data );
}


void draw()
{
	_Far unsigned int *vram;
	_FP_SEG(vram) = 0x120;
	int i, j, k;
	unsigned int *p;

	k = 0;
	i = 3;
	do
	{
		_FP_OFF(vram) = (i * 512 * 8);
		j = 0;
		do
		{
			p = (unsigned int *)&pattern_table[name_table[k]][0];

			vram[0] = p[0];
			_inline(0x8b, 0x41, 0x04, 0x65, 0x89, 0x82, 0x00, 0x02, 0x00, 0x00);
			_inline(0x8b, 0x41, 0x08, 0x65, 0x89, 0x82, 0x00, 0x04, 0x00, 0x00);
			_inline(0x8b, 0x41, 0x0c, 0x65, 0x89, 0x82, 0x00, 0x06, 0x00, 0x00);
			_inline(0x8b, 0x41, 0x10, 0x65, 0x89, 0x82, 0x00, 0x08, 0x00, 0x00);
			_inline(0x8b, 0x41, 0x14, 0x65, 0x89, 0x82, 0x00, 0x0a, 0x00, 0x00);
			_inline(0x8b, 0x41, 0x18, 0x65, 0x89, 0x82, 0x00, 0x0c, 0x00, 0x00);
			_inline(0x8b, 0x41, 0x1c, 0x65, 0x89, 0x82, 0x00, 0x0e, 0x00, 0x00);
			/*vram[128] = p[1];
			vram[256] = p[2];
			vram[384] = p[3];
			vram[512] = p[4];
			vram[640] = p[5];
			vram[768] = p[6];
			vram[896] = p[7];*/

			k++;
			vram++;
		} while(++j < 32);
	} while(++i < 27);
}


void display_init()
{
	egb_work = malloc(1536);
	EGB_init( egb_work, EgbWorkSize );
	EGB_resolution( egb_work, 1, 3 );
	EGB_resolution( egb_work, 0, 3 );
	EGB_displayPage(egb_work, 0, 1);
	Write_CRTC_register(0, 0x50);
	Write_CRTC_register(1, 0x24E);
	Write_CRTC_register(4, 0x29D);
	Write_CRTC_register(9, 0x82);
	Write_CRTC_register(0xA, 0x282);
	Write_CRTC_register(0xB, 0x82);
	Write_CRTC_register(0xC, 0x282);
	Write_CRTC_register(0x12, 0x82);
	Write_CRTC_register(0x16, 0x82);
	Write_CRTC_register(0x1B, 0x1111);
	Write_CRTC_register(0x1D, 3);
}


int pcm_buffer_init()
{
	pcm_left_buffer = (char *)malloc(32 + (64 * 1024));
	pcm_right_buffer = (char *)malloc(32 + (64 * 1024));

	// PCM Data Header
	*(unsigned int *)&pcm_left_buffer[0] = 0; // Sound name
	*(unsigned int *)&pcm_left_buffer[4] = 0; // Sound name
	*(unsigned int *)&pcm_left_buffer[8] = 0x68000; // Sound ID
	*(unsigned int *)&pcm_left_buffer[12] = 65536; // Data length
	*(unsigned int *)&pcm_left_buffer[16] = 0; // Loop start address
	*(unsigned int *)&pcm_left_buffer[20] = 65536; // Loop length
	*(unsigned short *)&pcm_left_buffer[24] = PCM_FREQ; // Frequency
	*(unsigned short *)&pcm_left_buffer[26] = 0; // Frequency correction
	pcm_left_buffer[28] = 0x3c; // Scale
	pcm_left_buffer[29] = 0x0; // Reserve1
	*(unsigned short *)&pcm_left_buffer[30] = 0; // Reserve2

	*(unsigned int *)&pcm_right_buffer[0] = 0;
	*(unsigned int *)&pcm_right_buffer[4] = 0;
	*(unsigned int *)&pcm_right_buffer[8] = 0x80386;
	*(unsigned int *)&pcm_right_buffer[12] = 65536;
	*(unsigned int *)&pcm_right_buffer[16] = 0;
	*(unsigned int *)&pcm_right_buffer[20] = 65536;
	*(unsigned short *)&pcm_right_buffer[24] = PCM_FREQ;
	*(unsigned short *)&pcm_right_buffer[26] = 0;
	pcm_right_buffer[28] = 0x3c;
	pcm_right_buffer[29] = 0x0;
	*(unsigned short *)&pcm_right_buffer[30] = 0;

	return 0;
}


void pattern_init()
{
	memset(pattern_table, 0, sizeof(pattern_table));
	memset(&pattern_table[1][0], 0x11, 32);
	memset(&pattern_table[2][0], 0x22, 32);
	memset(&pattern_table[3][0], 0x33, 32);
	memset(&pattern_table[4][0], 0x44, 32);
	memset(&pattern_table[5][0], 0x55, 32);
	memset(&pattern_table[6][0], 0x66, 32);
	memset(&pattern_table[7][0], 0x77, 32);
	memset(&pattern_table[8][0], 0x88, 32);
	memset(&pattern_table[9][0], 0x99, 32);
	memset(&pattern_table[10][0], 0xaa, 32);
	memset(&pattern_table[11][0], 0xbb, 32);
	memset(&pattern_table[12][0], 0xcc, 32);
	memset(&pattern_table[13][0], 0xdd, 32);
	memset(&pattern_table[14][0], 0xee, 32);
	memset(&pattern_table[15][0], 0xff, 32);
}


void font_bit_table_init()
{
	int i, j;
	unsigned char k, msb, lsb;

	i = 0;
	do
	{
	
		j = 0;
		do
		{
			msb = (j & 0xf0) >> 4;
			lsb = (j & 0x0f);
			k = ((i & 0x200) ? msb : lsb);
			k += ((i & 0x100) ? msb : lsb) << 4;
			font_bit_table[i + j] = k;
		} while(++j < 256);
		i += 256;
	} while(i < 1024);
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

	header_offset = 4;
	k = read_buffer[2];
	if (k & 0x80)
	{
		register int r = 1;
		do
		{
			msb = read_buffer[header_offset++];
			lsb = read_buffer[header_offset++];

			_outb(0xfd90, r);
			_outb(0xfd96, lsb & 0xF0);
			_outb(0xfd94, (lsb & 0x0F) << 4);
			_outb(0xfd92, (msb & 0x0F) << 4);
		} while(++r < 16);
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
		pcm_offset &=	0xfffff800;
		header_offset += 8;
	}
	else
	{
		pcm_offset = 0;
	}

	// Name Table
	namedata_offset = header_offset + 96;
	j = ((k << 8) + (read_buffer[3])) & 0x0fff;
	i = 0;
	do
	{
		register unsigned char c = read_buffer[header_offset + i];
		int *name_ptr = &name_table[i * 8];
		name_ptr[0] = (c & 0x80) ? read_buffer[namedata_offset++] : j++;
		name_ptr[1] = (c & 0x40) ? read_buffer[namedata_offset++] : j++;
		name_ptr[2] = (c & 0x20) ? read_buffer[namedata_offset++] : j++;
		name_ptr[3] = (c & 0x10) ? read_buffer[namedata_offset++] : j++;
		name_ptr[4] = (c & 0x8) ? read_buffer[namedata_offset++] : j++;
		name_ptr[5] = (c & 0x4) ? read_buffer[namedata_offset++] : j++;
		name_ptr[6] = (c & 0x2) ? read_buffer[namedata_offset++] : j++;
		name_ptr[7] = (c & 0x1) ? read_buffer[namedata_offset++] : j++;
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
		register unsigned char c;
		j = 0;
		do
		{
			c = read_buffer[pattern_offset];
			_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
			pattern_table[i][j] = c;
			pattern_offset++;
		} while(++j < 32);
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

	header_offset = 4;
	l = read_buffer[2];
	if (l & 0x80)
	{
		register int r = 1;
		do
		{
			msb = read_buffer[header_offset++];
			lsb = read_buffer[header_offset++];

			_outb(0xfd90, r);
			_outb(0xfd96, lsb & 0xF0);
			_outb(0xfd94, (lsb & 0x0F) << 4);
			_outb(0xfd92, (msb & 0x0F) << 4);
		} while(++r < 16);
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
		pcm_offset &=	0xfffff800;
		header_offset += 8;
	}
	else
	{
		pcm_offset = 0;
	}

	// Name Table
	namedata_offset = header_offset + 96;
	j = ((l << 8) + (read_buffer[3])) & 0x0fff;
	i = 0;
	do
	{
		register unsigned char c = read_buffer[header_offset + i];
		int *name_ptr = &name_table[i * 8];
		name_ptr[0] = (c & 0x80) ? read_buffer[namedata_offset++] : j++;
		name_ptr[1] = (c & 0x40) ? read_buffer[namedata_offset++] : j++;
		name_ptr[2] = (c & 0x20) ? read_buffer[namedata_offset++] : j++;
		name_ptr[3] = (c & 0x10) ? read_buffer[namedata_offset++] : j++;
		name_ptr[4] = (c & 0x8) ? read_buffer[namedata_offset++] : j++;
		name_ptr[5] = (c & 0x4) ? read_buffer[namedata_offset++] : j++;
		name_ptr[6] = (c & 0x2) ? read_buffer[namedata_offset++] : j++;
		name_ptr[7] = (c & 0x1) ? read_buffer[namedata_offset++] : j++;
	} while(++i < 96);
	namedata_offset = (namedata_offset + 1) & ~1;

	// Pattern
	pattern_offset = (read_buffer[namedata_offset++] << 8);
	pattern_offset += read_buffer[namedata_offset++];
	name_number = 16;
	int loop = (int)read_buffer[1];
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
				register unsigned char c;
				k = 0;
				do
				{
					c = read_buffer[pattern_offset];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					pattern_table[name_number][k] = c;
					pattern_offset++;
				} while(++k < 32);
			}
			else
			{
				// 8*8 2 colors (The Sega CD version used the "Font Bit" function.)
				unsigned char *font_ptr = &font_bit_table[l];
				k = 0;
				do
				{
					l = read_buffer[pattern_offset++];
					unsigned char *ptr = &pattern_table[name_number][k];
					ptr[0] = font_ptr[((l << 2) & 0x300)];
					ptr[1] = font_ptr[((l << 4) & 0x300)];
					ptr[2] = font_ptr[((l << 6) & 0x300)];
					ptr[3] = font_ptr[((l << 8) & 0x300)];
					k += 4;
				} while(k < 32);
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
	int i, j, k;
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

	header_offset = 4;
	m = read_buffer[2];
	if (m & 0x80)
	{
		register int r = 1;
		do
		{
			msb = read_buffer[header_offset++];
			lsb = read_buffer[header_offset++];

			_outb(0xfd90, r);
			_outb(0xfd96, lsb & 0xf0);
			_outb(0xfd94, (lsb & 0x0f) << 4);
			_outb(0xfd92, (msb & 0x0f) << 4);
		} while(++r < 16);
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
		register unsigned char c = read_buffer[header_offset + i];
		int *name_ptr = &name_table[i * 8];
		name_ptr[0] = (c & 0x80) ? read_buffer[namedata_offset++] : j++;
		name_ptr[1] = (c & 0x40) ? read_buffer[namedata_offset++] : j++;
		name_ptr[2] = (c & 0x20) ? read_buffer[namedata_offset++] : j++;
		name_ptr[3] = (c & 0x10) ? read_buffer[namedata_offset++] : j++;
		name_ptr[4] = (c & 0x8) ? read_buffer[namedata_offset++] : j++;
		name_ptr[5] = (c & 0x4) ? read_buffer[namedata_offset++] : j++;
		name_ptr[6] = (c & 0x2) ? read_buffer[namedata_offset++] : j++;
		name_ptr[7] = (c & 0x1) ? read_buffer[namedata_offset++] : j++;
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
				register unsigned char c;
				c = read_buffer[font_bit++];
				coord = ((k & 2) << 3) + ((k & 1) << 1);
				if(c == 1) // 4*4 uncompressed
				{
					unsigned char *ptr = &pattern_table[name_number][coord];
					unsigned char *read_ptr = &read_buffer[pattern_offset];
					c = read_ptr[0];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[0] = c;
					c = read_ptr[1];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[1] = c;
					c = read_ptr[2];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[4] = c;
					c = read_ptr[3];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[5] = c;
					c = read_ptr[4];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[8] = c;
					c = read_ptr[5];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[9] = c;
					c = read_ptr[6];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[12] = c;
					c = read_ptr[7];
					_inline(0xc0,0xc3,0x04); //rol    bl,0x4 : c = ((c >> 4) | (c << 4));
					ptr[13] = c;
					pattern_offset += 8;
				}
				else
				{
					msb = (c & 0xf0) >> 4;
					lsb = (c & 0x0f);

					if (lsb == msb) // 4*4 monochrome
					{
						unsigned char *ptr = &pattern_table[name_number][coord];
						_fill_char(ptr, 2, c);
						_inline(0x66, 0x89, 0x5e, 0x04, 0x66, 0x89, 0x5e, 0x08, 0x66, 0x89, 0x5e, 0x0c);
						/*_fill_char(ptr + 4, 2, c);
						_fill_char(ptr + 8, 2, c);
						_fill_char(ptr + 12, 2, c);*/
					}
					else // 4*4 2 colors (The Sega CD version used the "Font Bit" function.)
					{
						unsigned char *ptr = &pattern_table[name_number][coord];
						unsigned char *font_ptr = &font_bit_table[c];
						m = read_buffer[pattern_offset++];
						ptr[0] = font_ptr[((m << 2) & 0x300)];
						ptr[1] = font_ptr[((m << 4) & 0x300)];
						ptr[4] = font_ptr[((m << 6) & 0x300)];
						ptr[5] = font_ptr[((m << 8) & 0x300)];
						m = read_buffer[pattern_offset++];
						ptr[8] = font_ptr[((m << 2) & 0x300)];
						ptr[9] = font_ptr[((m << 4) & 0x300)];
						ptr[12] = font_ptr[((m << 6) & 0x300)];
						ptr[13] = font_ptr[((m << 8) & 0x300)];
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
		register unsigned char c;
		i = pcm_sector * 1024;
		j = 0;
		k = 0;
		do
		{
			c = ptr[k + j];
			if(c == 0xff)
			{
				break;
			}
			pcm_left_buffer[32 + pcm_buffer_offset] = c;
			pcm_right_buffer[32 + pcm_buffer_offset] = ptr[k + j + 1024];
			k++;
			pcm_buffer_offset++;
			pcm_buffer_offset&=0xffff;
			if(k == 1024)
			{
				k = 0;
				j += 2048;
			}
		} while(--i);
	}

	file_pointer += pcm_sector * 2048;
}


int main(int argc, char *argv[])
{
	int i;
	int change_format_frame;
	char filename[256];
	int filesize;
	fpos_t fsize;

	printf("\nメガCD版シルフィード ムービープレイヤー「見るフィード」 for FM TOWNS\n");

	pcm_volume = 112;
	video_format = -1;
	change_format_frame = 0;

	for(i = 1;i < argc;i++)
	{
		if ( (strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "-H") == 0) )
		{
			printf("\n使い方: run386 milpheed 動画ファイル名\n[オプションコマンド] -c 動画アルゴリズム選択(0～2) -v 音量(0～127)\n");
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

	if(pcm_volume > 127)
	{
		pcm_volume = 127;
	}

	int len;
	len = strlen(filename);

	if(len == 0)
	{
		printf("ファイル名が指定されていません\n");
		printf("\n使い方: run386 milpheed 動画ファイル名\n[オプションコマンド] -c 動画アルゴリズム選択(0～2) -v 音量(0～127)\n");
		return 1;
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
		printf("\n使い方: run386 milpheed 動画ファイル名\n[オプションコマンド] -c 動画アルゴリズム選択(0～2) -v 音量(0～127)\n");
		return 1;
	}

	fp = fopen( filename, "rb" );
	if(fp == NULL)
	{
		printf("ファイルが開けませんでした\n");
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	fgetpos(fp, &fsize);
	filesize = fsize;
	fseek(fp, 0, SEEK_SET);

	read_buffer = (unsigned char *)malloc(32768);

	display_init();

	if(pcm_volume != 0)
	{
		snd_work = malloc(16384);
		SND_init(snd_work);
		SND_elevol_mute(0x01) ;
		SND_pcm_mode_set(2);
		SND_pan_set(71, 0);
		SND_pan_set(70, 127);

		pcm_buffer_init();
		initialize_PCM_flag = 1;
	}
	else
	{
		initialize_PCM_flag = 0;
	}

	name_table = (int *)malloc(32 * 24 * sizeof(int));
	pattern_init();
	font_bit_table_init();

	HIS_stackArea( EGB_stack , stackSize );
	HIS_setHandler( VSYNCintNumber , VSYNChandler );
	HIS_enableInterrupt( VSYNCintNumber );

	if(video_format == 2)
	{
		file_pointer = 0x1000;

		if(initialize_PCM_flag == 1)
		{
			fread(read_buffer, 1, 4096, fp);

#define PCM_BUFFER_START 16384
			memset(&pcm_left_buffer[32], 0, PCM_BUFFER_START);
			memset(&pcm_right_buffer[32], 0, PCM_BUFFER_START);
			memcpy(&pcm_left_buffer[32 + PCM_BUFFER_START], &read_buffer[0], 1024);
			memcpy(&pcm_right_buffer[32 + PCM_BUFFER_START], &read_buffer[1024], 1024);
			memcpy(&pcm_left_buffer[32 + PCM_BUFFER_START + 1024], &read_buffer[2048], 1024);
			memcpy(&pcm_right_buffer[32 + PCM_BUFFER_START + 1024], &read_buffer[3072], 1024);

			SND_pcm_play( 71, 0x3c, pcm_volume, pcm_left_buffer );
			SND_pcm_play( 70, 0x3c, pcm_volume, pcm_right_buffer );

			pcm_buffer_offset = PCM_BUFFER_START + 2048;
		}
	}
	else
	{
		file_pointer = 0;
	}

	int frame = 0;
	int wait_flag;
	VsyncCount = 0;

	while((file_pointer < filesize) && (_kbhit() == 0))
	{
		fseek(fp, file_pointer, SEEK_SET);

		frame++;

		switch(video_format)
		{
			case 1:
				fread(read_buffer, 1, 16384, fp);
				decode2();
				wait_flag = 4;
				break;
			case 2:
				fread(read_buffer, 1, 16384, fp);
				decode3();
				wait_flag = 4;
				break;
			default:
				fread(read_buffer, 1, 20480, fp);
				decode1();
				wait_flag = 8;
				if(frame == change_format_frame)
				{
					video_format = 1;
				}
				break;
		}

		draw();

		// Vsync wait
		do{} while(VsyncCount < wait_flag);
		VsyncCount = 0;
	}


	if(initialize_PCM_flag == 1)
	{
		SND_pcm_abort();
		SND_end();
		free(snd_work);
		free(pcm_left_buffer);
		free(pcm_right_buffer);
	}

	HIS_detachHandler( VSYNCintNumber );

	EGB_init( egb_work, EgbWorkSize );
	EGB_displayPage(egb_work, 0, 3);
	free(egb_work);

	free(name_table);
	free(read_buffer);
	fclose(fp);

	return 0;
}