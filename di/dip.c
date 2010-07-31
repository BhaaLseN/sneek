/*

SNEEK - SD-NAND/ES + DI emulation kit for Nintendo Wii

Copyright (C) 2009-2010  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "dip.h"

static u32 error ALIGNED(32);
static u32 PartitionSize ALIGNED(32);
static u32 DIStatus ALIGNED(32);
static u32 DICover ALIGNED(32);
static u32 ChangeDisc ALIGNED(32);
DIConfig *DICfg;

static char GamePath[64];
static u32 *KeyID ALIGNED(32);

static u8 *FSTable ALIGNED(32);
u32 ApploaderSize=0;
u32 DolSize=0;
u32 DolOffset=0;
u32 FSTableSize=0;
u32 FSTableOffset=0;
u32 DiscType=0;

u32 FCEntry=0;
FileCache FC[FILECACHE_MAX];

int verbose = 0;
u32 Partition = 0;
u32 Motor = 0;
u32 Disc = 0;
u64 PartitionOffset=0;
u64 PartitionDataOffset=0;
u32 PartitionTableOffset=0;

s32 DVDSelectGame( int SlotID )
{
	u32 count = 0;

	if( DVDOpenDir( "/games" ) != DVD_SUCCESS )
	{
		return DI_FATAL;
	}

	while( DVDReadDir() == DVD_SUCCESS )
	{
		if( DVDDirIsFile() )		// skip files
			continue;

		if( count == SlotID )
		{
			char *str = (char *)malloca( 128, 32 );
			//build path
			sprintf( GamePath, "/games/%s/", DVDDirGetEntryName() );
			dbgprintf("DIP:Set game path to:\"%s\"\n", GamePath );

			FSTable = (u8*)NULL;
			ChangeDisc = 1;
			DICover |= 1;
		
			//Get Apploader size
			sprintf( str, "%ssys/apploader.img", GamePath );
			s32 fd = DVDOpen( str, DREAD );
			if( fd < 0 )
			{
				DIP_Fatal("DVDSelectGame()",__LINE__,__FILE__, fd, "Failed to open apploader.img!");
				while(1);
			}

			ApploaderSize = DVDGetSize( fd ) >> 2;
			DVDClose( fd );
		
			dbgprintf("DIP:apploader size:%08X\n", ApploaderSize<<2 );

			free( str );

			//update di-config
			fd = DVDOpen( "/sneek/diconfig.bin", DWRITE );
			if( fd >= 0 )
			{
				DICfg->SlotID = SlotID;
				DVDWrite( fd, DICfg, 0x10 );
				DVDClose( fd );
			}

			//Init cache
			for( count=0; count < FILECACHE_MAX; ++count )
			{
				FC[count].File = 0xdeadbeef;
			}

			return DI_SUCCESS;
		}
		count++;
	}

	return DI_FATAL;
}

unsigned char sig_fwrite[32] =
{
    0x94, 0x21, 0xFF, 0xD0,
	0x7C, 0x08, 0x02, 0xA6,
	0x90, 0x01, 0x00, 0x34,
	0xBF, 0x21, 0x00, 0x14, 
    0x7C, 0x9B, 0x23, 0x78,
	0x7C, 0xDC, 0x33, 0x78,
	0x7C, 0x7A, 0x1B, 0x78,
	0x7C, 0xB9, 0x2B, 0x78, 
} ;

unsigned char patch_fwrite[144] =
{
    0x7C, 0x85, 0x21, 0xD7, 0x40, 0x81, 0x00, 0x84, 0x3C, 0xE0, 0xCD, 0x00, 0x3D, 0x40, 0xCD, 0x00, 
    0x3D, 0x60, 0xCD, 0x00, 0x60, 0xE7, 0x68, 0x14, 0x61, 0x4A, 0x68, 0x24, 0x61, 0x6B, 0x68, 0x20, 
    0x38, 0xC0, 0x00, 0x00, 0x7C, 0x06, 0x18, 0xAE, 0x54, 0x00, 0xA0, 0x16, 0x64, 0x08, 0xB0, 0x00, 
    0x38, 0x00, 0x00, 0xD0, 0x90, 0x07, 0x00, 0x00, 0x7C, 0x00, 0x06, 0xAC, 0x91, 0x0A, 0x00, 0x00, 
    0x7C, 0x00, 0x06, 0xAC, 0x38, 0x00, 0x00, 0x19, 0x90, 0x0B, 0x00, 0x00, 0x7C, 0x00, 0x06, 0xAC, 
    0x80, 0x0B, 0x00, 0x00, 0x7C, 0x00, 0x04, 0xAC, 0x70, 0x09, 0x00, 0x01, 0x40, 0x82, 0xFF, 0xF4, 
    0x80, 0x0A, 0x00, 0x00, 0x7C, 0x00, 0x04, 0xAC, 0x39, 0x20, 0x00, 0x00, 0x91, 0x27, 0x00, 0x00, 
    0x7C, 0x00, 0x06, 0xAC, 0x74, 0x09, 0x04, 0x00, 0x41, 0x82, 0xFF, 0xB8, 0x38, 0xC6, 0x00, 0x01, 
    0x7F, 0x86, 0x20, 0x00, 0x40, 0x9E, 0xFF, 0xA0, 0x7C, 0xA3, 0x2B, 0x78, 0x4E, 0x80, 0x00, 0x20, 
};
unsigned char sig_iplmovie[] =
{
    0x4D, 0x50, 0x4C, 0x53,
	0x2E, 0x4D, 0x4F, 0x56,
	0x49, 0x45, 0x00, 
} ;

unsigned char patch_iplmovie[] =
{
    0x49, 0x50, 0x4C, 0x2E,
	0x45, 0x55, 0x4C, 0x41,
	0x00, 
} ;

s32 DVDLowRead( u32 Offset, u32 Length, void *ptr )
{
	s32 fd;
	char Path[256];

	//dbgprintf("DIP:Read Offset:0x%08X Size:0x%08X as:%08X\n", Offset<<2, Length, ApploaderSize<<2 );

	if( Offset < 0x110 )	// 0x440
	{
		sprintf( Path, "%ssys/boot.bin", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			dbgprintf("DIP:[boot.bin] Failed to open!\n" );
			return DI_FATAL;
		} else {
			dbgprintf("DIP:[boot.bin] Offset:%08X Size:%08X\n", Offset, Length );

			u32 *rbuf = (u32*)halloca( sizeof(u32), 32 );

			//read requested data
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );

			//Read DOL/FST offset/sizes for later usage
			DVDSeek( fd, 0x0420, 0 );
			DVDRead( fd, rbuf, 4 );
			DolOffset = *rbuf;

			DVDSeek( fd, 0x0424, 0 );
			DVDRead( fd, rbuf, 4 );
			FSTableOffset = *rbuf;

			DVDSeek( fd, 0x0428, 0 );
			DVDRead( fd, rbuf, 4 );
			FSTableSize = *rbuf;

			hfree( rbuf );

			DolSize = FSTableOffset - DolOffset;
			FSTableSize <<= 2;

			if( DiscType == DISC_REV )
			{
				dbgprintf("DIP:FSTableOffset:%08X\n", FSTableOffset<<2 );
				dbgprintf("DIP:FSTableSize:  %08X\n", FSTableSize );
				dbgprintf("DIP:DolOffset:    %08X\n", DolOffset<<2 );
				dbgprintf("DIP:DolSize:      %08X\n", DolSize<<2 );
			} else {
				dbgprintf("DIP:FSTableOffset:%08X\n", FSTableOffset );
				dbgprintf("DIP:FSTableSize:  %08X\n", FSTableSize );
				dbgprintf("DIP:DolOffset:    %08X\n", DolOffset );
				dbgprintf("DIP:DolSize:      %08X\n", DolSize );
			}

			DVDClose( fd );
			return DI_SUCCESS;
		}

	} else if( Offset < 0x910 )	// 0x2440
	{
		Offset -= 0x110;

		sprintf( Path, "%ssys/bi2.bin", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			dbgprintf("DIP:[bi2.bin] Failed to open!\n" );
			return DI_FATAL;
		} else {
			dbgprintf("DIP:[bi2.bin] Offset:%08X Size:%08X Dst:%p\n", Offset, Length, ptr );
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );

			//GC region patch
			if( DiscType == DISC_DOL )
			{
				*(vu32*)(ptr+0x18) = DICfg->Region;
				dbgprintf("DIP:Patched GC region to:%d\n", DICfg->Region );
			}
			return DI_SUCCESS;
		}


	} else if( Offset < 0x910+ApploaderSize )	// 0x2440
	{
		Offset -= 0x910;

		sprintf( Path, "%ssys/apploader.img", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			dbgprintf("DIP:[apploader.img] Failed to open!\n" );
			return DI_FATAL;
		} else {
			dbgprintf("DIP:[apploader.img] Offset:%08X Size:%08X Dst:%p\n", Offset, Length, ptr );
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );
			return DI_SUCCESS;
		}
	} else if( Offset < DolOffset + DolSize )
	{
		Offset -= DolOffset;

		sprintf( Path, "%ssys/main.dol", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			dbgprintf("DIP:[main.dol] Failed to open!\n" );
			return DI_FATAL;
		} else {
			dbgprintf("DIP:[main.dol] Offset:%08X Size:%08X Dst:%p\n", Offset, Length, ptr );
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );

			int i;
			for( i=0; i < Length; i+=4 )
			{
				if( DICfg->Config & CONFIG_PATCH_FWRITE )
				{
					if( memcmp( (void*)(ptr+i), sig_fwrite, sizeof(sig_fwrite) ) == 0 )
					{
						dbgprintf("DIP:[patcher] Found __fwrite pattern:%08X\n",  (u32)(ptr+i) | 0x80000000 );
						memcpy( (void*)(ptr+i), patch_fwrite, sizeof(patch_fwrite) );
					}
				}
				if( DICfg->Config & CONFIG_PATCH_MPVIDEO )
				{
					if( memcmp( (void*)(ptr+i), sig_iplmovie, sizeof(sig_iplmovie) ) == 0 )
					{
						dbgprintf("DIP:[patcher] Found sig_iplmovie pattern:%08X\n", (u32)(ptr+i) | 0x80000000 );
						memcpy( (void*)(ptr+i), patch_iplmovie, sizeof(patch_iplmovie) );
					}
				}
				if( DICfg->Config & CONFIG_PATCH_VIDEO )
				{
					if( *(vu32*)(ptr+i) == 0x3C608000 )
					{
						if( ((*(vu32*)(ptr+i+4) & 0xFC1FFFFF ) == 0x800300CC) && ((*(vu32*)(ptr+i+8) >> 24) == 0x54 ) )
						{
							dbgprintf("DIP:[patcher] Found VI pattern:%08X\n", (u32)(ptr+i) | 0x80000000 );
							*(vu32*)(ptr+i+4) = 0x5400F0BE | ((*(vu32*)(ptr+i+4) & 0x3E00000) >> 5	);
						}
					}
				}
			}

			return DI_SUCCESS;
		}
	} else if( Offset < FSTableOffset+(FSTableSize>>2) )
	{
		Offset -= FSTableOffset;

		sprintf( Path, "%ssys/fst.bin", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			dbgprintf("DIP:[fst.bin]  Failed to open!\n" );
			return DI_FATAL;
		} else {
			dbgprintf("DIP:[fst.bin]  Offset:%08X Size:%08X Dst:%p\n", Offset, Length, ptr );
			DVDSeek( fd, (u64)Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );

			if( FSTable == NULL )
			{
				FSTable	= (u8*)ptr;
				dbgprintf("DIP:FSTOffset:  %08X\n", (u32)FSTable );
			}

			return DI_SUCCESS;
		}
	} else {
		
		//Get FSTTable offset from low memory, must be set by apploader
		if( FSTable == NULL )
		{
			FSTable	= (u8*)((*(vu32*)0x38) & 0x7FFFFFFF);
			dbgprintf("DIP:FSTOffset:  %08X\n", (u32)FSTable );
		}

		u32 i,j;

		//try cache first!
		for( i=0; i < FILECACHE_MAX; ++i )
		{
			if( FC[i].File == 0xdeadbeef )
				continue;

			if( Offset >= FC[i].Offset )
			{
				u64 nOffset = ((u64)(Offset-FC[i].Offset)) << 2;
				if( nOffset < FC[i].Size )
				{
					//dbgprintf("DIP:[Cache:%02d][%08X:%05X]\n", i, (u32)(nOffset>>2), Length );
					DVDSeek( FC[i].File, nOffset, 0 );
					DVDRead( FC[i].File, ptr, ((Length)+31)&(~31) );
					return DI_SUCCESS;
				}
			}
		}

		//The fun part!

		u32 Entries = *(u32*)(FSTable+0x08);
		char *NameOff = (char*)(FSTable + Entries * 0x0C);
		FEntry *fe = (FEntry*)(FSTable);

		u32 Entry[16];
		u32 LEntry[16];
		u32 level=0;

		for( i=1; i < Entries; ++i )
		{
			if( level )
			{
				while( LEntry[level-1] == i )
				{
					//printf("[%03X]leaving :\"%s\" Level:%d\n", i, buffer + NameOff + swap24( fe[Entry[level-1]].NameOffset ), level );
					level--;
				}
			}

			if( fe[i].Type )
			{
				//Skip empty folders
				if( fe[i].NextOffset == i+1 )
					continue;

				//printf("[%03X]Entering:\"%s\" Level:%d leave:%04X\n", i, buffer + NameOff + swap24( fe[i].NameOffset ), level, swap32( fe[i].NextOffset ) );
				Entry[level] = i;
				LEntry[level++] = fe[i].NextOffset;
				if( level > 15 )	// something is wrong!
					break;
			} else {

				if( Offset >= fe[i].FileOffset )
				{
					u64 nOffset = ((u64)(Offset-fe[i].FileOffset)) << 2;
					if( nOffset < fe[i].FileLength )
					{
						//dbgprintf("DIP:Offset:%08X FOffset:%08X Dif:%08X Flen:%08X nOffset:%08X\n", Offset, fe[i].FileOffset, Offset-fe[i].FileOffset, fe[i].FileLength, nOffset );

						//dbgprintf("Found Entry:%X file:%s FCEntry:%d\n", i,  FSTable + NameOff + fe[i].NameOffset, FCEntry );

						//Do not remove!
						memset( Path, 0, 256 );					
						sprintf( Path, "%sfiles/", GamePath );

						for( j=0; j<level; ++j )
						{
							if( j )
								Path[strlen(Path)] = '/';
							memcpy( Path+strlen(Path), NameOff + fe[Entry[j]].NameOffset, strlen(NameOff + fe[Entry[j]].NameOffset ) );
						}
						if( level )
							Path[strlen(Path)] = '/';
						memcpy( Path+strlen(Path), NameOff + fe[i].NameOffset, strlen(NameOff + fe[i].NameOffset) );

						if( FCEntry >= FILECACHE_MAX )
							FCEntry = 0;

						if( FC[FCEntry].File != 0xdeadbeef )
						{
							DVDClose( FC[FCEntry].File );
							FC[FCEntry].File = 0xdeadbeef;
						}

						FC[FCEntry].File = DVDOpen( Path, DREAD );
						if( FC[FCEntry].File < 0 )
						{
							FC[FCEntry].File = 0xdeadbeef;

							dbgprintf("DIP:[%s] Failed to open!\n", Path );
							error = 0x031100;
							return DI_FATAL;
						} else {

							FC[FCEntry].Size	= fe[i].FileLength;
							FC[FCEntry].Offset	= fe[i].FileOffset;

							dbgprintf("DIP:[%s][%08X:%05X]\n", Path, (u32)(nOffset>>2), Length );
							DVDSeek( FC[FCEntry].File, nOffset, 0 );
							DVDRead( FC[FCEntry].File, ptr, Length );
							
							FCEntry++;

							return DI_SUCCESS;
						}
					}
				}
			}
		}
	}
	return DI_FATAL;
}
s32 DVDLowReadUnencrypted( u32 Offset, u32 Length, void *ptr )
{
	switch( Offset )
	{
		case 0x00:
		case 0x08:		// 0x20
		{
			char *str = (char *)malloca( 64, 32 );
			sprintf( str, "%ssys/boot.bin", GamePath );

			u32 ret = DI_FATAL;
			s32 fd = DVDOpen( str, FA_READ );
			if( fd >= 0 )
			{
				if( DVDRead( fd, ptr, Length ) == Length )
				{
					free( str );
					ret = DI_SUCCESS;				
				}
				DVDClose( fd );
			}

			free( str );
			return ret;
			
		} break;
		case 0x10000:		// 0x40000
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)	= 1;				// one partition
			*(u32*)(ptr+4)	= 0x40020>>2;		// partition table info

			return DI_SUCCESS;
		} break;
		case 0x10008:		// 0x40020
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)	= 0x03E00000;		// partition offset
			*(u32*)(ptr+4)	= 0x00000000;		// partition type

			return DI_SUCCESS;
		} break;
		case 0x00013800:		// 0x4E000
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)		= DICfg->Region;
			*(u32*)(ptr+0x1FFC)	= 0xC3F81A8E;

			dbgprintf("DIP:Region:%d\n", DICfg->Region );

			return DI_SUCCESS;
		} break;
		default:
			dbgprintf("DIP:unexpected read offset!\n");
			dbgprintf("DIP:DVDLowUnencryptedRead( %08X, %08X, %p )\n", Offset, Length, ptr );
			while(1);
		break;
	}
}
s32 DVDLowReadDiscID( u32 Offset, u32 Length, void *ptr )
{
	char *str = (char *)malloca( 32, 32 );
	sprintf( str, "%ssys/boot.bin", GamePath );
	s32 fd = DVDOpen( str, FA_READ );
	if( fd < 0 )
	{
		return DI_FATAL;
	} else {
		DVDRead( fd, ptr, Length );
		DVDClose( fd );
	}

	free( str );

	hexdump( ptr, Length );

	if( *(vu32*)(ptr+0x1C) == 0xc2339f3d )
	{
		DiscType = DISC_DOL;
	} else if( *(vu32*)(ptr+0x18) == 0x5D1C9EA3 )
	{
		DiscType = DISC_REV;
	} else {
		DiscType = DISC_INV;
	}

	return DI_SUCCESS;
}

void DIP_Fatal( char *name, u32 line, char *file, s32 error, char *msg )
{
	//dbgprintf("************ DI FATAL ERROR ************\n");
	//dbgprintf("Function :%s\n", name );
	//dbgprintf("line     :%d\n", line );
	//dbgprintf("file     :%s\n", file );
	//dbgprintf("error    :%d\n", error );
	//dbgprintf("%s\n", msg );
	//dbgprintf("************ DI FATAL ERROR ************\n");

	while(1);
}
int DIP_Ioctl( struct ipcmessage *msg )
{
	u8  *bufin  = (u8*)msg->ioctl.buffer_in;
	u32 lenin   = msg->ioctl.length_in;
	u8  *bufout = (u8*)msg->ioctl.buffer_io;
	u32 lenout  = msg->ioctl.length_io;
	s32 ret		= DI_FATAL;
	s32 fd;

	//dbgprintf("DIP:IOS_Ioctl( %d 0x%x 0x%p 0x%x 0x%p 0x%x )\n", msg->fd, msg->ioctl.command, bufin, lenin, bufout, lenout);
			
	switch(msg->ioctl.command)
	{
		case DVD_WRITE_CONFIG:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;
			fd = DVDOpen( "/sneek/diconfig.bin", FA_WRITE|FA_OPEN_EXISTING );
			if( fd < 0 )
			{
				DVDDelete( "/sneek/diconfig.bin" );
				fd = DVDOpen( "/sneek/diconfig.bin", FA_WRITE|FA_CREATE_ALWAYS );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				}
			}
			DVDWrite( fd, (u8*)(vec[0]), 0x10 );
			DVDClose( fd );
					
			memcpy( DICfg, (u8*)(vec[0]), 0x10 );

			dbgprintf("DIP:Region:%d SlotID:%d GameCount:%d Config:%04X\n", ((DIConfig*)(vec[0]))->Region, ((DIConfig*)(vec[0]))->SlotID, ((DIConfig*)(vec[0]))->Gamecount, ((DIConfig*)(vec[0]))->Config  );

			ret = DI_SUCCESS;

			dbgprintf("DIP:DVDWriteDIConfig( %p ):%d\n", vec[0], ret );
		} break;
		case DVD_READ_GAMEINFO:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;

			fd = DVDOpen( "/sneek/diconfig.bin", FA_READ );
			if( fd < 0 )
			{
				ret = DI_FATAL;
			} else {

				DVDSeek( fd, vec[0], 0 );
				DVDRead( fd, (u8*)(vec[2]), vec[1] );
				DVDClose( fd );

				ret = DI_SUCCESS;
			}

			dbgprintf("DIP:DVDReadGameInfo( %08X, %08X, %p ):%d\n", vec[0], vec[1], vec[2], ret );
		} break;
		case DVD_INSERT_DISC:
		{
			DICover &= ~1;
			ret = DI_SUCCESS;
			dbgprintf("DIP:InsertDisc(%d):%d\n", ret );
		} break;
		case DVD_EJECT_DISC:
		{
			DICover |= 1;
			ret = DI_SUCCESS;
			dbgprintf("DIP:EjectDisc(%d):%d\n", ret );
		} break;
		case DVD_GET_GAMECOUNT:	//Get Game count
		{
			*(u32*)bufout = DICfg->Gamecount;
			ret = DI_SUCCESS;
			dbgprintf("DIP:GetGameCount(%d):%d\n", DICfg->Gamecount, ret );
		} break;
		case DVD_SELECT_GAME:
		{
			ret = DVDSelectGame( *(u32*)(bufin) );

			dbgprintf("DIP:DVDSelectGame(%d):%d\n", *(u32*)(bufin), ret );
		} break;
		case DVD_SET_AUDIO_BUFFER:
		{
			memset32( bufout, 0, lenout );
			hexdump( bufin, lenin );
			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowConfigAudioBuffer():%d\n", ret );
		} break;
		case 0x96:
		case DVD_REPORTKEY:
		{
			ret = DI_ERROR;
			error = 0x00052000;
			dbgprintf("DIP:DVDLowReportKey():%d\n", ret );
		} break;
		case 0xDD:			// 0 out
		{
			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowSetMaximumRotation():%d\n", ret);
		} break;
		case 0x95:			// 0x20 out
		{
			*(u32*)bufout = DIStatus;

			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowPrepareStatusRegister( %08X ):%d\n", *(u32*)bufout, ret );
		} break;
		case 0x7A:			// 0x20 out
		{
			*(u32*)bufout = DICover;

			if( ChangeDisc )
			{
				DICover &= ~1;
				ChangeDisc = 0;
			}

			ret = DI_SUCCESS;
			//dbgprintf("DIP:DVDLowPrepareCoverRegister( %08X ):%d\n", *(u32*)bufout, ret );
		} break;
		case 0x86:			// 0 out
		{
			ret = DI_SUCCESS;
			//dbgprintf("DIP:DVDLowClearCoverInterrupt():%d\n", ret);
		} break;
		case DVD_GETCOVER:	// 0x88
		{
			*(u32*)bufout = DICover;
			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowGetCoverStatus( %08X ):%d\n", *(u32*)bufout, ret );
		} break;
		case 0x89:
		{
			DICover &= ~4;
			DICover |= 2;

			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowUnknownRegister():%d\n", ret );
		} break;
		case DVD_IDENTIFY:
		{
			memset( bufout, 0, 0x20 );

			*(u32*)(bufout)		= 0x00000002;
			*(u32*)(bufout+4)	= 0x20070213;
			*(u32*)(bufout+8)	= 0x41000000;

			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowIdentify():%d\n", ret);
		} break;
		case DVD_GET_ERROR:	// 0xE0
		{
			*(u32*)bufout = error; 
			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowGetError( %08X ):%d\n", error, ret );
		} break;
		case 0x8E:
		{
			if( (*(u8*)(bufin+4)) == 0 )
				EnableVideo( 1 );
			else
				EnableVideo( 0 );

			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowEnableVideo(%d):%d\n", *(u32*)(bufin+4), ret);
		} break;
		case DVD_LOW_READ:	// 0x71, GC never calls this
		{
			if( Partition )
			{
				if( *(u32*)(bufin+8) > PartitionSize )
				{
					ret = DI_ERROR;
					error = 0x00052100;
				} else {
					ret = DVDLowRead( *(u32*)(bufin+8), *(u32*)(bufin+4), bufout );
				}
			} else {
				DIP_Fatal("DVDLowRead", __LINE__, __FILE__, 64, "Partition not opened!");
			}
			if( ret < 0 )
				dbgprintf("DIP:DVDLowRead( %08X, %08X, %p ):%d\n", *(u32*)(bufin+8), *(u32*)(bufin+4), bufout, ret );
		} break;
		case DVD_READ_UNENCRYPTED:
		{
			if( DiscType == DISC_DOL )
			{
				ret = DVDLowRead( *(u32*)(bufin+8), *(u32*)(bufin+4), bufout );

			} else if ( DiscType == DISC_REV )
			{
				if( *(u32*)(bufin+8) > 0x46090000 )
				{
					error = 0x00052100;
					return DI_ERROR;

				} else {
					ret = DVDLowReadUnencrypted( *(u32*)(bufin+8), *(u32*)(bufin+4), bufout );
				}
			} else {
				//Invalid disc type!
				ret = DI_FATAL;
			}

			dbgprintf("DIP:DVDLowUnencryptedRead( %08X, %08X, %p ):%d\n", *(u32*)(bufin+8), *(u32*)(bufin+4), bufout, ret );
		} break;
		case DVD_READ_DISCID:
		{
			ret = DVDLowReadDiscID( 0, lenout, bufout );

			dbgprintf("DIP:DVDLowReadDiscID(%p):%d\n", bufout, ret );
		} break;
		case DVD_RESET:
		{
			DIStatus = 0x00;
			DICover  = 0x00;
			Partition = 0;

			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowReset( %d ):%d\n", *(u32*)(bufin+4), ret);
		} break;
		case DVD_SET_MOTOR:
		{
			Motor = 0;
			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDStopMotor(%d,%d):%d\n", *(u32*)(bufin+4), *(u32*)(bufin+8), ret);
		} break;
		case DVD_CLOSE_PARTITION:
		{
			Partition=0;
			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowClosePartition():%d\n", ret );
		} break;
		case DVD_READ_BCA:
		{
			memset32( (u32*)bufout, 0, 0x40 );
			*(u32*)(bufout+0x30) = 0x00000001;

			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowReadBCA():%d\n", ret );
		} break;
		case DVD_LOW_SEEK:
		{
			ret = DI_SUCCESS;
			dbgprintf("DIP:DVDLowSeek():%d\n", ret );
		} break;
		default:
			hexdump( bufin, lenin );
			dbgprintf("DIP:Unknown IOS_Ioctl( %d 0x%x 0x%p 0x%x 0x%p 0x%x )\n", msg->fd, msg->ioctl.command, bufin, lenin, bufout, lenout);
			MessageQueueAck( (void *)msg, DI_FATAL);
			while(1);
		break;
	}

	//Reset errror after a successful command
	if( msg->command != 0xE0 && ret == DI_SUCCESS )
		error = 0;

	return ret;
}
int DIP_Ioctlv(struct ipcmessage *msg)
{
	u32 InCount		= msg->ioctlv.argc_in;
	u32 OutCount	= msg->ioctlv.argc_io;
	vector *v		= (vector*)(msg->ioctlv.argv);
	s32 ret			= DI_FATAL;

	switch(msg->ioctl.command)
	{
		case DVD_OPEN_PARTITION:
		{
			if( Partition == 1 )
			{
				DIP_Fatal( "DVDLowOpenPartition", __LINE__, __FILE__, 0, "Partition already open!");
			}

			PartitionOffset = *(u32*)(v[0].data+4);
			PartitionOffset <<= 2;

			u8 *TMD;
			u8 *TIK;
			u8 *CRT;
			u8 *hashes = (u8*)halloca( sizeof(u8)*0x14, 32 );
			u8 *buffer = (u8*)halloca( 0x40, 32 );
			memset( buffer, 0, 0x40 );

			char *str = (char*)halloca( 0x40,32 );

			sprintf( str, "%sticket.bin", GamePath );

			s32 fd = DVDOpen( str, FA_READ );
			if( fd < 0 )
			{
				dbgprintf("DIP:Could not open:\"%s\"\n", str );
				ret = DI_FATAL;
				break;
			} else {
				TIK = (u8*)halloca( 0x2a4, 32 );

				ret = DVDRead( fd, TIK, 0x2a4 );

				if( ret != 0x2A4 )
				{
					ret = DI_FATAL;
					DVDClose( fd );
					dbgprintf("DIP:Failed to read:\"%s\":%d\n", str, ret );
					break;
				}

				((u32*)buffer)[0x04] = (u32)TIK;			//0x10
				((u32*)buffer)[0x05] = 0x2a4;				//0x14
				sync_before_read(TIK, 0x2a4);
				
				DVDClose( fd );
			}

			sprintf( str, "%stmd.bin", GamePath );
			
			fd = DVDOpen( str, FA_READ );
			if( fd < 0 )
			{
				dbgprintf("DIP:Could not open:\"%s\"\n", str );
				ret = DI_FATAL;
				break;
			} else {
				u32 asize = (DVDGetSize(fd)+31)&(~31);
				TMD = (u8*)halloca( asize, 32 );
				memset( TMD, 0, asize );
				ret = DVDRead( fd, TMD, DVDGetSize(fd) );
				if( ret != DVDGetSize(fd) )
				{
					ret = DI_FATAL;
					DVDClose( fd );
					dbgprintf("DIP:Failed to read:\"%s\":%d\n", str, ret );
					break;
				} 

				((u32*)buffer)[0x06] = (u32)TMD;		//0x18
				((u32*)buffer)[0x07] = DVDGetSize(fd);		//0x1C
				sync_before_read(TMD, asize);

				PartitionSize = (u32)((*(u64*)(TMD+0x1EC)) >> 2 );
				dbgprintf("DIP:Set partition size to:%02X%08X\n", (u32)((((u64)PartitionSize)<<2)>>32), (PartitionSize<<2) );

				if( v[3].data != NULL )
				{
					memcpy( v[3].data, TMD, DVDGetSize(fd) );
					sync_after_write( v[3].data, DVDGetSize(fd) );
				}

				DVDClose( fd );
			}

			sprintf( str, "%scert.bin", GamePath );
			fd = DVDOpen( str, FA_READ );
			if( fd < 0 )
			{
				dbgprintf("DIP:Could not open:\"%s\"\n", str );
				ret = DI_FATAL;
				break;
			} else {
				CRT = (u8*)halloca( 0xA00, 32 );
				ret = DVDRead( fd, CRT, 0xA00 );
				if( ret != 0xA00 )
				{
					ret = DI_FATAL;
					DVDClose( fd );
					dbgprintf("DIP:Failed to read:\"%s\":%d\n", str, ret );
					break;
				}

				((u32*)buffer)[0x00] = (u32)CRT;		//0x00
				((u32*)buffer)[0x01] = 0xA00;			//0x04
				sync_before_read(CRT, 0xA00);

				DVDClose( fd );
			}

			hfree( str );

			KeyID = (u32*)malloca( sizeof( u32 ), 32 );


			((u32*)buffer)[0x02] = (u32)NULL;			//0x08
			((u32*)buffer)[0x03] = 0;					//0x0C

			//out
			((u32*)buffer)[0x08] = (u32)KeyID;			//0x20
			((u32*)buffer)[0x09] = 4;					//0x24
			((u32*)buffer)[0x0A] = (u32)hashes;			//0x28
			((u32*)buffer)[0x0B] = 20;					//0x2C

			sync_before_read(buffer, 0x40);

			s32 ESHandle = IOS_Open("/dev/es", 0 );

			s32 r = IOS_Ioctlv( ESHandle, 0x1C, 4, 2, buffer );
			if( r < 0)
				dbgprintf("ios_ioctlv( %d, %02X, %d, %d, %p ):%d\n", ESHandle, 0x1C, 4, 2, buffer, r );

			IOS_Close( ESHandle );

			*(u32*)(v[4].data) = 0x00000000;

			free( KeyID );

			hfree( buffer );
			hfree( TIK );
			hfree( CRT );
			hfree( TMD );

			if( r < 0 )
			{
				DIP_Fatal( "DVDLowOpenPartition", __LINE__, __FILE__, r, "ios_ioctlv() failed!");
			}
			
			ret = DI_SUCCESS;

			Partition=1;

			dbgprintf("DIP:DVDOpenPartition(0x%08X):%d\n", *(u32*)(v[0].data+4), ret );

		} break;
		default:
			dbgprintf("DIP:IOS_Ioctlv( %d, 0x%x 0x%x 0x%x 0x%p )\n", msg->fd, msg->ioctl.command, InCount, OutCount, msg->ioctlv.argv );
			MessageQueueAck( (struct ipcmessage *)msg, DI_FATAL);
			while(1);
		break;
	}

	//Reset errror after a successful command
	if( ret == DI_SUCCESS )
		error = 0;

	return ret;
}


