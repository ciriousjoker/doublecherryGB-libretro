/*--------------------------------------------------
   TGB Dual - Gameboy Emulator -
   Copyright (C) 2001  Hii

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

//-----------------------------------------------
// ROMイメージ管理部 (含SRAM) // ROM image management unit (SRAM included)

#include "gb.h"
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <algorithm>

extern bool logging_transfers_to_file_allowed;

rom::rom()
{
	info = {};
	b_loaded     = false;
   b_persistent = false;

	dat          = NULL;
	sram         = NULL;
}

rom::~rom()
{
   if (!b_persistent)
      free(dat);
	free(sram);
}

void rom::log_info(char* info) {

	
		std::string filePath = "./gameinfo.txt";
		std::ofstream ofs(filePath.c_str(), std::ios_base::out | std::ios_base::app);

		ofs << info;
		ofs.close();
	
}

bool rom::has_battery()
{
	static const int has_bat[]=
		{0,0,0,1,0, 0,1,0,0,1,
		 0,0,1,1,0, 1,1,0,0,1,
		 0,0,0,0,0, 0,0,1,0,1,
		 1,0, 0,0,0,0,0,0,0,0}; // 0x20以下
	if (info.cart_type == 0x20) return true;
	return has_bat[(info.cart_type>0x20)?3:info.cart_type]==1;
}

int rom::get_sram_size()
{
	static const int tbl_ram[]={1,1,1,4,16,8};//0と1は保険
	return 0x2000*tbl_ram[info.ram_size];
}

int rom::get_battery_size()
{
    return info.cart_type == 0x20 ? Mbc6::BatterySize : get_sram_size();
}

bool rom::load_rom(byte *buf,int size,byte *ram,int ram_size, bool persistent)
{
	// Validate geometry before reading the header or replacing loaded data.
	// Every advertised ROM bank must be backed by the supplied buffer.
	if (!buf || size<0x150 || buf[0x148]>8 || buf[0x149]>5 ||
		size<(0x8000<<buf[0x148]))
		return false;

	// First supported MBC6 board is the physically existing 1 MiB/32 KiB board.
	if (buf[0x147] == 0x20 && (buf[0x148] != 5 || buf[0x149] != 3 || size != int(Mbc6::RomSize)))
		return false;
	if (ram_size < 0) return false;

    static const int ram_banks[] = {1, 1, 1, 4, 16, 8};
    const int battery_size = buf[0x147] == 0x20 ? int(Mbc6::BatterySize) : 0x2000 * ram_banks[buf[0x149]];
    byte *loaded_dat = persistent ? buf : static_cast<byte *>(malloc(size));
    byte *loaded_sram = static_cast<byte *>(malloc(battery_size));
    if (!loaded_dat || !loaded_sram) {
        if (!persistent) free(loaded_dat);
        free(loaded_sram);
        return false;
    }
    if (!persistent) memcpy(loaded_dat, buf, size);

	byte momocol_title[16]={0x4D,0x4F,0x4D,0x4F,0x43,0x4F,0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	if (b_loaded){
      if (!b_persistent)
         free(dat);
		free(sram);
	}

	memcpy(info.cart_name,buf+0x134,16);
	info.cart_name[16]='\0';
	info.cart_name[17]='\0';
	info.cart_type=buf[0x147];
	info.rom_size=buf[0x148];
	info.ram_size=buf[0x149];

	if(logging_transfers_to_file_allowed)
		log_info(info.cart_name);

	if (memcmp(info.cart_name,momocol_title,16)==0){
		info.cart_type=0x100;//mmm01
	}

	byte tmp2=buf[0x143];

	info.gb_type=(tmp2&0x80)?3:1;

    dat = loaded_dat;
    first_page = dat;
    sram = loaded_sram;
	memset(sram, 0xff, get_battery_size());
	if (info.cart_type == 0x20) sram[Mbc6::ProtectionOffset] = 0;
	if (ram) memcpy(sram, ram, std::min(ram_size, get_battery_size()));

	b_loaded     = true;
   b_persistent = persistent;

	return true;
}

void rom::serialize(serializer &s)
{
    // Cartridge geometry belongs to the loaded ROM, never to untrusted state bytes.
    rom_info stored_info = info;
    s_VAR(stored_info);
    s.process(sram, get_battery_size());
}

