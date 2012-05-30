#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include "gc.h"
#include "defines.h"

syssram* __SYS_LockSram();
u32 __SYS_UnlockSram(u32 write);
u32 __SYS_SyncSram(void);

void GC_SetVideoMode(u8 videomode)
{
	syssram *sram;
	sram = __SYS_LockSram();
	void *m_frameBuf;
	static GXRModeObj *rmode;
	int memflag = 0;

	if((VIDEO_HaveComponentCable() && (CONF_GetProgressiveScan() > 0)) || videomode > 3)
		sram->flags |= 0x80; //set progressive flag
	else
		sram->flags &= 0x7F; //clear progressive flag

	if(videomode == 1 || videomode == 3 || videomode == 5)
	{
		memflag = 1;
		sram->flags |= 0x01; // Set bit 0 to set the video mode to PAL
		sram->ntd |= 0x40; //set pal60 flag
	}
	else
	{
		sram->flags &= 0xFE; // Clear bit 0 to set the video mode to NTSC
		sram->ntd &= 0xBF; //clear pal60 flag
	}

	if(videomode == 1)
		rmode = &TVPal528IntDf;
	else if(videomode == 2)
		rmode = &TVNtsc480IntDf;
	else if(videomode == 3)
	{
		rmode = &TVEurgb60Hz480IntDf;
		memflag = 5;
	}
	else if(videomode == 4)
		rmode = &TVNtsc480Prog;
	else if(videomode == 5)
	{
		rmode = &TVNtsc480Prog; //TVEurgb60Hz480Prog codedumps
		memflag = 5;
	}

	__SYS_UnlockSram(1); // 1 -> write changes
	while(!__SYS_SyncSram());

	/* Set video mode to PAL or NTSC */
	*(vu32*)0x800000CC = memflag;
	DCFlushRange((void *)(0x800000CC), 4);
	ICInvalidateRange((void *)(0x800000CC), 4);

	VIDEO_Configure(rmode);
	m_frameBuf = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	VIDEO_ClearFrameBuffer(rmode, m_frameBuf, COLOR_BLACK);
	VIDEO_SetNextFramebuffer(m_frameBuf);

	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) 
		VIDEO_WaitVSync();
}

u8 get_wii_language()
{
	switch (CONF_GetLanguage())
	{
		case CONF_LANG_GERMAN:
			return SRAM_GERMAN;
		case CONF_LANG_FRENCH:
			return SRAM_FRENCH;
		case CONF_LANG_SPANISH:
			return SRAM_SPANISH;
		case CONF_LANG_ITALIAN:
			return SRAM_ITALIAN;
		case CONF_LANG_DUTCH:
			return SRAM_DUTCH;
		default:
			return SRAM_ENGLISH;
	}
}

void GC_SetLanguage(u8 lang)
{
	if (lang == 0)
		lang = get_wii_language();
	else
		lang--;

	syssram *sram;
	sram = __SYS_LockSram();
	sram->lang = lang;

	__SYS_UnlockSram(1); // 1 -> write changes
	while(!__SYS_SyncSram());
}


void DML_New_SetOptions(char *GamePath)
{
	DML_CFG *DMLCfg = (DML_CFG*)memalign(32, sizeof(DML_CFG));
	memset(DMLCfg, 0, sizeof(DML_CFG));

	DMLCfg->Magicbytes = 0xD1050CF6;
	DMLCfg->CfgVersion = 0x00000001;
	//DMLCfg->VideoMode |= DML_VID_FORCE;
	DMLCfg->VideoMode |= DML_VID_NONE;

	if(GamePath != NULL)
	{
		snprintf(DMLCfg->GamePath, sizeof(DMLCfg->GamePath), "/games/%s/game.iso", GamePath);
		DMLCfg->Config |= DML_CFG_GAME_PATH;
	}

	#ifdef ACTIVITYLED
		DMLCfg->Config |= DML_CFG_ACTIVITY_LED;
	#endif

	#ifdef PADRESET
		DMLCfg->Config |= DML_CFG_PADHOOK;
	#endif

	#ifdef CHEATS
		DMLCfg->Config |= DML_CFG_CHEATS;
	#endif

	#ifdef NMM
		DMLCfg->Config |= DML_CFG_NMM;
	#endif

	#ifdef NODISC
		DMLCfg->Config |= DML_CFG_NODISC;
	#endif

	/*
	if(DMLvideoMode == 1)
		DMLCfg->VideoMode |= DML_VID_FORCE_PAL50;
	else if(DMLvideoMode == 2)
		DMLCfg->VideoMode |= DML_VID_FORCE_NTSC;
	else if(DMLvideoMode == 3)
		DMLCfg->VideoMode |= DML_VID_FORCE_PAL60;
	else
		DMLCfg->VideoMode |= DML_VID_FORCE_PROG;
	*/

	//Write options into memory
	memcpy((void *)0xC0001700, DMLCfg, sizeof(DML_CFG));
	free(DMLCfg);
}

void DML_Old_SetOptions(char *GamePath)
{
	FILE *f;
	f = fopen("sd:/games/boot.bin", "wb");
	fwrite(GamePath, 1, strlen(GamePath) + 1, f);
	fclose(f);

	//Tell DML to boot the game from sd card
	*(vu32*)0x80001800 = 0xB002D105;
	DCFlushRange((void *)(0x80001800), 4);
	ICInvalidateRange((void *)(0x80001800), 4);

	*(vu32*)0xCC003024 |= 7;
}
