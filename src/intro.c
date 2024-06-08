#include "global.h"
#include "main.h"
#include "palette.h"
#include "scanline_effect.h"
#include "task.h"
#include "title_screen.h"
#include "malloc.h"
#include "gpu_regs.h"
#include "link.h"
#include "load_save.h"
#include "save.h"
#include "new_game.h"
#include "m4a.h"
#include "intro.h"
#include "graphics.h"
#include "constants/rgb.h"

/*
    The intro is grouped into the following scenes
    Scene 0. Copyright screen
    Scene 1. GF Logo, pan up over plants, Flygon silhouette goes by
    Scene 2. Player biking on path, joined by Pokémon
    Scene 3. A fight between Groudon/Kyogre ends with Rayquaza

    After this it progresses to the title screen
*/

static void CB2_FreePalestine(void);

static void VBlankCB_Intro(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void LoadCopyrightGraphics(u16 tilesetAddress, u16 tilemapAddress, u16 paletteOffset)
{
    LZ77UnCompVram(gIntroCopyright_Gfx, (void *)(VRAM + tilesetAddress));
    LZ77UnCompVram(gIntroCopyright_Tilemap, (void *)(VRAM + tilemapAddress));
    LoadPalette(gIntroCopyright_Pal, paletteOffset, PLTT_SIZE_4BPP);
}

static u8 SetUpCopyrightScreen(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        *(u16 *)PLTT = RGB_WHITE;
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        SetGpuReg(REG_OFFSET_BG0HOFS, 0);
        SetGpuReg(REG_OFFSET_BG0VOFS, 0);
        CpuFill32(0, (void *)VRAM, VRAM_SIZE);
        CpuFill32(0, (void *)OAM, OAM_SIZE);
        CpuFill16(0, (void *)(PLTT + 2), PLTT_SIZE - 2);
        ResetPaletteFade();
        LoadCopyrightGraphics(0, 0x3800, BG_PLTT_ID(0));
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_WHITEALPHA);
        SetGpuReg(REG_OFFSET_BG0CNT, BGCNT_PRIORITY(0)
                                   | BGCNT_CHARBASE(0)
                                   | BGCNT_SCREENBASE(7)
                                   | BGCNT_16COLOR
                                   | BGCNT_TXT256x256);
        EnableInterrupts(INTR_FLAG_VBLANK);
        SetVBlankCallback(VBlankCB_Intro);
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON;
        SetSerialCallback(NULL);
    // REG_DISPCNT needs to be overwritten the second time, because otherwise the intro won't show up on VBA 1.7.2 and John GBA Lite emulators.
    // The REG_DISPCNT overwrite is NOT needed in m-GBA, No$GBA, VBA 1.8.0, My Boy and Pizza Boy GBA emulators.
    case 1:
        REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON;
    default:
        UpdatePaletteFade();
        gMain.state++;
        break;
    case 140:
	BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
	gMain.state++;
        break;
    case 141:
        if (UpdatePaletteFade())
            break;

	/* Play Palestine flag scene */
	SetMainCallback2(CB2_FreePalestine);

	return 0;
    }

    return 1;
}

void CB2_InitCopyrightScreenAfterBootup(void)
{
    if (!SetUpCopyrightScreen())
    {
        SetSaveBlocksPointers(GetSaveBlocksPointersBaseOffset());
        ResetMenuAndMonGlobals();
        Save_ResetSaveCounters();
        LoadGameSave(SAVE_NORMAL);
        if (gSaveFileStatus == SAVE_STATUS_EMPTY || gSaveFileStatus == SAVE_STATUS_CORRUPT)
            Sav2_ClearSetDefault();
        SetPokemonCryStereo(gSaveBlock2Ptr->optionsSound);
        InitHeap(gHeap, HEAP_SIZE);
    }
}

void CB2_InitCopyrightScreenAfterTitleScreen(void)
{
    SetUpCopyrightScreen();
}

const u16 gIntroPalestine_Pal[] = INCBIN_U16("graphics/intro/palestine.gbapal");
const u32 gIntroPalestine_Gfx[] = INCBIN_U32("graphics/intro/palestine.4bpp.lz");
const u32 gIntroPalestine_Tilemap[] = INCBIN_U32("graphics/intro/palestine.bin.lz");

void CB2_FreePalestine(void)
{
    switch (gMain.state)
    {
    case 0:
	LZ77UnCompVram(gIntroPalestine_Gfx, (void*)VRAM);
	LZ77UnCompVram(gIntroPalestine_Tilemap, (void *)(VRAM + 0x3800));
	LoadPalette(gIntroPalestine_Pal, BG_PLTT_ID(0), PLTT_SIZE_4BPP);

	BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, (RGB_BLACK | RGB_ALPHA));
    default:
	UpdatePaletteFade();
	gMain.state++;
	break;
    case 220:
	BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
	gMain.state++;
	break;
    case 221:
        UpdatePaletteFade();
	gMain.state++;
	break;
    case 255:
	SetMainCallback2(CB2_InitTitleScreen);

	SetSerialCallback(SerialCB);
    }
}

void PanFadeAndZoomScreen(u16 screenX, u16 screenY, u16 zoom, u16 alpha)
{
    struct BgAffineSrcData src;
    struct BgAffineDstData dest;

    src.texX = 0x8000;
    src.texY = 0x8000;
    src.scrX = screenX;
    src.scrY = screenY;
    src.sx = zoom;
    src.sy = zoom;
    src.alpha = alpha;
    BgAffineSet(&src, &dest, 1);
    SetGpuReg(REG_OFFSET_BG2PA, dest.pa);
    SetGpuReg(REG_OFFSET_BG2PB, dest.pb);
    SetGpuReg(REG_OFFSET_BG2PC, dest.pc);
    SetGpuReg(REG_OFFSET_BG2PD, dest.pd);
    SetGpuReg(REG_OFFSET_BG2X_L, dest.dx);
    SetGpuReg(REG_OFFSET_BG2X_H, dest.dx >> 16);
    SetGpuReg(REG_OFFSET_BG2Y_L, dest.dy);
    SetGpuReg(REG_OFFSET_BG2Y_H, dest.dy >> 16);
}
