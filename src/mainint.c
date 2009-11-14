/* 
 * OpenTyrian Classic: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "backgrnd.h"
#include "config.h"
#include "editship.h"
#include "episodes.h"
#include "file.h"
#include "fonthand.h"
#include "helptext.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "lds_play.h"
#include "loudness.h"
#include "mainint.h"
#include "menus.h"
#include "mouse.h"
#include "mtrand.h"
#include "network.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyr.h"
#include "palette.h"
#include "params.h"
#include "pcxmast.h"
#include "picload.h"
#include "setup.h"
#include "sndmast.h"
#include "sprite.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"

#include <assert.h>
#include <ctype.h>

bool button[4];

#define MAX_PAGE 8
#define TOPICS 6
const JE_byte topicStart[TOPICS] = { 0, 1, 2, 3, 7, 255 };

JE_shortint constantLastX;
JE_word textErase;
JE_word upgradeCost;
JE_word downgradeCost;
JE_boolean performSave;
JE_boolean jumpSection;
JE_boolean useLastBank; /* See if I want to use the last 16 colors for DisplayText */

bool pause_pressed = false, ingamemenu_pressed = false;

/* Draws a message at the bottom text window on the playing screen */
void JE_drawTextWindow( const char *text )
{
	if (textErase > 0) // erase current text
		blit_sprite(VGAScreenSeg, 16, 189, OPTION_SHAPES, 36);  // in-game text area
	
	textErase = 100;
	tempScreenSeg = VGAScreenSeg; /*sega000*/
	JE_outText(20, 190, text, 0, 4);
}

void JE_outCharGlow( JE_word x, JE_word y, const char *s )
{
	JE_integer maxloc, loc, z;
	JE_shortint glowcol[60]; /* [1..60] */
	JE_shortint glowcolc[60]; /* [1..60] */
	JE_word textloc[60]; /* [1..60] */
	JE_byte bank;
	
	setjasondelay2(1);
	
	bank = (warningRed) ? 7 : ((useLastBank) ? 15 : 14);
	
	if (s[0] == '\0')
		return;
	
	if (frameCountMax == 0)
	{
		JE_textShade(x, y, s, bank, 0, PART_SHADE);
		JE_showVGA();
	}
	else
	{
		maxloc = strlen(s);
		tempScreenSeg = VGAScreen;
		for (z = 0; z < 60; z++)
		{
			glowcol[z] = -8;
			glowcolc[z] = 1;
		}
		
		loc = x;
		for (z = 0; z < maxloc; z++)
		{
			textloc[z] = loc;

			int sprite_id = font_ascii[(unsigned char)s[z]];

			if (s[z] == ' ')
				loc += 6;
			else if (sprite_id != -1)
				loc += sprite(TINY_FONT, sprite_id)->width + 1;
		}
		
		for (loc = 0; (unsigned)loc < strlen(s) + 28; loc++)
		{
			if (!ESCPressed)
			{
				setjasondelay(frameCountMax);
				
				NETWORK_KEEP_ALIVE();
				
				int sprite_id = -1;
				
				for (z = loc - 28; z <= loc; z++)
				{
					if (z >= 0 && z < maxloc)
					{
						sprite_id = font_ascii[(unsigned char)s[z]];
						
						if (sprite_id != -1)
						{
							blit_sprite_hv(VGAScreen, textloc[z], y, TINY_FONT, sprite_id, bank, glowcol[z]);
							
							glowcol[z] += glowcolc[z];
							if (glowcol[z] > 9)
								glowcolc[z] = -1;
						}
					}
				}
				if (sprite_id != -1 && --z < maxloc)
					blit_sprite_dark(tempScreenSeg, textloc[z] + 1, y + 1, TINY_FONT, sprite_id, true);
				
				if (JE_anyButton())
					frameCountMax = 0;
				
				do
				{
					if (levelWarningDisplay)
						JE_updateWarning();
					
					SDL_Delay(16);
				}
				while (!(delaycount() == 0 || ESCPressed));
				
				JE_showVGA();
			}
		}
	}
}

void JE_drawPortConfigButtons( void ) // rear weapon pattern indicator
{
	if (twoPlayerMode)
		return;
	
	if (portConfig[1] == 1)
	{
		blit_sprite(VGAScreenSeg, 285, 44, OPTION_SHAPES, 18);  // lit
		blit_sprite(VGAScreenSeg, 302, 44, OPTION_SHAPES, 19);  // unlit
	}
	else
	{
		blit_sprite(VGAScreenSeg, 285, 44, OPTION_SHAPES, 19);  // unlit
		blit_sprite(VGAScreenSeg, 302, 44, OPTION_SHAPES, 18);  // lit
	}
}

void JE_helpSystem( JE_byte startTopic )
{
	JE_integer page, lastPage = 0;
	JE_byte menu;

	page = topicStart[startTopic-1];

	fade_black(10);
	JE_loadPic(2, false);
	
	play_song(SONG_MAPVIEW);
	
	JE_showVGA();
	fade_palette(colors, 10, 0, 255);

	memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);

	do
	{
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

		temp2 = 0;

		for (temp = 0; temp < TOPICS; temp++)
		{
			if (topicStart[temp] <= page)
			{
				temp2 = temp;
			}
		}

		if (page > 0)
		{
			JE_char buf[128];

			sprintf(buf, "%s %d", miscText[24], page-topicStart[temp2]+1);
			JE_outText(10, 192, buf, 13, 5);

			sprintf(buf, "%s %d of %d", miscText[25], page, MAX_PAGE);
			JE_outText(220, 192, buf, 13, 5);

			JE_dString(JE_fontCenter(topicName[temp2], SMALL_FONT_SHAPES), 1, topicName[temp2], SMALL_FONT_SHAPES);
		}

		menu = 0;

		helpBoxBrightness = 3;
		verticalHeight = 8;

		switch (page)
		{
			case 0:
				menu = 2;
				if (lastPage == MAX_PAGE)
				{
					menu = TOPICS;
				}
				JE_dString(JE_fontCenter(topicName[0], FONT_SHAPES), 30, topicName[0], FONT_SHAPES);

				do
				{
					for (temp = 1; temp <= TOPICS; temp++)
					{
						char buf[21+1];

						if (temp == menu-1)
						{
							strcpy(buf+1, topicName[temp]);
							buf[0] = '~';
						} else {
							strcpy(buf, topicName[temp]);
						}

						JE_dString(JE_fontCenter(topicName[temp], SMALL_FONT_SHAPES), temp * 20 + 40, buf, SMALL_FONT_SHAPES);
					}

					//JE_waitRetrace();  didn't do anything anyway?
					JE_showVGA();

					tempW = 0;
					JE_textMenuWait(&tempW, false);
					if (newkey)
					{
						switch (lastkey_sym)
						{
							case SDLK_UP:
								menu--;
								if (menu < 2)
								{
									menu = TOPICS;
								}
								JE_playSampleNum(S_CURSOR);
								break;
							case SDLK_DOWN:
								menu++;
								if (menu > TOPICS)
								{
									menu = 2;
								}
								JE_playSampleNum(S_CURSOR);
								break;
							default:
								break;
						}
					}
				} while (!(lastkey_sym == SDLK_ESCAPE || lastkey_sym == SDLK_RETURN));

				if (lastkey_sym == SDLK_RETURN)
				{
					page = topicStart[menu-1];
					JE_playSampleNum(S_CLICK);
				}

				break;
			case 1: /* One-Player Menu */
				JE_HBox(10,  20,  2, 60);
				JE_HBox(10,  50,  5, 60);
				JE_HBox(10,  80, 21, 60);
				JE_HBox(10, 110,  1, 60);
				JE_HBox(10, 140, 28, 60);
				break;
			case 2: /* Two-Player Menu */
				JE_HBox(10,  20,  1, 60);
				JE_HBox(10,  60,  2, 60);
				JE_HBox(10, 100, 21, 60);
				JE_HBox(10, 140, 28, 60);
				break;
			case 3: /* Upgrade Ship */
				JE_HBox(10,  20,  5, 60);
				JE_HBox(10,  70,  6, 60);
				JE_HBox(10, 110,  7, 60);
				break;
			case 4:
				JE_HBox(10,  20,  8, 60);
				JE_HBox(10,  55,  9, 60);
				JE_HBox(10,  87, 10, 60);
				JE_HBox(10, 120, 11, 60);
				JE_HBox(10, 170, 13, 60);
				break;
			case 5:
				JE_HBox(10,  20, 14, 60);
				JE_HBox(10,  80, 15, 60);
				JE_HBox(10, 120, 16, 60);
				break;
			case 6:
				JE_HBox(10,  20, 17, 60);
				JE_HBox(10,  40, 18, 60);
				JE_HBox(10, 130, 20, 60);
				break;
			case 7: /* Options */
				JE_HBox(10,  20, 21, 60);
				JE_HBox(10,  70, 22, 60);
				JE_HBox(10, 110, 23, 60);
				JE_HBox(10, 140, 24, 60);
				break;
			case 8:
				JE_HBox(10,  20, 25, 60);
				JE_HBox(10,  60, 26, 60);
				JE_HBox(10, 100, 27, 60);
				JE_HBox(10, 140, 28, 60);
				JE_HBox(10, 170, 29, 60);
				break;
		}

		helpBoxBrightness = 1;
		verticalHeight = 7;

		lastPage = page;

		if (menu == 0)
		{
			do {
				setjasondelay(3);
				
				push_joysticks_as_keyboard();
				service_SDL_events(true);
				
				JE_showVGA();
				
				wait_delay();
			} while (!newkey && !newmouse);

			wait_noinput(false, true, false);

			if (newmouse)
			{
				switch (lastmouse_but)
				{
					case SDL_BUTTON_LEFT:
						lastkey_sym = SDLK_RIGHT;
						break;
					case SDL_BUTTON_RIGHT:
						lastkey_sym = SDLK_LEFT;
						break;
					case SDL_BUTTON_MIDDLE:
						lastkey_sym = SDLK_ESCAPE;
						break;
				}
				do
				{
					service_SDL_events(false);
				} while (mousedown);
				newkey = true;
			}

			if (newkey)
			{
				switch (lastkey_sym)
				{
					case SDLK_LEFT:
					case SDLK_UP:
					case SDLK_PAGEUP:
						page--;
						JE_playSampleNum(S_CURSOR);
						break;
					case SDLK_RIGHT:
					case SDLK_DOWN:
					case SDLK_PAGEDOWN:
					case SDLK_RETURN:
					case SDLK_SPACE:
						if (page == MAX_PAGE)
						{
							page = 0;
						} else {
							page++;
						}
						JE_playSampleNum(S_CURSOR);
						break;
					case SDLK_F1:
						page = 0;
						JE_playSampleNum(S_CURSOR);
						break;
					default:
						break;
				}
			}
		}

		if (page == 255)
		{
			lastkey_sym = SDLK_ESCAPE;
		}
	} while (lastkey_sym != SDLK_ESCAPE);
}

long weapon_upgrade_cost( long base_cost, unsigned int power )
{
	assert(power <= 11);
	
	unsigned int temp = 0;
	
	for (; power > 0; power--)
		temp += power;
	
	return base_cost * temp;
}

JE_longint JE_getCost( JE_byte itemType, JE_word itemNum )
{
	long cost = 0;
	
	switch (itemType)
	{
	case 2:
		cost = (itemNum > 90) ? 100 : ships[itemNum].cost;
		break;
	case 3:
	case 4:
		cost = weaponPort[itemNum].cost;
		downgradeCost = weapon_upgrade_cost(cost, portPower[itemType-3] - 1);
		upgradeCost = weapon_upgrade_cost(cost, portPower[itemType-3]);
		break;
	case 5:
		cost = shields[itemNum].cost;
		break;
	case 6:
		cost = powerSys[itemNum].cost;
		break;
	case 7:
	case 8:
		cost = options[itemNum].cost;
		break;
	}

	return cost;
}

void JE_loadScreen( void )
{
	JE_boolean quit;
	JE_byte sel, screen, min = 0, max = 0;
	char *tempstr;
	char *tempstr2;
	JE_boolean mal_str = false;
	int len;

	tempstr = NULL;

	free_sprite2s(&shapes6);
	JE_loadCompShapes(&shapes6, '1');  /* Items */

	fade_black(10);
	JE_loadPic(2, false);
	JE_showVGA();
	fade_palette(colors, 10, 0, 255);

	screen = 1;
	sel = 1;
	quit = false;

	memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);

	do
	{
		while (mousedown)
		{
			service_SDL_events(false);
			tempX = mouse_x;
			tempY = mouse_y;
		}

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

		JE_dString(JE_fontCenter(miscText[38 + screen - 1], FONT_SHAPES), 5, miscText[38 + screen - 1], FONT_SHAPES);

		switch (screen)
		{
		case 1:
			min = 1;
			max = 12;
			break;
		case 2:
			min = 12;
			max = 23;
		}

		/* SYN: Go through text line by line */
		for (x = min; x <= max; x++)
		{
			tempY = 30 + (x - min) * 13;

			if (x == max)
			{
				/* Last line is return to main menu, not a save game */
				if (mal_str)
				{
					free(tempstr);
					mal_str = false;
				}
				tempstr = miscText[34 - 1];

				if (x == sel) /* Highlight if selected */
				{
					temp2 = 254;
				} else {
					temp2 = 250;
				}
			} else {
				if (x == sel) /* Highlight if selected */
				{
					temp2 = 254;
				} else {
					temp2 = 250 - ((saveFiles[x - 1].level == 0) << 1);
				}

				if (saveFiles[x - 1].level == 0) /* I think this means the save file is unused */
				{
					if (mal_str)
					{
						free(tempstr);
						mal_str = false;
					}
					tempstr = miscText[3 - 1];
				} else {
					if (mal_str)
					{
						free(tempstr);
						mal_str = false;
					}
					tempstr = saveFiles[x - 1].name;
				}
			}

			/* Write first column text */
			JE_textShade(10, tempY, tempstr, 13, (temp2 % 16) - 8, FULL_SHADE);

			if (x < max) /* Write additional columns for all but the last row */
			{
				if (saveFiles[x - 1].level == 0)
				{
					if (mal_str)
					{
						free(tempstr);
					}
					tempstr = malloc(7);
					mal_str = true;
					strcpy(tempstr, "-----"); /* Unused save slot */
				} else {
					tempstr = saveFiles[x - 1].levelName;
					tempstr2 = malloc(5 + strlen(miscTextB[2-1]));
					sprintf(tempstr2, "%s %d", miscTextB[2-1], saveFiles[x - 1].episode);
					JE_textShade(250, tempY, tempstr2, 5, (temp2 % 16) - 8, FULL_SHADE);
					free(tempstr2);
				}

				len = strlen(miscTextB[3-1]) + 2 + strlen(tempstr);
				tempstr2 = malloc(len);
				sprintf(tempstr2, "%s %s", miscTextB[3 - 1], tempstr);
				JE_textShade(120, tempY, tempstr2, 5, (temp2 % 16) - 8, FULL_SHADE);
				free(tempstr2);
			}

		}

		if (screen == 2)
		{
			blit_sprite2x2(VGAScreen, 90, 180, shapes6, 279);
		}
		if (screen == 1)
		{
			blit_sprite2x2(VGAScreen, 220, 180, shapes6, 281);
		}

		helpBoxColor = 15;
		JE_helpBox(110, 182, miscText[56-1], 25);

		JE_showVGA();

		tempW = 0;
		JE_textMenuWait(&tempW, false);


		if (newkey)
		{
			switch (lastkey_sym)
			{
			case SDLK_UP:
				sel--;
				if (sel < min)
				{
					sel = max;
				}
				JE_playSampleNum(S_CURSOR);
				break;
			case SDLK_DOWN:
				sel++;
				if (sel > max)
				{
					sel = min;
				}
				JE_playSampleNum(S_CURSOR);
				break;
			case SDLK_LEFT:
			case SDLK_RIGHT:
				if (screen == 1)
				{
					screen = 2;
					sel += 11;
				} else {
					screen = 1;
					sel -= 11;
				}
				break;
			case SDLK_RETURN:
				if (sel < max)
				{
					if (saveFiles[sel - 1].level > 0)
					{
						JE_playSampleNum (S_SELECT);
						performSave = false;
						JE_operation(sel);
						quit = true;
					} else {
						JE_playSampleNum (S_CLINK);
					}
				} else {
					quit = true;
				}


				break;
			case SDLK_ESCAPE:
				quit = true;
				break;
			default:
				break;
			}

		}
	} while (!quit);
}

JE_longint JE_totalScore( JE_longint score, JE_PItemsType pItems )
{
	long temp = score;
	
	temp += JE_getValue(2, pItems[P_SHIP]);
	temp += JE_getValue(3, pItems[P_FRONT]);
	temp += JE_getValue(4, pItems[P_REAR]);
	temp += JE_getValue(5, pItems[P_SHIELD]);
	temp += JE_getValue(6, pItems[P_GENERATOR]);
	temp += JE_getValue(7, pItems[P_LEFT_SIDEKICK]);
	temp += JE_getValue(8, pItems[P_RIGHT_SIDEKICK]);
	
	return temp;
}

JE_longint JE_getValue( JE_byte itemType, JE_word itemNum )
{
	long value = 0;
	
	switch (itemType)
	{
	case 2:
		value = ships[itemNum].cost;
		break;
	case 3:
	case 4:;
		long base_value = weaponPort[itemNum].cost;
		value = base_value;
		for (unsigned int i = 1; i < portPower[itemType-3]; i++)
			value += weapon_upgrade_cost(base_value, i);
		break;
	case 5:
		value = shields[itemNum].cost;
		break;
	case 6:
		value = powerSys[itemNum].cost;
		break;
	case 7:
	case 8:
		value = options[itemNum].cost;
		break;
	}
	
	return value;
}

void JE_nextEpisode( void )
{
	strcpy(lastLevelName, "Completed");
	
	if (episodeNum == pItems[P_EPISODE] && !gameHasRepeated && episodeNum != 4 &&
	    !isNetworkGame && !constantPlay)
	{
		JE_highScoreCheck();
	}
	
	unsigned int newEpisode = JE_findNextEpisode();
	
	if (jumpBackToEpisode1)
	{
		// shareware version check
		if (episodeNum == 1 &&
			!isNetworkGame && !constantPlay)
		{
			// JE_loadOrderingInfo();
		}
		
		if (episodeNum > 2 &&
			!constantPlay)
		{
			JE_playCredits();
		}
		
		// randomly give player the SuperCarrot
		if ((mt_rand() % 6) == 0)
		{
			pItems[P_FRONT] = 23;
			pItems[P_REAR] = 24;
			pItems[P_SHIP] = 2;
			portPower[1-1] = 1;
			portPower[2-1] = 1;
			pItemsPlayer2[P_REAR] = 24;
			memcpy(pItemsBack2, pItems, sizeof(pItemsBack2));
		}
	}
	
	if (newEpisode != episodeNum)
		JE_initEpisode(newEpisode);
		
	gameLoaded = true;
	mainLevel = FIRST_LEVEL;
	saveLevel = FIRST_LEVEL;
	
	play_song(26);
	
	JE_clr256();
	memcpy(colors, palettes[6-1], sizeof(colors));
	
	tempScreenSeg = VGAScreen;
	
	JE_dString(JE_fontCenter(episode_name[episodeNum], SMALL_FONT_SHAPES), 130, episode_name[episodeNum], SMALL_FONT_SHAPES);
	
	JE_dString(JE_fontCenter(miscText[5-1], SMALL_FONT_SHAPES), 185, miscText[5-1], SMALL_FONT_SHAPES);
	
	JE_showVGA();
	fade_palette(colors, 15, 0, 255);
	
	JE_wipeKey();
	if (!constantPlay)
	{
		do
		{
			NETWORK_KEEP_ALIVE();
			
			SDL_Delay(16);
		} while (!JE_anyButton());
	}
	
	fade_black(15);
}

void JE_initPlayerData( void )
{
	/* JE: New Game Items/Data */
	
	pItems[P_SHIP] = 1;            // USP Talon
	pItems[P_FRONT] = 1;           // Pulse-Cannon
	pItems[P_REAR] = 0;            // None
	pItems[P_SHIELD] = 4;          // Gencore High Energy Shield
	pItems[P_GENERATOR] = 2;       // Advanced MR-12
	pItems[P_LEFT_SIDEKICK] = 0;   // None
	pItems[P_RIGHT_SIDEKICK] = 0;  // None
	pItems[P_SPECIAL] = 0;         // None
	pItems[P2_SIDEKICK_MODE] = 2;  // not sure
	pItems[P2_SIDEKICK_TYPE] = 1;  // not sure
	
	pItems[P_EPISODE] = 0;            // initial episode number
	
	memcpy(pItemsBack2, pItems, sizeof(pItems));
	
	memcpy(pItemsPlayer2, pItems, sizeof(pItems));
	pItemsPlayer2[P_REAR] = 15;             // Vulcan Cannon
	pItemsPlayer2[P_LEFT_SIDEKICK] = 0;     // None
	pItemsPlayer2[P_RIGHT_SIDEKICK] = 0;    // None
	pItemsPlayer2[P2_SIDEKICK_MODE] = 101;  // 101, 102, 103
	pItemsPlayer2[P2_SIDEKICK_TYPE] = 0;    // None
	
	gameHasRepeated = false;
	onePlayerAction = false;
	superArcadeMode = SA_NONE;
	superTyrian = false;
	twoPlayerMode = false;
	
	secretHint = (mt_rand() % 3) + 1;
	
	armorLevel = ships[pItems[P_SHIP]].dmg;
	
	portPower[0] = 1;
	portPower[1] = 1;
	
	portConfig[0] = 1;
	portConfig[1] = 1;
	
	mainLevel = FIRST_LEVEL;
	saveLevel = FIRST_LEVEL;
	
	strcpy(lastLevelName, miscText[19]);
}

void JE_sortHighScores( void )
{
	JE_byte x;
	
	temp = 0;
	for (x = 0; x < 6; x++)
	{
		JE_sort();
		temp += 3;
	}
}

void JE_highScoreScreen( void )
{
	int min = 1;
	int max = 3;

	int x, z;
	short int chg;
	int quit;
	char scoretemp[32];
	
	free_sprite2s(&shapes6);
	JE_loadCompShapes(&shapes6, '1');  /* Items */
	
	fade_black(10);
	JE_loadPic(2, false);
	JE_showVGA();
	fade_palette(colors, 10, 0, 255);
	tempScreenSeg = VGAScreen;
	
	quit = false;
	x = 1;
	chg = 1;

	memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);

	do
	{
		if (episodeAvail[x-1])
		{
			memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);

			JE_dString(JE_fontCenter(miscText[51 - 1], FONT_SHAPES), 03, miscText[51 - 1], FONT_SHAPES);
			JE_dString(JE_fontCenter(episode_name[x], SMALL_FONT_SHAPES), 30, episode_name[x], SMALL_FONT_SHAPES);

			/* Player 1 */
			temp = (x * 6) - 6;

			JE_dString(JE_fontCenter(miscText[47 - 1], SMALL_FONT_SHAPES), 55, miscText[47 - 1], SMALL_FONT_SHAPES);

			for (z = 0; z < 3; z++)
			{
				temp5 = saveFiles[temp + z].highScoreDiff;
				if (temp5 > 9)
				{
					saveFiles[temp + z].highScoreDiff = 0;
					temp5 = 0;
				}
				sprintf(scoretemp, "~#%d:~ %d", z + 1, saveFiles[temp+z].highScore1);
				JE_textShade(250, ((z+1) * 10) + 65 , difficultyNameB[temp5], 15, temp5 + ((JE_byte) (temp5 == 0)) - 1, FULL_SHADE);
				JE_textShade(20, ((z+1) * 10) + 65 , scoretemp, 15, 0, FULL_SHADE);
				JE_textShade(110, ((z+1) * 10) + 65 , saveFiles[temp + z].highScoreName, 15, 2, FULL_SHADE);
			}

			/* Player 2 */
			temp += 3;

			JE_dString( JE_fontCenter( miscText[48 - 1], SMALL_FONT_SHAPES), 120, miscText[48 - 1], SMALL_FONT_SHAPES);

			/*{        textshade(20,125,misctext[49],15,3,_FullShade);
			  textshade(80,125,misctext[50],15,3,_FullShade);}*/

			for (z = 0; z < 3; z++)
			{
				temp5 = saveFiles[temp + z].highScoreDiff;
				if (temp5 > 9)
				{
					saveFiles[temp + z].highScoreDiff = 0;
					temp5 = 0;
				}
				sprintf(scoretemp, "~#%d:~ %d", z + 1, saveFiles[temp+z].highScore1); /* Not .highScore2 for some reason */
				JE_textShade(250, ((z+1) * 10) + 125 , difficultyNameB[temp5], 15, temp5 + ((JE_byte) (temp5 == 0)) - 1, FULL_SHADE);
				JE_textShade(20, ((z+1) * 10) + 125 , scoretemp, 15, 0, FULL_SHADE);
				JE_textShade(110, ((z+1) * 10) + 125 , saveFiles[temp + z].highScoreName, 15, 2, FULL_SHADE);
			}

			if (x > 1)
			{
				blit_sprite2x2(VGAScreen,  90, 180, shapes6, 279);
			}

			if ( ( (x < 2) && episodeAvail[2-1] ) || ( (x < 3) && episodeAvail[3-1] ) )
			{
				blit_sprite2x2(VGAScreen,  220, 180, shapes6, 281);
			}

			helpBoxColor = 15;
			JE_helpBox(110, 182, miscText[57 - 1], 25);

			/* {Dstring(fontcenter(misctext[57],_SmallFontShapes),190,misctext[57],_SmallFontShapes);} */

			JE_showVGA();

			tempW = 0;
			JE_textMenuWait(&tempW, false);

			if (newkey)
			{
				switch (lastkey_sym)
				{
				case SDLK_LEFT:
					x--;
					chg = -1;
					break;
				case SDLK_RIGHT:
					x++;
					chg = 1;
					break;
				default:
					break;
				}
			}

		} else {
			x += chg;
		}

		x = ( x < min ) ? max : ( x > max ) ? min : x;

		if (newkey)
		{
			switch (lastkey_sym)
			{
			case SDLK_RETURN:
			case SDLK_ESCAPE:
				quit = true;
				break;
			default:
				break;
			}
		}

	} while (!quit);

}

void JE_gammaCorrect_func( JE_byte *col, JE_real r )
{
	int temp = roundf(*col * r);
	if (temp > 255)
	{
		temp = 255;
	}
	*col = temp;
}

void JE_gammaCorrect( Palette *colorBuffer, JE_byte gamma )
{
	int x;
	JE_real r = 1 + (JE_real)gamma / 10;
	
	for (x = 0; x < 256; x++)
	{
		JE_gammaCorrect_func(&(*colorBuffer)[x].r, r);
		JE_gammaCorrect_func(&(*colorBuffer)[x].g, r);
		JE_gammaCorrect_func(&(*colorBuffer)[x].b, r);
	}
}

JE_boolean JE_gammaCheck( void )
{
	Uint8 temp = keysactive[SDLK_F11];
	if (temp)
	{
		keysactive[SDLK_F11] = false;
		newkey = false;
		gammaCorrection = (gammaCorrection + 1) % 4;
		memcpy(colors, palettes[pcxpal[3-1]], sizeof(colors));
		JE_gammaCorrect(&colors, gammaCorrection);
		set_palette(colors, 0, 255);
	}
	return temp;
}

void JE_doInGameSetup( void )
{
	haltGame = false;
	
	if (isNetworkGame)
	{
		network_prepare(PACKET_GAME_MENU);
		network_send(4);  // PACKET_GAME_MENU
		
		while (true)
		{
			service_SDL_events(false);
			
			if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_GAME_MENU)
			{
				network_update();
				break;
			}
			
			network_update();
			network_check();
			
			SDL_Delay(16);
		}
	}
	
	if (yourInGameMenuRequest)
	{
		if (JE_inGameSetup())
		{
			reallyEndLevel = true;
			playerEndLevel = true;
		}
		quitRequested = false;
		
		keysactive[SDLK_ESCAPE] = false;
		
		if (isNetworkGame)
		{
			if (!playerEndLevel)
			{
				network_prepare(PACKET_WAITING);
				network_send(4);  // PACKET_WAITING
			} else {
				network_prepare(PACKET_GAME_QUIT);
				network_send(4);  // PACKET_GAMEQUIT
			}
		}
	}
	
	if (isNetworkGame)
	{
		SDL_Surface *temp_surface = VGAScreen;
		VGAScreen = VGAScreenSeg; /* side-effect of game_screen */
		
		if (!yourInGameMenuRequest)
		{
			JE_barShade(3, 60, 257, 80); /*Help Box*/
			JE_barShade(5, 62, 255, 78);
			tempScreenSeg = VGAScreen;
			JE_dString(10, 65, "Other player in options menu.", SMALL_FONT_SHAPES);
			JE_showVGA();
			
			while (true)
			{
				service_SDL_events(false);
				JE_showVGA();
				
				if (packet_in[0])
				{
					if (SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_WAITING)
					{
						network_check();
						break;
					} else if (SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_GAME_QUIT) {
						reallyEndLevel = true;
						playerEndLevel = true;
						
						network_check();
						break;
					}
				}
				
				network_update();
				network_check();
				
				SDL_Delay(16);
			}
		} else {
			/*
			JE_barShade(3, 160, 257, 180); /-*Help Box*-/
			JE_barShade(5, 162, 255, 178);
			tempScreenSeg = VGAScreen;
			JE_dString(10, 165, "Waiting for other player.", SMALL_FONT_SHAPES);
			JE_showVGA();
			*/
		}
		
		while (!network_is_sync())
		{
			service_SDL_events(false);
			JE_showVGA();
			
			network_check();
			SDL_Delay(16);
		}
		
		VGAScreen = temp_surface; /* side-effect of game_screen */
	}
	
	yourInGameMenuRequest = false;
	
	tempScreenSeg = VGAScreen;
	//skipStarShowVGA = true;
}

JE_boolean JE_inGameSetup( void )
{
	SDL_Surface *temp_surface = VGAScreen;
	VGAScreen = VGAScreenSeg; /* side-effect of game_screen */
	
	JE_boolean returnvalue = false;
	
	const JE_byte help[6] /* [1..6] */ = {15, 15, 28, 29, 26, 27};
	JE_byte  sel;
	JE_boolean quit;
	
	bool first = true;

	tempScreenSeg = VGAScreenSeg; /* <MXD> ? */
	
	quit = false;
	sel = 1;
	
	JE_barShade(3, 13, 217, 137); /*Main Box*/
	JE_barShade(5, 15, 215, 135);
	
	JE_barShade(3, 143, 257, 157); /*Help Box*/
	JE_barShade(5, 145, 255, 155);
	memcpy(VGAScreen2->pixels, VGAScreen->pixels, VGAScreen2->pitch * VGAScreen2->h);
	
	do
	{
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, VGAScreen->pitch * VGAScreen->h);
		
		for (x = 0; x < 6; x++)
		{
			JE_outTextAdjust(10, (x + 1) * 20, inGameText[x], 15, ((sel == x+1) << 1) - 4, SMALL_FONT_SHAPES, true);
		}
		
		JE_outTextAdjust(120, 3 * 20, detailLevel[processorType-1], 15, ((sel == 3) << 1) - 4, SMALL_FONT_SHAPES, true);
		JE_outTextAdjust(120, 4 * 20, gameSpeedText[gameSpeed-1],   15, ((sel == 4) << 1) - 4, SMALL_FONT_SHAPES, true);
		
		JE_outTextAdjust(10, 147, mainMenuHelp[help[sel-1]-1], 14, 6, TINY_FONT, true);
		
		JE_barDrawShadow(120, 20, 1, 16, tyrMusicVolume / 12, 3, 13);
		JE_barDrawShadow(120, 40, 1, 16, fxVolume / 12, 3, 13);
		
		JE_showVGA();
		
		if (first)
		{
			first = false;
			wait_noinput(false, false, true); // TODO: should up the joystick repeat temporarily instead
		}
		
		tempW = 0;
		JE_textMenuWait(&tempW, true);
		
		if (inputDetected)
		{
			switch (lastkey_sym)
			{
				case SDLK_RETURN:
					JE_playSampleNum(S_SELECT);
					switch (sel)
					{
						case 1:
						case 2:
						case 3:
						case 4:
							sel = 5;
							break;
						case 5:
							quit = true;
							break;
						case 6:
							returnvalue = true;
							quit = true;
							if (constantPlay)
							{
								JE_tyrianHalt(0);
							}
							
							if (isNetworkGame)
							{ /*Tell other computer to exit*/
								haltGame = true;
								playerEndLevel = true;
							}
							break;
					}
					break;
				case SDLK_ESCAPE:
					quit = true;
					JE_playSampleNum(S_SPRING);
					break;
				case SDLK_UP:
					if (--sel < 1)
					{
						sel = 6;
					}
					JE_playSampleNum(S_CURSOR);
					break;
				case SDLK_DOWN:
					if (++sel > 6)
					{
						sel = 1;
					}
					JE_playSampleNum(S_CURSOR);
					break;
				case SDLK_LEFT:
					switch (sel)
					{
						case 1:
							JE_changeVolume(&tyrMusicVolume, -12, &fxVolume, 0);
							if (music_disabled)
							{
								music_disabled = false;
								restart_song();
							}
							break;
						case 2:
							JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, -12);
							samples_disabled = false;
							break;
						case 3:
							if (--processorType < 1)
							{
								processorType = 4;
							}
							JE_initProcessorType();
							JE_setNewGameSpeed();
							break;
						case 4:
							if (--gameSpeed < 1)
							{
								gameSpeed = 5;
							}
							JE_initProcessorType();
							JE_setNewGameSpeed();
							break;
					}
					if (sel < 5)
					{
						JE_playSampleNum(S_CURSOR);
					}
					break;
				case SDLK_RIGHT:
					switch (sel)
					{
						case 1:
							JE_changeVolume(&tyrMusicVolume, 12, &fxVolume, 0);
							if (music_disabled)
							{
								music_disabled = false;
								restart_song();
							}
							break;
						case 2:
							JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, 12);
							samples_disabled = false;
							break;
						case 3:
							if (++processorType > 4)
							{
								processorType = 1;
							}
							JE_initProcessorType();
							JE_setNewGameSpeed();
							break;
						case 4:
							if (++gameSpeed > 5)
							{
								gameSpeed = 1;
							}
							JE_initProcessorType();
							JE_setNewGameSpeed();
							break;
					}
					if (sel < 5)
					{
						JE_playSampleNum(S_CURSOR);
					}
					break;
				case SDLK_w:
					if (sel == 3)
					{
						processorType = 6;
						JE_initProcessorType();
					}
				default:
					break;
			}
		}
		
	} while (!(quit || haltGame));
	
	VGAScreen = temp_surface; /* side-effect of game_screen */
	
	return returnvalue;
}

void JE_inGameHelp( void )
{
	SDL_Surface *temp_surface = VGAScreen;
	VGAScreen = VGAScreenSeg; /* side-effect of game_screen */
	
	tempScreenSeg = VGAScreenSeg;
	
	JE_clearKeyboard();
	JE_wipeKey();
	
	JE_barShade(1, 1, 262, 182); /*Main Box*/
	JE_barShade(3, 3, 260, 180);
	JE_barShade(5, 5, 258, 178);
	JE_barShade(7, 7, 256, 176);
	JE_bar     (9, 9, 254, 174, 0);
	
	if (twoPlayerMode)  // Two-Player Help
	{
		helpBoxColor = 3;
		helpBoxBrightness = 3;
		JE_HBox(20,  4, 36, 50);
		
		// weapon help
		blit_sprite(VGAScreenSeg, 2, 21, OPTION_SHAPES, 43);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(55, 20, 37, 40);
		
		// sidekick help
		blit_sprite(VGAScreenSeg, 5, 36, OPTION_SHAPES, 41);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(40, 43, 34, 44);
		
		// sheild/armor help
		blit_sprite(VGAScreenSeg, 2, 79, OPTION_SHAPES, 42);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(54, 84, 35, 40);
		
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(5, 126, 38, 55);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(5, 160, 39, 55);
	}
	else
	{
		// power bar help
		blit_sprite(VGAScreenSeg, 15, 5, OPTION_SHAPES, 40);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(40, 10, 31, 45);
		
		// weapon help
		blit_sprite(VGAScreenSeg, 5, 37, OPTION_SHAPES, 39);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(40, 40, 32, 44);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(40, 60, 33, 44);
		
		// sidekick help
		blit_sprite(VGAScreenSeg, 5, 98, OPTION_SHAPES, 41);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(40, 103, 34, 44);
		
		// shield/armor help
		blit_sprite(VGAScreenSeg, 2, 138, OPTION_SHAPES, 42);
		helpBoxColor = 5;
		helpBoxBrightness = 3;
		JE_HBox(54, 143, 35, 40);
	}
	
	// "press a key"
	blit_sprite(VGAScreenSeg, 16, 189, OPTION_SHAPES, 36);  // in-game text area
	JE_outText(120 - JE_textWidth(miscText[5-1], TINY_FONT) / 2 + 20, 190, miscText[5-1], 0, 4);
	
	JE_showVGA();
	
	do
	{
		tempW = 0;
		JE_textMenuWait(&tempW, true);
	}
	while (!inputDetected);
	
	textErase = 1;
	
	VGAScreen = temp_surface;
}

void JE_highScoreCheck( void )
{
	Sint32 temp_score;
	
	for (int temp_p = 0; temp_p < (twoPlayerMode ? 2 : 1); ++temp_p)
	{
		JE_sortHighScores();
		
		int p = temp_p;
		
		if (twoPlayerMode)
		{
			// ask for the highest scorer first
			if (score < score2)
				p = (temp_p == 0) ? 1 : 0;
			
			temp_score = (p == 0) ? score : score2;
		}
		else
		{
			// single player highscore includes cost of upgrades
			temp_score = JE_totalScore(score, pItems);
		}
		
		int slot;
		const int first_slot = (pItems[P_EPISODE] - 1) * 6 + (twoPlayerMode ? 3 : 0),
		          slot_limit = first_slot + 3;
		
		for (slot = first_slot; slot < slot_limit; ++slot)
		{
			if (temp_score > saveFiles[slot].highScore1)
				break;
		}
		
		// did you get a high score?
		if (slot < slot_limit)
		{
			// shift down old scores
			for (int i = slot_limit - 1; i > slot; --i)
			{
				saveFiles[i].highScore1 = saveFiles[i - 1].highScore1;
				strcpy(saveFiles[i].highScoreName, saveFiles[i - 1].highScoreName);
			}
			
			wait_noinput(false, true, false);
			
			JE_clr256();
			JE_showVGA();
			memcpy(colors, palettes[0], sizeof(colors));
			
			play_song(33);
			
			{
				/* Enter Thy name */
				
				JE_byte flash = 8 * 16 + 10;
				JE_boolean fadein = true;
				JE_boolean quit = false, cancel = false;
				char stemp[30], tempstr[30];
				char buffer[256];
				
				strcpy(stemp, "                             ");
				temp = 0;
				
				JE_barShade(65, 55, 255, 155);
				
				do
				{
					service_SDL_events(true);
					
					JE_dString(JE_fontCenter(miscText[51], FONT_SHAPES), 3, miscText[51], FONT_SHAPES);
					
					temp3 = twoPlayerMode ? 58 + p : 53;
					
					JE_dString(JE_fontCenter(miscText[temp3-1], SMALL_FONT_SHAPES), 30, miscText[temp3-1], SMALL_FONT_SHAPES);
					
					blit_sprite(VGAScreenSeg, 50, 50, OPTION_SHAPES, 35);  // message box
					
					if (twoPlayerMode)
					{
						sprintf(buffer, "%s %s", miscText[48 + p], miscText[53]);
						JE_textShade(60, 55, buffer, 11, 4, FULL_SHADE);
					}
					else
					{
						JE_textShade(60, 55, miscText[53], 11, 4, FULL_SHADE);
					}
					
					sprintf(buffer, "%s %d", miscText[37], temp_score);
					JE_textShade(70, 70, buffer, 11, 4, FULL_SHADE);
					
					do
					{
						flash = (flash == 8 * 16 + 10) ? 8 * 16 + 2 : 8 * 16 + 10;
						temp3 = (temp3 == 6) ? 2 : 6;
						
						strncpy(tempstr, stemp, temp);
						tempstr[temp] = '\0';
						JE_outText(65, 89, tempstr, 8, 3);
						tempW = 65 + JE_textWidth(tempstr, TINY_FONT);
						JE_barShade(tempW + 2, 90, tempW + 6, 95);
						JE_bar(tempW + 1, 89, tempW + 5, 94, flash);
						
						for (int i = 0; i < 14; i++)
						{
							setjasondelay(1);
							
							JE_mouseStart();
							JE_showVGA();
							if (fadein)
							{
								fade_palette(colors, 15, 0, 255);
								fadein = false;
							}
							JE_mouseReplace();
							
							push_joysticks_as_keyboard();
							service_wait_delay();
							
							if (newkey || newmouse)
								break;
						}
						
					} while (!newkey && !newmouse);
					
					if (!playing)
						play_song(31);
					
					if (mouseButton > 0)
					{
						if (mouseX > 56 && mouseX < 142 && mouseY > 123 && mouseY < 149)
						{
							quit = true;
						}
						else if (mouseX > 151 && mouseX < 237 && mouseY > 123 && mouseY < 149)
						{
							quit = true;
							cancel = true;
						}
					}
					else if (newkey)
					{
						bool validkey = false;
						lastkey_char = toupper(lastkey_char);
						switch(lastkey_char)
						{
							case ' ':
							case '-':
							case '.':
							case ',':
							case ':':
							case '!':
							case '?':
							case '#':
							case '@':
							case '$':
							case '%':
							case '*':
							case '(':
							case ')':
							case '/':
							case '=':
							case '+':
							case '<':
							case '>':
							case ';':
							case '"':
							case '\'':
								validkey = true;
							default:
								if (temp < 28 && (validkey || (lastkey_char >= 'A' && lastkey_char <= 'Z') || (lastkey_char >= '0' && lastkey_char <= '9')))
								{
									stemp[temp] = lastkey_char;
									temp++;
								}
								break;
							case SDLK_BACKSPACE:
							case SDLK_DELETE:
								if (temp)
								{
									temp--;
									stemp[temp] = ' ';
								}
								break;
							case SDLK_ESCAPE:
								quit = true;
								cancel = true;
								break;
							case SDLK_RETURN:
								quit = true;
								break;
						}
					}
				}
				while (!quit);
				
				if (!cancel)
				{
					saveFiles[slot].highScore1 = temp_score;
					strcpy(saveFiles[slot].highScoreName, stemp);
					saveFiles[slot].highScoreDiff = difficultyLevel;
				}
				
				fade_black(15);
				JE_loadPic(2, false);
				
				JE_dString(JE_fontCenter(miscText[50], FONT_SHAPES), 10, miscText[50], FONT_SHAPES);
				JE_dString(JE_fontCenter(episode_name[episodeNum], SMALL_FONT_SHAPES), 35, episode_name[episodeNum], SMALL_FONT_SHAPES);
				
				for (int i = first_slot; i < slot_limit; ++i)
				{
					if (i != slot)
					{
						sprintf(buffer, "~#%d:~  %d", (i - first_slot + 1), saveFiles[i].highScore1);
						JE_textShade( 20, ((i - first_slot + 1) * 12) + 65, buffer, 15, 0, FULL_SHADE);
						JE_textShade(150, ((i - first_slot + 1) * 12) + 65, saveFiles[i].highScoreName, 15, 2, FULL_SHADE);
					}
				}
				
				JE_showVGA();
				
				fade_palette(colors, 15, 0, 255);
				
				sprintf(buffer, "~#%d:~  %d", (slot - first_slot + 1), saveFiles[slot].highScore1);
				
				frameCountMax = 6;
				textGlowFont = TINY_FONT;
				
				textGlowBrightness = 10;
				JE_outTextGlow( 20, (slot - first_slot + 1) * 12 + 65, buffer);
				textGlowBrightness = 10;
				JE_outTextGlow(150, (slot - first_slot + 1) * 12 + 65, saveFiles[slot].highScoreName);
				textGlowBrightness = 10;
				JE_outTextGlow(JE_fontCenter(miscText[4], TINY_FONT), 180, miscText[4]);
				
				JE_showVGA();
				
				if (frameCountMax != 0)
					wait_input(true, true, true);
				
				fade_black(15);
			}
			
		}
	}
}

void JE_changeDifficulty( void )
{
	JE_byte newDifficultyLevel;
	JE_longint temp;

	if (twoPlayerMode)
	{
		temp = score + score2;
	} else {
		temp = JE_totalScore(score, pItems);
	}

	switch (initialDifficulty)
	{
		case 1:
			temp = roundf(temp * 0.4f);
			break;
		case 2:
			temp = roundf(temp * 0.8f);
			break;
		case 3:
			temp = roundf(temp * 1.3f);
			break;
		case 4:
			temp = roundf(temp * 1.6f);
			break;
		case 5:
		case 6:
			temp = roundf(temp * 2);
			break;
		case 7:
		case 8:
		case 9:
			temp = roundf(temp * 3);
			break;
	}

	if (twoPlayerMode)
	{
		if (temp < 10000)
		{
			newDifficultyLevel = 1; /* Easy */
		} else if (temp < 20000) {
			newDifficultyLevel = 2; /* Normal */
		} else if (temp < 50000) {
			newDifficultyLevel = 3; /* Hard */
		} else if (temp < 80000) {
			newDifficultyLevel = 4; /* Impossible */
		} else if (temp < 125000) {
			newDifficultyLevel = 5; /* Impossible B */
		} else if (temp < 200000) {
			newDifficultyLevel = 6; /* Suicide */
		} else if (temp < 400000) {
			newDifficultyLevel = 7; /* Maniacal */
		} else if (temp < 600000) {
			newDifficultyLevel = 8; /* Zinglon */
		} else {
			newDifficultyLevel = 9; /* Nortaneous */
		}
	} else {
		if (temp < 40000)
		{
			newDifficultyLevel = 1; /* Easy */
		} else if (temp < 70000) {
			newDifficultyLevel = 2; /* Normal */
		} else if (temp < 150000) {
			newDifficultyLevel = 3; /* Hard */
		} else if (temp < 300000) {
			newDifficultyLevel = 4; /* Impossible */
		} else if (temp < 600000) {
			newDifficultyLevel = 5; /* Impossible B */
		} else if (temp < 1000000) {
			newDifficultyLevel = 6; /* Suicide */
		} else if (temp < 2000000) {
			newDifficultyLevel = 7; /* Maniacal */
		} else if (temp < 3000000) {
			newDifficultyLevel = 8; /* Zinglon */
		} else {
			newDifficultyLevel = 9; /* Nortaneous */
		}
	}

	if (newDifficultyLevel > difficultyLevel)
	{
		difficultyLevel = newDifficultyLevel;
	}

}

bool load_next_demo( void )
{
	if (++demo_num > 5)
		demo_num = 1;
	
	char demo_filename[9];
	snprintf(demo_filename, sizeof(demo_filename), "demo.%d", demo_num);
	demo_file = dir_fopen_die(data_dir(), demo_filename, "rb"); // TODO: only play demos from existing file (instead of dying)
	
	difficultyLevel = 2;
	bonusLevelCurrent = false;
	
	Uint8 temp = fgetc(demo_file);
	JE_initEpisode(temp);
	efread(levelName, 1, 10, demo_file); levelName[10] = '\0';
	lvlFileNum = fgetc(demo_file);
	efread(pItems, sizeof(Uint8), 12, demo_file);
	efread(portPower, sizeof(Uint8), 5, demo_file);
	levelSong = fgetc(demo_file);
	
	demo_keys_wait = 0;
	demo_keys = next_demo_keys = 0;
	
	printf("loaded demo '%s'\n", demo_filename);
	
	return true;
}

bool replay_demo_keys( void )
{
	if (demo_keys_wait == 0)
		if (read_demo_keys() == false)
			return false; // no more keys
	
	if (demo_keys_wait > 0)
		demo_keys_wait--;
	
	if (demo_keys & (1 << 0))
		PY -= CURRENT_KEY_SPEED;
	if (demo_keys & (1 << 1))
		PY += CURRENT_KEY_SPEED;
	
	if (demo_keys & (1 << 2))
		PX -= CURRENT_KEY_SPEED;
	if (demo_keys & (1 << 3))
		PX += CURRENT_KEY_SPEED;
	
	button[0] = (bool)(demo_keys & (1 << 4));
	button[3] = (bool)(demo_keys & (1 << 5));
	button[1] = (bool)(demo_keys & (1 << 6));
	button[2] = (bool)(demo_keys & (1 << 7));
	
	return true;
}

bool read_demo_keys( void )
{
	demo_keys = next_demo_keys;
	
	efread(&demo_keys_wait, sizeof(Uint16), 1, demo_file);
	demo_keys_wait = SDL_Swap16(demo_keys_wait);
	
	next_demo_keys = getc(demo_file);
	
	return !feof(demo_file);
}

/*Street Fighter codes*/
void JE_SFCodes( JE_byte playerNum_, JE_integer PX_, JE_integer PY_, JE_integer mouseX_, JE_integer mouseY_, JE_PItemsType pItems_ )
{
	JE_byte temp, temp2, temp3, temp4, temp5;
	
	/*Get direction*/
	tempW = pItems_[P_SHIP]; // Get player ship
	if (playerNum_ == 2 && tempW < 15)
	{
		tempW = 0;
	}
	
	if (tempW < 15)
	{
		
		temp2 = (mouseY_ > PY_) +    /*UP*/
		        (mouseY_ < PY_) +    /*DOWN*/
		        (PX_ < mouseX_) +    /*LEFT*/
		        (PX_ > mouseX_);     /*RIGHT*/
		temp = (mouseY_ > PY_) * 1 + /*UP*/
		       (mouseY_ < PY_) * 2 + /*DOWN*/
		       (PX_ < mouseX_) * 3 + /*LEFT*/
		       (PX_ > mouseX_) * 4;  /*RIGHT*/
		
		if (temp == 0) // no direction being pressed
		{
			if (!button[0]) // if fire button is released
			{
				temp = 9;
				temp2 = 1;
			} else {
				temp2 = 0;
				temp = 99;
			}
		}
		
		if (temp2 == 1) // if exactly one direction pressed or firebutton is released
		{
			temp += button[0] * 4;
			
			temp3 = superTyrian ? 21 : 3;
			for (temp2 = 0; temp2 < temp3; temp2++)
			{
				
				/*Use SuperTyrian ShipCombos or not?*/
				temp5 = superTyrian ? shipCombosB[temp2] : shipCombos[tempW][temp2];
				
				// temp5 == selected combo in ship
				if (temp5 == 0) /* combo doesn't exists */
				{
					// mark twiddles as cancelled/finished
					SFCurrentCode[playerNum_-1][temp2] = 0;
				} else {
					// get next combo key
					temp4 = keyboardCombos[temp5-1][SFCurrentCode[playerNum_-1][temp2]];
					
					// correct key
					if (temp4 == temp)
					{
						SFCurrentCode[playerNum_-1][temp2]++;
						
						temp4 = keyboardCombos[temp5-1][SFCurrentCode[playerNum_-1][temp2]];
						if (temp4 > 100 && temp4 <= 100 + SPECIAL_NUM)
						{
							SFCurrentCode[playerNum_-1][temp2] = 0;
							SFExecuted[playerNum_-1] = temp4 - 100;
						}
					} else {
						if ((temp != 9) &&
						    (temp4 - 1) % 4 != (temp - 1) % 4 &&
						    (SFCurrentCode[playerNum_-1][temp2] == 0 ||
						     keyboardCombos[temp5-1][SFCurrentCode[playerNum_-1][temp2]-1] != temp))
						{
							SFCurrentCode[playerNum_-1][temp2] = 0;
						}
					}
				}
			}
		}
		
	}
}

void JE_sort( void )
{
	JE_byte a, b;

	for (a = 0; a < 2; a++)
	{
		for (b = a + 1; b < 3; b++)
		{
			if (saveFiles[temp + a].highScore1 < saveFiles[temp + b].highScore1)
			{
				JE_longint tempLI;
				char tempStr[30];
				JE_byte tempByte;

				tempLI = saveFiles[temp + a].highScore1;
				saveFiles[temp + a].highScore1 = saveFiles[temp + b].highScore1;
				saveFiles[temp + b].highScore1 = tempLI;

				strcpy(tempStr, saveFiles[temp + a].highScoreName);
				strcpy(saveFiles[temp + a].highScoreName, saveFiles[temp + b].highScoreName);
				strcpy(saveFiles[temp + b].highScoreName, tempStr);

				tempByte = saveFiles[temp + a].highScoreDiff;
				saveFiles[temp + a].highScoreDiff = saveFiles[temp + b].highScoreDiff;
				saveFiles[temp + b].highScoreDiff = tempByte;
			}
		}
	}
}

JE_boolean JE_getPassword( void )
{
	STUB();
	return false;
}

void JE_playCredits( void )
{
	const int maxlines = 132;
	typedef char JE_CreditStringType[maxlines][66];
	
	JE_CreditStringType credstr;
	JE_word x, max = 0, maxlen = 0;
	JE_integer curpos, newpos;
	JE_byte yloc;
	JE_byte currentpic = 1, fade = 0;
	JE_shortint fadechg = 1;
	JE_byte currentship = 0;
	JE_integer shipx = 0, shipxwait = 0;
	JE_shortint shipxc = 0, shipxca = 0;
	
	load_sprites_file(EXTRA_SHAPES, "estsc.shp");
	
	setjasondelay2(1000);
	
	play_song(8);
	
	FILE *f = dir_fopen_die(data_dir(), "tyrian.cdt", "rb");
	while (!feof(f))
	{
		maxlen += 20 * 3;
		JE_readCryptLn(f, credstr[max]);
		max++;
	}
	
	memcpy(colors, palettes[6-1], sizeof(colors));
	JE_clr256();
	JE_showVGA();
	fade_palette(colors, 2, 0, 255);
	
	tempScreenSeg = VGAScreenSeg;
	
	for (x = 0; x < maxlen; x++)
	{
		setjasondelay(1);
		JE_clr256();
		
		blit_sprite_hv(VGAScreenSeg, 319 - sprite(EXTRA_SHAPES, currentpic-1)->width, 100 - (sprite(EXTRA_SHAPES, currentpic-1)->height / 2), EXTRA_SHAPES, currentpic-1, 0x0, fade - 15);
		
		fade += fadechg;
		if (fade == 0 && fadechg == -1)
		{
			fadechg = 1;
			currentpic++;
			if (currentpic > sprite_table[EXTRA_SHAPES].count)
				currentpic = 1;
		}
		if (fade == 15)
			fadechg = 0;
		
		if (delaycount2() == 0)
		{
			fadechg = -1;
			setjasondelay2(900);
		}
		
		curpos = (x / 3) / 20;
		yloc = 20 - ((x / 3) % 20);
		
		if (x % 200 == 0)
		{
			currentship = (mt_rand() % 11) + 1;
			shipxwait = (mt_rand() % 80) + 10;
			if ((mt_rand() % 2) == 1)
			{
				shipx = 1;
				shipxc = 0;
				shipxca = 1;
			}
			else
			{
				shipx = 900;
				shipxc = 0;
				shipxca = -1;
			}
		}
		
		shipxwait--;
		if (shipxwait == 0)
		{
			if (shipx == 1 || shipx == 900)
				shipxc = 0;
			shipxca = -shipxca;
			shipxwait = (mt_rand() % 40) + 15;
		}
		shipxc += shipxca;
		shipx += shipxc;
		if (shipx < 1)
		{
			shipx = 1;
			shipxwait = 1;
		}
		if (shipx > 900)
		{
			shipx = 900;
			shipxwait = 1;
		}
      	tempI = shipxc * shipxc;
		if (450 + tempI < 0 || 450 + tempI > 900)
		{
			if (shipxca < 0 && shipxc < 0)
				shipxwait = 1;
			if (shipxca > 0 && shipxc > 0)
				shipxwait = 1;
		}
		tempW = ships[currentship].shipgraphic;
		if (shipxc < -10)
			tempW -= 2;
		if (shipxc < -20)
			tempW -= 2;
		if (shipxc > 10)
			tempW += 2;
		if (shipxc > 20)
			tempW += 2;
		blit_sprite2x2(VGAScreen, shipx / 40, 184 - (x % 200), shapes9, tempW);
		
		for (newpos = curpos - 9; newpos <= curpos; newpos++)
		{
			if (newpos > 0 && newpos <= max)
			{
				if (strcmp(&credstr[newpos-1][0], ".") && strlen(credstr[newpos-1]))
				{
					JE_outTextAdjust(110 - JE_textWidth(&credstr[newpos-1][1], SMALL_FONT_SHAPES) / 2 + abs((yloc / 18) % 4 - 2) - 1, yloc - 1, &credstr[newpos-1][1], credstr[newpos-1][0] - 65, -8, SMALL_FONT_SHAPES, false);
					JE_outTextAdjust(110 - JE_textWidth(&credstr[newpos-1][1], SMALL_FONT_SHAPES) / 2, yloc, &credstr[newpos-1][1], credstr[newpos-1][0] - 65, -2, SMALL_FONT_SHAPES, false);
				}
			}
			
			yloc += 20;
		}
		
		JE_bar(0,  0, 319, 10, 0);
		JE_bar(0, 190, 319, 199, 0);
		
		if (currentpic == sprite_table[EXTRA_SHAPES].count)
			JE_outTextAdjust(5, 180, miscText[55-1], 2, -2, SMALL_FONT_SHAPES, false);
		
		NETWORK_KEEP_ALIVE();
		
		wait_delay();
		
		JE_showVGA();
		if (JE_anyButton())
		{
			x = maxlen - 1;
		}
		else
		{
			if (newpos == maxlines - 8)
				fade_song();
			if (x == maxlen - 1)
			{
				x--;
				play_song(9);
			}
		}
	}
	
	fade_black(10);
	
	free_sprites(EXTRA_SHAPES);
}

void JE_endLevelAni( void )
{
	JE_word x, y;
	JE_byte temp;
	char tempStr[256];
	
	Sint8 i;
	
	if (!constantPlay)
	{
		/*Grant Bonus Items*/
		/*Front/Rear*/
		saveTemp[SAVE_FILES_SIZE + pItems[P_FRONT]] = 1;
		saveTemp[SAVE_FILES_SIZE + pItems[P_REAR]] = 1;
		saveTemp[SAVE_FILES_SIZE + pItemsPlayer2[P_FRONT]] = 1;
		saveTemp[SAVE_FILES_SIZE + pItemsPlayer2[P_REAR]] = 1;
		
		/*Special*/
		if (pItems[P_SPECIAL] < 21)
			saveTemp[SAVE_FILES_SIZE + 81 + pItems[P_SPECIAL]] = 1;
		
		/*Options*/
		saveTemp[SAVE_FILES_SIZE + 51 + pItems[P_LEFT_SIDEKICK]] = 1;
		saveTemp[SAVE_FILES_SIZE + 51 + pItems[P_RIGHT_SIDEKICK]] = 1;
		saveTemp[SAVE_FILES_SIZE + 51 + pItemsPlayer2[P_LEFT_SIDEKICK]] = 1;
		saveTemp[SAVE_FILES_SIZE + 51 + pItemsPlayer2[P_RIGHT_SIDEKICK]] = 1;
	}
	
	JE_changeDifficulty();
	
	memcpy(pItemsBack2, pItems, sizeof(pItemsBack2));
	strcpy(lastLevelName, levelName);
	
	JE_wipeKey();
	frameCountMax = 4;
	textGlowFont = SMALL_FONT_SHAPES;
	
	SDL_Color white = { 255, 255, 255 };
	set_colors(white, 254, 254);
	
	if (!levelTimer || levelTimerCountdown > 0 || !(episodeNum == 4))
		JE_playSampleNum(V_LEVEL_END);
	else
		play_song(21);
	  
	if (bonusLevel)
	{
		JE_outTextGlow(20, 20, miscText[17-1]);
	}
	else if (playerAlive && (!twoPlayerMode || playerAliveB))
	{
		sprintf(tempStr, "%s %s", miscText[27-1], levelName);
		JE_outTextGlow(20, 20, tempStr);
	}
	else
	{
		sprintf(tempStr, "%s %s", miscText[62-1], levelName);
		JE_outTextGlow(20, 20, tempStr);
	}
	
	if (twoPlayerMode)
	{
		sprintf(tempStr, "%s %d", miscText[41-1], score);
		JE_outTextGlow(30, 50, tempStr);
		
		sprintf(tempStr, "%s %d", miscText[42-1], score2);
		JE_outTextGlow(30, 70, tempStr);
	}
	else
	{
		sprintf(tempStr, "%s %d", miscText[28-1], score);
		JE_outTextGlow(30, 50, tempStr);
	}
	
	temp = (totalEnemy == 0) ? 0 : roundf(enemyKilled * 100 / totalEnemy);
	sprintf(tempStr, "%s %d%%", miscText[63-1], temp);
	JE_outTextGlow(40, 90, tempStr);
	
	if (!constantPlay)
		editorLevel += temp / 5;
	
	if (!onePlayerAction && !twoPlayerMode)
	{
		JE_outTextGlow(30, 120, miscText[4-1]);   /*Cubes*/
		
		if (cubeMax > 0)
		{
			if (cubeMax > 4)
				cubeMax = 4;
			
			if (frameCountMax != 0)
				frameCountMax = 1;
			
			for (temp = 1; temp <= cubeMax; temp++)
			{
				NETWORK_KEEP_ALIVE();
				
				JE_playSampleNum(18);
				x = 20 + 30 * temp;
				y = 135;
				JE_drawCube(x, y, 9, 0);
				JE_showVGA();
				
				for (i = -15; i <= 10; i++)
				{
					setjasondelay(frameCountMax);
					
					blit_sprite_hv(VGAScreenSeg, x, y, OPTION_SHAPES, 25, 0x9, i);
					
					if (JE_anyButton())
						frameCountMax = 0;
					
					JE_showVGA();
					
					wait_delay();
				}
				for (i = 10; i >= 0; i--)
				{
					setjasondelay(frameCountMax);
					
					blit_sprite_hv(VGAScreenSeg, x, y, OPTION_SHAPES, 25, 0x9, i);
					
					if (JE_anyButton())
						frameCountMax = 0;
					
					JE_showVGA();
					
					wait_delay();
				}
			}
		}
		else
		{
			JE_outTextGlow(50, 135, miscText[15-1]);
		}
		
	}

	if (frameCountMax != 0)
	{
		frameCountMax = 6;
		temp = 1;
	} else {
		temp = 0;
	}
	temp2 = twoPlayerMode ? 150 : 160;
	JE_outTextGlow(90, temp2, miscText[5-1]);
	
	if (!constantPlay)
	{
		do
		{
			setjasondelay(1);
			
			NETWORK_KEEP_ALIVE();
			
			wait_delay();
		} while (!(JE_anyButton() || (frameCountMax == 0 && temp == 1)));
	}
	
	wait_noinput(false, false, true); // TODO: should up the joystick repeat temporarily instead
	
	fade_black(15);
	JE_clr256();
}

void JE_drawCube( JE_word x, JE_word y, JE_byte filter, JE_byte brightness )
{
	blit_sprite_dark(tempScreenSeg, x + 4, y + 4, OPTION_SHAPES, 25, false);
	blit_sprite_dark(tempScreenSeg, x + 3, y + 3, OPTION_SHAPES, 25, false);
	blit_sprite_hv(tempScreenSeg, x, y, OPTION_SHAPES, 25, filter, brightness);
}

void JE_handleChat( void )
{
	// STUB(); Annoying piece of crap =P
}

bool str_pop_int( char *str, int *val )
{
	bool success = false;
	
	char buf[256];
	assert(strlen(str) < sizeof(buf));
	
	// grab the value from str
	char *end;
	*val = strtol(str, &end, 10);
	
	if (end != str)
	{
		success = true;
		
		// shift the rest to the beginning
		strcpy(buf, end);
		strcpy(str, buf);
	}
	
	return success;
}

void JE_operation( JE_byte slot )
{
	JE_byte flash;
	char stemp[21];
	char tempStr[51];
	
	if (!performSave)
	{
		if (saveFiles[slot-1].level > 0)
		{
			gameJustLoaded = true;
			JE_loadGame(slot);
			gameLoaded = true;
		}
	}
	else if (slot % 11 != 0)
	{
		strcpy(stemp, "              ");
		memcpy(stemp, saveFiles[slot-1].name, strlen(saveFiles[slot-1].name));
		temp = strlen(stemp);
		while (stemp[temp-1] == ' ' && --temp);
		
		flash = 8 * 16 + 10;
		
		wait_noinput(false, true, false);
		
		JE_barShade(65, 55, 255, 155);
		
		bool quit = false;
		while (!quit)
		{
			service_SDL_events(true);
			
			blit_sprite(VGAScreenSeg, 50, 50, OPTION_SHAPES, 35);  // message box
			
			JE_textShade(60, 55, miscText[1-1], 11, 4, DARKEN);
			JE_textShade(70, 70, levelName, 11, 4, DARKEN);
			
			do
			{
				flash = (flash == 8 * 16 + 10) ? 8 * 16 + 2 : 8 * 16 + 10;
				temp3 = (temp3 == 6) ? 2 : 6;
				
				strcpy(tempStr, miscText[2-1]);
				strncat(tempStr, stemp, temp);
				JE_outText(65, 89, tempStr, 8, 3);
				tempW = 65 + JE_textWidth(tempStr, TINY_FONT);
				JE_barShade(tempW + 2, 90, tempW + 6, 95);
				JE_bar(tempW + 1, 89, tempW + 5, 94, flash);
				
				for (int i = 0; i < 14; i++)
				{
					setjasondelay(1);
					
					JE_mouseStart();
					JE_showVGA();
					JE_mouseReplace();
					
					push_joysticks_as_keyboard();
					service_wait_delay();
					
					if (newkey || newmouse)
						break;
				}
				
			}
			while (!newkey && !newmouse);
			
			if (mouseButton > 0)
			{
				if (mouseX > 56 && mouseX < 142 && mouseY > 123 && mouseY < 149)
				{
					quit = true;
					JE_saveGame(slot, stemp);
					JE_playSampleNum(S_SELECT);
				}
				else if (mouseX > 151 && mouseX < 237 && mouseY > 123 && mouseY < 149)
				{
					quit = true;
					JE_playSampleNum(S_SPRING);
				}
			}
			else if (newkey)
			{
				bool validkey = false;
				lastkey_char = toupper(lastkey_char);
				switch (lastkey_char)
				{
					case ' ':
					case '-':
					case '.':
					case ',':
					case ':':
					case '!':
					case '?':
					case '#':
					case '@':
					case '$':
					case '%':
					case '*':
					case '(':
					case ')':
					case '/':
					case '=':
					case '+':
					case '<':
					case '>':
					case ';':
					case '"':
					case '\'':
						validkey = true;
					default:
						if (temp < 14 && (validkey || (lastkey_char >= 'A' && lastkey_char <= 'Z') || (lastkey_char >= '0' && lastkey_char <= '9')))
						{
							JE_playSampleNum(S_CURSOR);
							stemp[temp] = lastkey_char;
							temp++;
						}
						break;
					case SDLK_BACKSPACE:
					case SDLK_DELETE:
						if (temp)
						{
							temp--;
							stemp[temp] = ' ';
							JE_playSampleNum(S_CLICK);
						}
						break;
					case SDLK_ESCAPE:
						quit = true;
						JE_playSampleNum(S_SPRING);
						break;
					case SDLK_RETURN:
						quit = true;
						JE_saveGame(slot, stemp);
						drawGameSaved = true;
						JE_playSampleNum(S_SELECT);
						break;
				}
				
			}
		}
	}
	
	wait_noinput(false, true, false);
}

void JE_inGameDisplays( void )
{
	char stemp[21];
	JE_byte temp;
	
	char tempstr[256];

	sprintf(tempstr, "%d", score);
	JE_textShade(30, 175, tempstr, 2, 4, FULL_SHADE);
	if (twoPlayerMode && !galagaMode)
	{
		sprintf(tempstr, "%d", score2);
		JE_textShade(230, 175, tempstr, 2, 4, FULL_SHADE);
	}

	/*Special Weapon?*/
	if (pItems[P_SPECIAL] > 0)
	{
		blit_sprite2x2(VGAScreen, 25, 1, eShapes6, special[pItems[P_SPECIAL]].itemgraphic);
	}

	/*Lives Left*/
	if (onePlayerAction || twoPlayerMode)
	{
		
		for (temp = 0; temp < (onePlayerAction ? 1 : 2); temp++)
		{
			temp5 = (temp == 0 && pItems[P_SPECIAL] > 0) ? 35 : 15;
			tempW = (temp == 0) ? 30: 270;
			
			if (portPower[temp] > 5)
			{
				blit_sprite2(VGAScreen, tempW, temp5, shapes9, 285);
				tempW = (temp == 0) ? 45 : 250;
				sprintf(tempstr, "%d", portPower[temp] - 1);
				JE_textShade(tempW, temp5 + 3, tempstr, 15, 1, FULL_SHADE);
			} else if (portPower[temp] > 1) {
				for (temp2 = 1; temp2 < portPower[temp]; temp2++)
				{
					blit_sprite2(VGAScreen, tempW, temp5, shapes9, 285);
					tempW = (temp == 0) ? (tempW + 12) : (tempW - 12);
				}
			}
			
			strcpy(stemp, (temp == 0) ? miscText[49-1] : miscText[50-1]);
			if (isNetworkGame)
			{
				strcpy(stemp, JE_getName(temp+1));
			}
			
			tempW = (temp == 0) ? 28 : (285 - JE_textWidth(stemp, TINY_FONT));
			JE_textShade(tempW, temp5 - 7, stemp, 2, 6, FULL_SHADE);
			
		}
		
	}

	/*Super Bombs!!*/
	for (temp = 0; temp < 2; temp++)
	{
		if (superBomb[temp] > 0)
		{
			tempW = (temp == 0) ? 30 : 270;
			
			for (temp2 = 0; temp2 < superBomb[temp]; temp2++)
			{
				blit_sprite2(VGAScreen, tempW, 160, shapes9, 304);
				tempW = (temp == 0) ? (tempW + 12) : (tempW - 12);
			}
		}
	}

	if (youAreCheating)
	{
		JE_outText(90, 170, "Cheaters always prosper.", 3, 4);
	}
}

void JE_mainKeyboardInput( void )
{
	tempB = JE_gammaCheck();

	/* { Network Request Commands } */

	if (!isNetworkGame)
	{
		/* { Edited Ships } for Player 1 */
		if (extraAvail && keysactive[SDLK_TAB] && !isNetworkGame && !superTyrian)
		{
			for (x = SDLK_0; x <= SDLK_9; x++)
			{
				if (keysactive[x])
				{
					int z = x == SDLK_0 ? 10 : x - SDLK_0;
					pItems[P_SHIP] = 90 + z;                     /*Ships*/
					z = (z - 1) * 15;
					pItems[P_FRONT] = extraShips[z + 1];
					pItems[P_REAR] = extraShips[z + 2];
					pItems[P_SPECIAL] = extraShips[z + 3];
					pItems[P_LEFT_SIDEKICK] = extraShips[z + 4];
					pItems[P_RIGHT_SIDEKICK] = extraShips[z + 5];
					pItems[P_GENERATOR] = extraShips[z + 6];
					/*Armor*/
					pItems[P_SHIELD] = extraShips[z + 8];
					memset(shotMultiPos, 0, sizeof(shotMultiPos));
					JE_portConfigs();
					if (portConfig[1] > tempW)
						portConfig[1] = 1;
					tempW = armorLevel;
					JE_getShipInfo();
					if (armorLevel > tempW && editShip1)
						armorLevel = tempW;
					editShip1 = true;
					
					SDL_Surface *temp_surface = VGAScreen;
					VGAScreen = VGAScreenSeg;
					JE_wipeShieldArmorBars();
					JE_drawArmor();
					JE_drawShield();
					VGAScreen = temp_surface;
					tempScreenSeg = VGAScreenSeg; /* sega000 */
					JE_drawOptions();
					tempScreenSeg = VGAScreenSeg; /* sega000 */
					
					keysactive[x] = false;
				}
			}
		}

		/* for Player 2 */
		if (extraAvail && keysactive[SDLK_CAPSLOCK] && !isNetworkGame && !superTyrian)
		{
			for (x = SDLK_0; x <= SDLK_9; x++)
			{
				if (keysactive[x])
				{
					int z = x == SDLK_0 ? 10 : x - SDLK_0;
					pItemsPlayer2[P_SHIP] = 90 + z;
					z = (z - 1) * 15;
					pItemsPlayer2[P_FRONT] = extraShips[z + 1];
					pItemsPlayer2[P_REAR] = extraShips[z + 2];
					pItemsPlayer2[P_SPECIAL] = extraShips[z + 3];
					pItemsPlayer2[P_LEFT_SIDEKICK] = extraShips[z + 4];
					pItemsPlayer2[P_RIGHT_SIDEKICK] = extraShips[z + 5];
					pItemsPlayer2[P_GENERATOR] = extraShips[z + 6];
					/*Armor*/
					pItemsPlayer2[P_SHIELD] = extraShips[z + 8];
					memset(shotMultiPos, 0, sizeof(shotMultiPos));
					JE_portConfigs();
					if (portConfig[1] > tempW)
						portConfig[1] = 1;
					tempW = armorLevel2;
					JE_getShipInfo();
					if (armorLevel2 > tempW && editShip2)
						armorLevel2 = tempW;
					editShip2 = true;
					
					SDL_Surface *temp_surface = VGAScreen;
					VGAScreen = VGAScreenSeg;
					JE_wipeShieldArmorBars();
					JE_drawArmor();
					JE_drawShield();
					VGAScreen = temp_surface;
					tempScreenSeg = VGAScreenSeg; /* sega000 */
					JE_drawOptions();
					tempScreenSeg = VGAScreenSeg; /* sega000 */
					
					keysactive[x] = false;
				}
			}
		}
	}

	/* { In-Game Help } */
	if (keysactive[SDLK_F1])
	{
		if (isNetworkGame)
		{
			helpRequest = true;
		} else {
			JE_inGameHelp();
			skipStarShowVGA = true;
		}
	}

	/* {!Activate Nort Ship!} */
	if (keysactive[SDLK_F2] && keysactive[SDLK_F4] && keysactive[SDLK_F6] && keysactive[SDLK_F7] &&
	    keysactive[SDLK_F9] && keysactive[SDLK_BACKSLASH] && keysactive[SDLK_SLASH])
	{
		if (isNetworkGame)
		{
			nortShipRequest = true;
		} else {
			pItems[P_SHIP] = 12;
			pItems[P_SPECIAL] = 13;
			pItems[P_REAR] = 36;
			shipGr = 1;
		}
	}

	/* {Cheating} */
	if (!isNetworkGame && !twoPlayerMode && !superTyrian && superArcadeMode == SA_NONE)
	{
		if (keysactive[SDLK_F2] && keysactive[SDLK_F3] && keysactive[SDLK_F6])
		{
			youAreCheating = !youAreCheating;
			keysactive[SDLK_F2] = false;
		}

		if (keysactive[SDLK_F2] && keysactive[SDLK_F3] && (keysactive[SDLK_F4] || keysactive[SDLK_F5]) && !superTyrian)
		{
			armorLevel = 0;
			armorLevel2 = 0;
			youAreCheating = !youAreCheating;
			JE_drawTextWindow(miscText[63-1]);
		}

		if (constantPlay && keysactive[SDLK_c] && !superTyrian && superArcadeMode == SA_NONE)
		{
			youAreCheating = !youAreCheating;
			keysactive[SDLK_c] = false;
		}
	}

	if (superTyrian)
	{
		youAreCheating = false;
	}

	/* {Personal Commands} */

	/* {DEBUG} */
	if (keysactive[SDLK_F10] && keysactive[SDLK_BACKSPACE])
	{
		keysactive[SDLK_F10] = false;
		debug = !debug;

		debugHist = 1;
		debugHistCount = 1;

		/* YKS: clock ticks since midnight replaced by SDL_GetTicks */
		lastDebugTime = SDL_GetTicks();
	}

	/* {CHEAT-SKIP LEVEL} */
	if (keysactive[SDLK_F2] && keysactive[SDLK_F6] && (keysactive[SDLK_F7] || keysactive[SDLK_F8]) && !keysactive[SDLK_F9]
	    && !superTyrian && superArcadeMode == SA_NONE)
	{
		if (isNetworkGame)
		{
			skipLevelRequest = true;
		} else {
			levelTimer = true;
			levelTimerCountdown = 0;
			endLevel = true;
			levelEnd = 40;
		}
	}
	
	/* pause game */
	pause_pressed |= keysactive[SDLK_p];
	
	/* in-game setup */
	ingamemenu_pressed |= keysactive[SDLK_ESCAPE];
	
	/* {MUTE SOUND} */
	if (keysactive[SDLK_s])
	{
		keysactive[SDLK_s] = false;
		
		samples_disabled = !samples_disabled;
		
		JE_drawTextWindow(samples_disabled ? miscText[17] : miscText[18]);
	}
	
	/* {MUTE MUSIC} */
	if (keysactive[SDLK_m])
	{
		keysactive[SDLK_m] = false;
		
		music_disabled = !music_disabled;
		if (!music_disabled)
			restart_song();
		
		JE_drawTextWindow(music_disabled ? miscText[35] : miscText[36]);
	}
	
	if (keysactive[SDLK_BACKSPACE])
	{
		/* toggle screenshot pause */
		if (keysactive[SDLK_NUMLOCK])
		{
			superPause = !superPause;
		}

		/* {SMOOTHIES} */
		if (keysactive[SDLK_F12] && keysactive[SDLK_SCROLLOCK])
		{
			for (temp = SDLK_2; temp <= SDLK_9; temp++)
			{
				if (keysactive[temp])
				{
					smoothies[temp-SDLK_2] = !smoothies[temp-SDLK_2];
				}
			}
			if (keysactive[SDLK_0])
			{
				smoothies[8] = !smoothies[8];
			}
		} else

		/* {CYCLE THROUGH FILTER COLORS} */
		if (keysactive[SDLK_MINUS])
		{
			if (levelFilter == -99)
			{
				levelFilter = 0;
			} else {
				levelFilter++;
				if (levelFilter == 16)
				{
					levelFilter = -99;
				}
			}
		} else

		/* {HYPER-SPEED} */
		if (keysactive[SDLK_1])
		{
			fastPlay++;
			if (fastPlay > 2)
			{
				fastPlay = 0;
			}
			keysactive[SDLK_1] = false;
			JE_setNewGameSpeed();
		}

		/* {IN-GAME RANDOM MUSIC SELECTION} */
		if (keysactive[SDLK_SCROLLOCK])
		{
			play_song(mt_rand() % MUSIC_NUM);
		}
	}
}

void JE_pauseGame( void )
{
	JE_boolean done = false;
	JE_word mouseX, mouseY;
	
	tempScreenSeg = VGAScreenSeg; // sega000
	if (!superPause)
	{
		JE_dString(120, 90, miscText[22], FONT_SHAPES);
		
		VGAScreen = VGAScreenSeg;
		JE_showVGA();
	}
	
	set_volume(tyrMusicVolume / 2, fxVolume);
	
	if (isNetworkGame)
	{
		network_prepare(PACKET_GAME_PAUSE);
		network_send(4);  // PACKET_GAME_PAUSE
		
		while (true)
		{
			service_SDL_events(false);
			
			if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_GAME_PAUSE)
			{
				network_update();
				break;
			}
			
			network_update();
			network_check();
			
			SDL_Delay(16);
		}
	}
	
	wait_noinput(false, false, true); // TODO: should up the joystick repeat temporarily instead
	
	do
	{
		setjasondelay(2);
		
		push_joysticks_as_keyboard();
		service_SDL_events(true);
		
		if ((newkey && lastkey_sym != SDLK_LCTRL && lastkey_sym != SDLK_RCTRL && lastkey_sym != SDLK_LALT && lastkey_sym != SDLK_RALT)
		    || JE_mousePosition(&mouseX, &mouseY) > 0)
		{
			if (isNetworkGame)
			{
				network_prepare(PACKET_WAITING);
				network_send(4);  // PACKET_WAITING
			}
			done = true;
		}
		
		if (isNetworkGame)
		{
			network_check();
			
			if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_WAITING)
			{
				network_check();
				
				done = true;
			}
		}
		
		wait_delay();
	} while (!done);
	
	if (isNetworkGame)
	{
		while (!network_is_sync())
		{
			service_SDL_events(false);
			
			network_check();
			SDL_Delay(16);
		}
	}
	
	set_volume(tyrMusicVolume, fxVolume);

	tempScreenSeg = VGAScreen;
	//skipStarShowVGA = true;
}

void JE_playerMovement( JE_byte inputDevice,
                        JE_byte playerNum_,
                        JE_word shipGr_,
                        Sprite2_array *shapes9ptr_,
                        JE_integer *armorLevel_, JE_integer *baseArmor_,
                        JE_shortint *shield_, JE_shortint *shieldMax_,
                        JE_word *playerInvulnerable_,
                        JE_integer *PX_, JE_integer *PY_,
                        JE_integer *lastPX2_, JE_integer *lastPY2_,
                        JE_integer *PXChange_, JE_integer *PYChange_,
                        JE_integer *lastTurn_, JE_integer *lastTurn2_,
                        JE_byte *stopWaitX_, JE_byte *stopWaitY_,
                        JE_word *mouseX_, JE_word *mouseY_,
                        JE_boolean *playerAlive_,
                        JE_byte *playerStillExploding_,
                        JE_PItemsType pItems_ )
{
	JE_integer mouseXC, mouseYC;
	JE_integer accelXC, accelYC;
	JE_byte leftOptionIsSpecial = 0;
	JE_byte rightOptionIsSpecial = 0;

	if (playerNum_ == 2 || !twoPlayerMode)
	{
		if (playerNum_ == 2)
		{
			tempW = weaponPort[pItemsPlayer2[P_REAR]].opnum;
		} else {
			tempW = weaponPort[pItems_[P_REAR]].opnum;
		}
		if (portConfig[2-1] > tempW)
		{
			portConfig[2-1] = 1;
		}
	}

	if (isNetworkGame && thisPlayerNum == playerNum_)
	{
		network_state_prepare();
		memset(&packet_state_out[0]->data[4], 0, 10);
	}

redo:

	if (isNetworkGame)
	{
		inputDevice = 0;
	}

	mouseXC = 0;
	mouseYC = 0;
	accelXC = 0;
	accelYC = 0;

	bool link_gun_analog = false;
	float link_gun_angle = 0;

	/* Draw Player */
	if (!*playerAlive_)
	{
		if (*playerStillExploding_ > 0)
		{
			(*playerStillExploding_)--;

			if (levelEndFxWait > 0)
			{
				levelEndFxWait--;
			}
			else
			{
				levelEndFxWait = (mt_rand() % 6) + 3;
				if ((mt_rand() % 3) == 1)
					soundQueue[6] = S_EXPLOSION_9;
				else
					soundQueue[5] = S_EXPLOSION_11;
			}
			tempW = *PX_ + (mt_rand() % 32) - 16;
			tempW2 = *PY_ + (mt_rand() % 32) - 16;

			JE_setupExplosionLarge(false, 0, *PX_ + (mt_rand() % 32) - 16, *PY_ + (mt_rand() % 32) - 16 + 7);
			JE_setupExplosionLarge(false, 0, *PX_, *PY_ + 7);

			if (levelEnd > 0)
				levelEnd--;
		}
		else
		{
			if (twoPlayerMode || onePlayerAction)
			{
				if (portPower[playerNum_-1] > 1)
				{
					reallyEndLevel = false;
					shotMultiPos[playerNum_-1] = 0;
					portPower[playerNum_-1]--;
					JE_calcPurpleBall(playerNum_);
					twoPlayerLinked = false;
					if (galagaMode)
						twoPlayerMode = false;
					*PY_ = 160;
					*playerInvulnerable_ = 100;
					*playerAlive_ = true;
					endLevel = false;

					if (galagaMode || episodeNum == 4)
						*armorLevel_ = *baseArmor_;
					else
						*armorLevel_ = *baseArmor_ / 2;

					if (galagaMode)
						*shield_ = 0;
					else
						*shield_ = *shieldMax_ / 2;

					VGAScreen = VGAScreenSeg; /* side-effect of game_screen */
					JE_drawArmor();
					JE_drawShield();
					VGAScreen = game_screen; /* side-effect of game_screen */
					goto redo;
				} else {
					if (galagaMode)
						twoPlayerMode = false;
					if (allPlayersGone && isNetworkGame)
						reallyEndLevel = true;
				}

			}
		}
	} else if (constantDie)	{
		if (*playerStillExploding_ == 0)
		{

			*shield_ = 0;
			if (*armorLevel_ > 0)
			{
				(*armorLevel_)--;
			} else {
				*playerAlive_ = false;
				*playerStillExploding_ = 60;
				levelEnd = 40;
			}

			JE_wipeShieldArmorBars();
			VGAScreen = VGAScreenSeg; /* side-effect of game_screen */
			JE_drawArmor();
			VGAScreen = game_screen; /* side-effect of game_screen */
			if (portPower[1-1] < 11)
				portPower[1-1]++;
		}
	}


	if (!*playerAlive_)
	{
		explosionFollowAmountX = explosionFollowAmountY = 0;
		return;
	}
	
		if (!endLevel)
		{

			*mouseX_ = *PX_;
			*mouseY_ = *PY_;
			button[1-1] = false;
			button[2-1] = false;
			button[3-1] = false;
			button[4-1] = false;

			/* --- Movement Routine Beginning --- */

			if (!isNetworkGame || playerNum_ == thisPlayerNum)
			{

				if (endLevel)
				{
					*PY_ -= 2;
				}
				else 
				{
					if (record_demo || play_demo)
						inputDevice = 1;  // keyboard is required device for demo recording
					
					// demo playback input
					if (play_demo)
					{
						if (!replay_demo_keys())
						{
							endLevel = true;
							levelEnd = 40;
						}
					}
					
					/* joystick input */
					if ((inputDevice == 0 || inputDevice >= 3) && joysticks > 0)
					{
						int j = inputDevice  == 0 ? 0 : inputDevice - 3;
						int j_max = inputDevice == 0 ? joysticks : inputDevice - 3 + 1;
						for (; j < j_max; j++)
						{
							poll_joystick(j);
							
							if (joystick[j].analog)
							{
								mouseXC += joystick_axis_reduce(j, joystick[j].x);
								mouseYC += joystick_axis_reduce(j, joystick[j].y);
								
								link_gun_analog = joystick_analog_angle(j, &link_gun_angle);
							}
							else
							{
								*PX_ += joystick[j].direction[3] ? -CURRENT_KEY_SPEED : 0 + joystick[j].direction[1] ? CURRENT_KEY_SPEED : 0;
								*PY_ += joystick[j].direction[0] ? -CURRENT_KEY_SPEED : 0 + joystick[j].direction[2] ? CURRENT_KEY_SPEED : 0;
							}
							
							button[0] |= joystick[j].action[0];
							button[1] |= joystick[j].action[2];
							button[2] |= joystick[j].action[3];
							button[3] |= joystick[j].action_pressed[1];
							
							ingamemenu_pressed |= joystick[j].action_pressed[4];
							pause_pressed |= joystick[j].action_pressed[5];
						}
					}
					
					service_SDL_events(false);
					
					/* mouse input */
					if ((inputDevice == 0 || inputDevice == 2) && mouseInstalled)
					{
						button[0] |= mouse_pressed[0];
						button[1] |= mouse_pressed[1];
						button[2] |= mouse_threeButton ? mouse_pressed[2] : mouse_pressed[1];

						if (input_grabbed)
						{
							mouseXC += mouse_x - 159;
							mouseYC += mouse_y - 100;
						}

						if ((!isNetworkGame || playerNum_ == thisPlayerNum)
						    && (!galagaMode || (playerNum_ == 2 || !twoPlayerMode || playerStillExploding2 > 0)))
							set_mouse_position(159, 100);
					}

					/* keyboard input */
					if ((inputDevice == 0 || inputDevice == 1) && !play_demo)
					{
						if (keysactive[keySettings[0]])
							*PY_ -= CURRENT_KEY_SPEED;
						if (keysactive[keySettings[1]])
							*PY_ += CURRENT_KEY_SPEED;
						
						if (keysactive[keySettings[2]])
							*PX_ -= CURRENT_KEY_SPEED;
						if (keysactive[keySettings[3]])
							*PX_ += CURRENT_KEY_SPEED;
						
						button[0] |= keysactive[keySettings[4]];
						button[3] |= keysactive[keySettings[5]];
						button[1] |= keysactive[keySettings[6]];
						button[2] |= keysactive[keySettings[7]];
						
						if (constantPlay)
						{
							for (unsigned int i = 0; i < 4; i++)
								button[i] = true;
							
							(*PY_)++;
							*PX_ += constantLastX;
						}
						
						// TODO: check if demo recording still works
						if (record_demo)
						{
							bool new_input = false;
							
							for (unsigned int i = 0; i < 8; i++)
							{
								bool temp = demo_keys & (1 << i);
								if (temp != keysactive[keySettings[i]])
									new_input = true;
							}
							
							demo_keys_wait++;
							
							if (new_input)
							{
								demo_keys_wait = SDL_Swap16(demo_keys_wait);
								efwrite(&demo_keys_wait, sizeof(Uint16), 1, demo_file);
								
								demo_keys = 0;
								for (unsigned int i = 0; i < 8; i++)
									demo_keys |= keysactive[keySettings[i]] ? (1 << i) : 0;
								
								fputc(demo_keys, demo_file);
								
								demo_keys_wait = 0;
							}
						}
					}

					if (smoothies[9-1])
					{
						*mouseY_ = *PY_ - (*mouseY_ - *PY_);
						mouseYC = -mouseYC;
					}
					
					accelXC += *PX_ - *mouseX_;
					accelYC += *PY_ - *mouseY_;
					
					if (mouseXC > 30)
						mouseXC = 30;
					else if (mouseXC < -30)
						mouseXC = -30;
					if (mouseYC > 30)
						mouseYC = 30;
					else if (mouseYC < -30)
						mouseYC = -30;
					
					if (mouseXC > 0)
						*PX_ += (mouseXC + 3) / 4;
					else if (mouseXC < 0)
						*PX_ += (mouseXC - 3) / 4;
					if (mouseYC > 0)
						*PY_ += (mouseYC + 3) / 4;
					else if (mouseYC < 0)
						*PY_ += (mouseYC - 3) / 4;
					
					if (mouseXC > 3)
						accelXC++;
					else if (mouseXC < -2)
						accelXC--;
					if (mouseYC > 2)
						accelYC++;
					else if (mouseYC < -2)
						accelYC--;
					
				}   /*endLevel*/

				if (isNetworkGame && playerNum_ == thisPlayerNum)
				{
					Uint16 buttons = 0;
					for (int i = 4 - 1; i >= 0; i--)
					{
						buttons <<= 1;
						buttons |= button[i];
					}
					
					SDLNet_Write16(*PX_ - *mouseX_, &packet_state_out[0]->data[4]);
					SDLNet_Write16(*PY_ - *mouseY_, &packet_state_out[0]->data[6]);
					SDLNet_Write16(accelXC,         &packet_state_out[0]->data[8]);
					SDLNet_Write16(accelYC,         &packet_state_out[0]->data[10]);
					SDLNet_Write16(buttons,         &packet_state_out[0]->data[12]);
					
					*PX_ = *mouseX_;
					*PY_ = *mouseY_;
					
					button[0] = false;
					button[1] = false;
					button[2] = false;
					button[3] = false;
				
					accelXC = 0;
					accelYC = 0;
				}
			}  /*isNetworkGame*/

			/* --- Movement Routine Ending --- */

			moveOk = true;

			if (isNetworkGame && !network_state_is_reset())
			{
				if (playerNum_ != thisPlayerNum)
				{
					if (thisPlayerNum == 2)
						difficultyLevel = SDLNet_Read16(&packet_state_in[0]->data[16]);
					
					Uint16 buttons = SDLNet_Read16(&packet_state_in[0]->data[12]);
					for (int i = 0; i < 4; i++)
					{
						button[i] = buttons & 1;
						buttons >>= 1;
					}
					
					*PX_ += (Sint16)SDLNet_Read16(&packet_state_in[0]->data[4]);
					*PY_ += (Sint16)SDLNet_Read16(&packet_state_in[0]->data[6]);
					accelXC = (Sint16)SDLNet_Read16(&packet_state_in[0]->data[8]);
					accelYC = (Sint16)SDLNet_Read16(&packet_state_in[0]->data[10]);
				}
				else
				{
					Uint16 buttons = SDLNet_Read16(&packet_state_out[network_delay]->data[12]);
					for (int i = 0; i < 4; i++)
					{
						button[i] = buttons & 1;
						buttons >>= 1;
					}
					
					*PX_ += (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[4]);
					*PY_ += (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[6]);
					accelXC = (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[8]);
					accelYC = (Sint16)SDLNet_Read16(&packet_state_out[network_delay]->data[10]);
				}
			}

			/*Street-Fighter codes*/
			JE_SFCodes(playerNum_, *PX_, *PY_, *mouseX_, *mouseY_, pItems_);

			if (moveOk)
			{

				/* END OF MOVEMENT ROUTINES */

				/*Linking Routines*/

				if (twoPlayerMode && !twoPlayerLinked && *PX_ == *mouseX_ && *PY_ == *mouseY_
				    && abs(PX - PXB) < 8 && abs(PY - PYB) < 8
				    && playerAlive && playerAliveB && !galagaMode)
				{
					twoPlayerLinked = true;
				}

				if (playerNum_ == 1 && (button[3-1] || button[2-1]) && !galagaMode)
					twoPlayerLinked = false;

				if (twoPlayerMode && twoPlayerLinked && playerNum_ == 2
				    && (*PX_ != *mouseX_ || *PY_ != *mouseY_))
				{
					if (button[0])
					{
						if (link_gun_analog)
						{
							linkGunDirec = link_gun_angle;
						}
						else
						{
							if (abs(*PX_ - *mouseX_) > abs(*PY_ - *mouseY_))
								tempR = (*PX_ - *mouseX_ > 0) ? M_PI_2 : (M_PI + M_PI_2);
							else
								tempR = (*PY_ - *mouseY_ > 0) ? 0 : M_PI;
							
							tempR2 = linkGunDirec - tempR;
							
							if (fabsf(linkGunDirec - tempR) < 0.3f)
								linkGunDirec = tempR;
							else if (linkGunDirec < tempR && linkGunDirec - tempR > -3.24f)
								linkGunDirec += 0.2f;
							else if (linkGunDirec - tempR < M_PI)
								linkGunDirec -= 0.2f;
							else
								linkGunDirec += 0.2f;
						}
						
						if (linkGunDirec >= (2 * M_PI))
							linkGunDirec -= (2 * M_PI);
						else if (linkGunDirec < 0)
							linkGunDirec += (2 * M_PI);
					}
					else if (!galagaMode)
					{
						twoPlayerLinked = false;
					}
				}

				leftOptionIsSpecial  = options[option1Item].tr;
				rightOptionIsSpecial = options[option2Item].tr;

			} /*if (*playerAlive_) ...*/
		} /*if (!endLevel) ...*/


		if (levelEnd > 0 &&
		    !*playerAlive_ && (!twoPlayerMode || !playerAliveB))
			reallyEndLevel = true;
		/* End Level Fade-Out */
		if (*playerAlive_ && endLevel)
		{

			if (levelEnd == 0)
			{
				reallyEndLevel = true;
			}
			else
			{
				*PY_ -= levelEndWarp;
				if (*PY_ < -200)
					reallyEndLevel = true;

				tempI = 1;
				tempW2 = *PY_;
				tempI2 = abs(41 - levelEnd);
				if (tempI2 > 20)
					tempI2 = 20;

				for (int z = 1; z <= tempI2; z++)
				{
					tempW2 += tempI;
					tempI++;
				}

				for (int z = 1; z <= tempI2; z++)
				{
					tempW2 -= tempI;
					tempI--;
					if (tempW2 > 0 && tempW2 < 170)
					{
						if (shipGr_ == 0)
						{
							blit_sprite2x2(VGAScreen, *PX_ - 17, tempW2 - 7, *shapes9ptr_, 13);
							blit_sprite2x2(VGAScreen, *PX_ + 7 , tempW2 - 7, *shapes9ptr_, 51);
						}
						else if (shipGr_ == 1)
						{
							blit_sprite2x2(VGAScreen, *PX_ - 17, tempW2 - 7, *shapes9ptr_, 220);
							blit_sprite2x2(VGAScreen, *PX_ + 7 , tempW2 - 7, *shapes9ptr_, 222);
						}
						else
						{
							blit_sprite2x2(VGAScreen, *PX_ - 5, tempW2 - 7, *shapes9ptr_, shipGr_);
						}
					}
				}
			}
		}

		if (play_demo)
			JE_dString(115, 10, miscText[7], SMALL_FONT_SHAPES); // insert coin

		if (*playerAlive_ && !endLevel)
		{

			if (!twoPlayerLinked || playerNum_ < 2)
			{

				if (!twoPlayerMode || shipGr2 != 0)
				{
					option1X = *mouseX_ - 14;
					option1Y = *mouseY_;

					if (rightOptionIsSpecial == 0)
					{
						option2X = *mouseX_ + 16;
						option2Y = *mouseY_;
					}
				}

				if (*stopWaitX_ > 0)
					(*stopWaitX_)--;
				else {
					*stopWaitX_ = 2;
					if (*lastTurn_ < 0)
						(*lastTurn_)++;
					else
						if (*lastTurn_ > 0)
							(*lastTurn_)--;
				}

				if (*stopWaitY_ > 0)
					(*stopWaitY_)--;
				else {
					*stopWaitY_ = 1;
					if (*lastTurn2_ < 0)
						(*lastTurn2_)++;
					else
						if (*lastTurn2_ > 0)
							(*lastTurn2_)--;
				}

				*lastTurn_ += accelYC;
				*lastTurn2_ += accelXC;

				if (*lastTurn2_ < -4)
					*lastTurn2_ = -4;
				if (*lastTurn2_ > 4)
					*lastTurn2_ = 4;
				if (*lastTurn_ < -4)
					*lastTurn_ = -4;
				if (*lastTurn_ > 4)
					*lastTurn_ = 4;

				*PX_ += *lastTurn2_;
				*PY_ += *lastTurn_;


				/*Option History for special new sideships*/
				if (playerHNotReady)
					JE_resetPlayerH();
				else
					if ((playerNum_ == 1 && !twoPlayerMode)
					    || (playerNum_ == 2 && twoPlayerMode))
						if (*PX_ - *mouseX_ != 0 || *PY_ - *mouseY_ != 0)
						{ /*Option History*/
							for (temp = 0; temp < 19; temp++)
							{
								playerHX[temp] = playerHX[temp + 1];
								playerHY[temp] = playerHY[temp + 1];
							}
							playerHX[20-1] = *PX_;
							playerHY[20-1] = *PY_;
						}

			} else {  /*twoPlayerLinked*/
				if (shipGr_ == 0)
					*PX_ = PX - 1;
				else
					*PX_ = PX;
				*PY_ = PY + 8;
				*lastTurn2_ = lastTurn2;
				*lastTurn_ = 4;

				shotMultiPos[5-1] = 0;
				JE_initPlayerShot(0, 5, *PX_ + 1 + roundf(sinf(linkGunDirec + 0.2f) * 26), *PY_ + roundf(cosf(linkGunDirec + 0.2f) * 26),
				                  *mouseX_, *mouseY_, 148, playerNum_);
				shotMultiPos[5-1] = 0;
				JE_initPlayerShot(0, 5, *PX_ + 1 + roundf(sinf(linkGunDirec - 0.2f) * 26), *PY_ + roundf(cosf(linkGunDirec - 0.2f) * 26),
				                  *mouseX_, *mouseY_, 148, playerNum_);
				shotMultiPos[5-1] = 0;
				JE_initPlayerShot(0, 5, *PX_ + 1 + roundf(sinf(linkGunDirec) * 26), *PY_ + roundf(cosf(linkGunDirec) * 26),
				                  *mouseX_, *mouseY_, 147, playerNum_);

				if (shotRepeat[2-1] > 0)
					shotRepeat[2-1]--;
				else
					if (button[1-1])
					{
						shotMultiPos[2-1] = 0;
						JE_initPlayerShot(0, 2, *PX_ + 1 + roundf(sinf(linkGunDirec) * 20), *PY_ + roundf(cosf(linkGunDirec) * 20),
						                  *mouseX_, *mouseY_, linkGunWeapons[pItems_[P_REAR]-1], playerNum_);
						playerShotData[b].shotXM = -roundf(sinf(linkGunDirec) * playerShotData[b].shotYM);
						playerShotData[b].shotYM = -roundf(cosf(linkGunDirec) * playerShotData[b].shotYM);

						switch (pItems_[P_REAR])
						{
							case 27:
							case 32:
							case 10:
								temp = roundf(linkGunDirec * (16 / (2 * M_PI)));  /*16 directions*/
								playerShotData[b].shotGr = linkMultiGr[temp];
								break;
							case 28:
							case 33:
							case 11:
								temp = roundf(linkGunDirec * (16 / (2 * M_PI)));  /*16 directions*/
								playerShotData[b].shotGr = linkSonicGr[temp];
								break;
							case 30:
							case 35:
							case 14:
								if (linkGunDirec > M_PI_2 && linkGunDirec < M_PI + M_PI_2)
								{
									playerShotData[b].shotYC = 1;
								}
								break;
							case 38:
							case 22:
								temp = roundf(linkGunDirec * (16 / (2 * M_PI)));  /*16 directions*/
								playerShotData[b].shotGr = linkMult2Gr[temp];
								break;
						}

					}
			}

		}  /*moveOK*/

		if (!endLevel)
		{
			if (*PX_ > 256)
			{
				*PX_ = 256;
				constantLastX = -constantLastX;
			}
			if (*PX_ < 40)
			{
				*PX_ = 40;
				constantLastX = -constantLastX;
			}

			if (isNetworkGame && playerNum_ == 1)
			{
				if (*PY_ > 154)
					*PY_ = 154;
			} else {
				if (*PY_ > 160)
					*PY_ = 160;
			}

			if (*PY_ < 10)
				*PY_ = 10;

			tempI2 = *lastTurn2_ / 2;
			tempI2 += (*PX_ - *mouseX_) / 6;

			if (tempI2 < -2)
				tempI2 = -2;
			else
				if (tempI2 > 2)
					tempI2 = 2;


			tempI  = tempI2 * 2 + shipGr_;

			tempI4 = *PY_ - *lastPY2_;
			if (tempI4 < 1)
				tempI4 = 0;
			explosionFollowAmountX = *PX_ - *lastPX2_;
			explosionFollowAmountY = tempI4;
			*lastPX2_ = *PX_;
			*lastPY2_ = *PY_;

			if (shipGr_ == 0)
			{
				if (background2)
				{
					blit_sprite2x2_darken(VGAScreen, *PX_ - 17 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, tempI + 13);
					blit_sprite2x2_darken(VGAScreen, *PX_ + 7 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, tempI + 51);
					if (superWild)
					{
						blit_sprite2x2_darken(VGAScreen, *PX_ - 16 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, tempI + 13);
						blit_sprite2x2_darken(VGAScreen, *PX_ + 6 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, tempI + 51);
					}
				}
			} else
				if (shipGr_ == 1)
				{
					if (background2)
					{
						blit_sprite2x2_darken(VGAScreen, *PX_ - 17 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, 220);
						blit_sprite2x2_darken(VGAScreen, *PX_ + 7 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, 222);
					}
				} else {
					if (background2)
					{
						blit_sprite2x2_darken(VGAScreen, *PX_ - 5 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, tempI);
						if (superWild)
						{
							blit_sprite2x2_darken(VGAScreen, *PX_ - 4 - mapX2Ofs + 30, *PY_ - 7 + shadowyDist, *shapes9ptr_, tempI);
						}
					}
				}

			if (*playerInvulnerable_ > 0)
			{
				(*playerInvulnerable_)--;

				if (shipGr_ == 0)
				{
					blit_sprite2x2_blend(VGAScreen, *PX_ - 17, *PY_ - 7, *shapes9ptr_, tempI + 13);
					blit_sprite2x2_blend(VGAScreen, *PX_ + 7 , *PY_ - 7, *shapes9ptr_, tempI + 51);
				} else
					if (shipGr_ == 1)
					{
						blit_sprite2x2_blend(VGAScreen, *PX_ - 17, *PY_ - 7, *shapes9ptr_, 220);
						blit_sprite2x2_blend(VGAScreen, *PX_ + 7 , *PY_ - 7, *shapes9ptr_, 222);
					} else
						blit_sprite2x2_blend(VGAScreen, *PX_ - 5, *PY_ - 7, *shapes9ptr_, tempI);

			} else {
				if (shipGr_ == 0)
				{
					blit_sprite2x2(VGAScreen, *PX_ - 17, *PY_ - 7, *shapes9ptr_, tempI + 13);
					blit_sprite2x2(VGAScreen, *PX_ + 7, *PY_ - 7, *shapes9ptr_, tempI + 51);
				} else
					if (shipGr_ == 1)
					{
						blit_sprite2x2(VGAScreen, *PX_ - 17, *PY_ - 7, *shapes9ptr_, 220);
						blit_sprite2x2(VGAScreen, *PX_ + 7, *PY_ - 7, *shapes9ptr_, 222);
						switch (tempI)
						{
							case 5:
								blit_sprite2(VGAScreen, *PX_ - 17, *PY_ + 7, *shapes9ptr_, 40);
								tempW = *PX_ - 7;
								tempI2 = -2;
								break;
							case 3:
								blit_sprite2(VGAScreen, *PX_ - 17, *PY_ + 7, *shapes9ptr_, 39);
								tempW = *PX_ - 7;
								tempI2 = -1;
								break;
							case 1:
								tempI2 = 0;
								break;
							case -1:
								blit_sprite2(VGAScreen, *PX_ + 19, *PY_ + 7, *shapes9ptr_, 58);
								tempW = *PX_ + 9;
								tempI2 = 1;
								break;
							case -3:
								blit_sprite2(VGAScreen, *PX_ + 19, *PY_ + 7, *shapes9ptr_, 59);
								tempW = *PX_ + 9;
								tempI2 = 2;
								break;
						}
						if (tempI2 != 0)
						{  /*NortSparks*/
							if (shotRepeat[10-1] > 0)
								shotRepeat[10-1]--;
							else {
									JE_initPlayerShot(0, 10, tempW + (mt_rand() % 8) - 4, (*PY_) + (mt_rand() % 8) - 4, *mouseX_, *mouseY_, 671, 1);
									shotRepeat[10-1] = abs(tempI2) - 1;
							}
						}
					} else
						blit_sprite2x2(VGAScreen, *PX_ - 5, *PY_ - 7, *shapes9ptr_, tempI);
			}
		}  /*endLevel*/

		/*Options Location*/
		if (playerNum_ == 2 && shipGr_ == 0)
		{
			if (rightOptionIsSpecial == 0)
			{
				option2X = *PX_ + 17 + tempI;
				option2Y = *PY_;
			}

			option1X = *PX_ - 14 + tempI;
			option1Y = *PY_;
		}

		if (moveOk)
		{

			if (*playerAlive_)
			{
				if (!endLevel)
				{
					*PXChange_ = *PX_ - lastPXShotMove;
					*PYChange_ = *PY_ - lastPYShotMove;

					/* PLAYER SHOT Change */
					if (button[4-1])
					{
						portConfigChange = true;
						if (portConfigDone)
						{

							shotMultiPos[2-1] = 0;

							if (superArcadeMode != SA_NONE && superArcadeMode <= SA_NORTSHIPZ)
							{
								shotMultiPos[9-1] = 0;
								shotMultiPos[11-1] = 0;
								if (pItems[P_SPECIAL] == SASpecialWeapon[superArcadeMode-1])
								{
									pItems[P_SPECIAL] = SASpecialWeaponB[superArcadeMode-1];
									portConfig[2-1] = 2;
								}
								else
								{
									pItems[P_SPECIAL] = SASpecialWeapon[superArcadeMode-1];
									portConfig[2-1] = 1;
								}
							}
							else
							{
								portConfig[2-1]++;
								JE_portConfigs();
								if (portConfig[2-1] > tempW)
									portConfig[2-1] = 1;
							}

							JE_drawPortConfigButtons();
							portConfigDone = false;
						}
					}

					/* PLAYER SHOT Creation */

					/*SpecialShot*/
					if (!galagaMode)
						JE_doSpecialShot(playerNum_, armorLevel_, shield_);

					/*Normal Main Weapons*/
					if (!(twoPlayerLinked && playerNum_ == 2))
					{
						if (!twoPlayerMode)
						{
							min = 1;
							max = 2;
						} else
							switch (playerNum_)
							{
								case 1:
									min = 1;
									max = 1;
									break;
								case 2:
									min = 2;
									max = 2;
									break;
							}
							for (temp = min - 1; temp < max; temp++)
								if (pItems_[temp] > 0)
								{
									if (shotRepeat[temp] > 0)
										shotRepeat[temp]--;
									else
										if (button[1-1])
											JE_initPlayerShot(pItems_[temp], temp + 1, *PX_, *PY_, *mouseX_, *mouseY_,
											                  weaponPort[pItems_[temp]].op[portConfig[temp]-1]
											                  [portPower[temp] * !galagaMode + galagaMode - 1],
											                  playerNum_);
								}
					}

					/*Super Charge Weapons*/
					if (playerNum_ == 2)
					{

						if (!twoPlayerLinked)
							blit_sprite2(VGAScreen, *PX_ + (shipGr_ == 0) + 1, *PY_ - 13, eShapes6, 77 + chargeLevel + chargeGr * 19);

						if (chargeGrWait > 0)
						{
							chargeGrWait--;
						} else {
							chargeGr++;
							if (chargeGr == 4)
								chargeGr = 0;
							chargeGrWait = 3;
						}

						if (chargeLevel > 0)
						{
							JE_c_bar(269, 107 + (chargeLevel - 1) * 3, 275, 108 + (chargeLevel - 1) * 3, 193);
						}

						if (chargeWait > 0)
						{
							chargeWait--;
						} else {
							if (chargeLevel < chargeMax)
								chargeLevel++;
							chargeWait = 28 - portPower[2-1] * 2;
							if (difficultyLevel > 3)
								chargeWait -= 5;
						}

						if (chargeLevel > 0)
						{
							JE_c_bar(269, 107 + (chargeLevel - 1) * 3, 275, 108 + (chargeLevel - 1) * 3, 204);
						}

						if (shotRepeat[6-1] > 0)
							shotRepeat[6-1]--;
						else
							if (button[1-1]
							    && (!twoPlayerLinked || chargeLevel > 0))
							{
								shotMultiPos[6-1] = 0;
								JE_initPlayerShot(16, 6, *PX_, *PY_, *mouseX_, *mouseY_,
								                  chargeGunWeapons[pItemsPlayer2[P_REAR]-1] + chargeLevel, playerNum_);

								if (chargeLevel > 0)
								{
									JE_c_bar(269, 107 + (chargeLevel - 1) * 3, 275, 108 + (chargeLevel - 1) * 3, 193);
								}

								chargeLevel = 0;
								chargeWait = 30 - portPower[2-1] * 2;
							}
					}


					/*SUPER BOMB*/
					temp = playerNum_;
					if (temp == 0)
						temp = 1;  /*Get whether player 1 or 2*/

					if (superBomb[temp-1] > 0)
					{
						if (shotRepeat[temp-1 + 6] > 0)
							shotRepeat[temp-1 + 6]--;
						else {
							if (button[3-1] || button[2-1])
							{
								superBomb[temp-1]--;
								shotMultiPos[temp-1 + 6] = 0;
								JE_initPlayerShot(16, temp + 6, *PX_, *PY_, *mouseX_, *mouseY_, 535, playerNum_);
							}
						}
					}


					/*Special option following*/
					switch (leftOptionIsSpecial)
					{
						case 1:
						case 3:
							option1X = playerHX[10-1];
							option1Y = playerHY[10-1];
							break;
						case 2:
							option1X = *PX_;
							option1Y = *PY_ - 20;
							if (option1Y < 10)
								option1Y = 10;
							break;
						case 4:
							if (rightOptionIsSpecial == 4)
								optionSatelliteRotate += 0.2f;
							else
								optionSatelliteRotate += 0.15f;
							option1X = *PX_ + roundf(sinf(optionSatelliteRotate) * 20);
							option1Y = *PY_ + roundf(cosf(optionSatelliteRotate) * 20);
							break;
					}


					switch (rightOptionIsSpecial)
					{
						case 4:
							if (leftOptionIsSpecial != 4)
								optionSatelliteRotate += 0.15f;
							option2X = *PX_ - roundf(sinf(optionSatelliteRotate) * 20);
							option2Y = *PY_ - roundf(cosf(optionSatelliteRotate) * 20);
							break;
						case 1:
						case 3:
							option2X = playerHX[1-1];
							option2Y = playerHY[1-1];
							break;
						case 2:
							if (!optionAttachmentLinked)
							{
								option2Y += optionAttachmentMove / 2;
								if (optionAttachmentMove >= -2)
								{
									if (optionAttachmentReturn)
										temp = 2;
									else
										temp = 0;
									
									if (option2Y > (*PY_ - 20) + 5)
									{
										temp = 2;
										optionAttachmentMove -= 1 + optionAttachmentReturn;
									}
									else if (option2Y > (*PY_ - 20) - 0)
									{
										temp = 3;
										if (optionAttachmentMove > 0)
											optionAttachmentMove--;
										else
											optionAttachmentMove++;
									}
									else if (option2Y > (*PY_ - 20) - 5)
									{
										temp = 2;
										optionAttachmentMove++;
									}
									else if (optionAttachmentMove < 2 + optionAttachmentReturn * 4)
									{
										optionAttachmentMove += 1 + optionAttachmentReturn;
									}

									if (optionAttachmentReturn)
										temp = temp * 2;
									if (abs(option2X - *PX_ < temp))
										temp = 1;

									if (option2X > *PX_)
										option2X -= temp;
									else if (option2X < *PX_)
										option2X += temp;

									if (abs(option2Y - (*PY_ - 20)) + abs(option2X - *PX_) < 8)
									{
										optionAttachmentLinked = true;
										soundQueue[2] = S_CLINK;
									}

									if (button[3-1])
										optionAttachmentReturn = true;
								}
								else
								{
									optionAttachmentMove += 1 + optionAttachmentReturn;
									JE_setupExplosion(option2X + 1, option2Y + 10, 0, 0, false, false);
								}
							}
							else
							{
								option2X = *PX_;
								option2Y = *PY_ - 20;
								if (button[3-1])
								{
									optionAttachmentLinked = false;
									optionAttachmentReturn = false;
									optionAttachmentMove = -20;
									soundQueue[3] = S_WEAPON_26;
								}
							}

							if (option2Y < 10)
								option2Y = 10;
							break;
					}

					if (playerNum_ == 2 || !twoPlayerMode)
					{
						if (options[option1Item].wport > 0)
						{
							if (shotRepeat[3-1] > 0)
							{
								shotRepeat[3-1]--;
							}
							else
							{
								if (option1Ammo >= 0)
								{
									if (option1AmmoRechargeWait > 0)
									{
										option1AmmoRechargeWait--;
									}
									else
									{
										option1AmmoRechargeWait = option1AmmoRechargeWaitMax;
										if (option1Ammo < options[option1Item].ammo)
											option1Ammo++;
										JE_barDrawDirect (284, option1Draw + 13, option1AmmoMax, 112, option1Ammo, 2, 2); /*Option1Ammo*/
									}
								}
								
								if (option1Ammo > 0)
								{
									if (button[2-1])
									{
										JE_initPlayerShot(options[option1Item].wport, 3,
										                  option1X, option1Y,
										                  *mouseX_, *mouseY_,
										                  options[option1Item].wpnum + optionCharge1,
										                  playerNum_);
										if (optionCharge1 > 0)
											shotMultiPos[3-1] = 0;
										optionAni1Go = true;
										optionCharge1Wait = 20;
										optionCharge1 = 0;
										option1Ammo--;
										JE_c_bar(284, option1Draw + 13, 312, option1Draw + 15, 0);
										JE_barDrawDirect(284, option1Draw + 13, option1AmmoMax, 112, option1Ammo, 2, 2);
									}
								}
								else if (option1Ammo < 0)
								{
									if (button[1-1] || button[2-1])
									{
										JE_initPlayerShot(options[option1Item].wport, 3,
										                  option1X, option1Y,
										                  *mouseX_, *mouseY_,
										                  options[option1Item].wpnum + optionCharge1,
										                  playerNum_);
										if (optionCharge1 > 0)
											shotMultiPos[3-1] = 0;
										optionCharge1Wait = 20;
										optionCharge1 = 0;
										optionAni1Go = true;
									}
								}
							}
						}
						
						if (options[option2Item].wport > 0)
						{
							if (shotRepeat[4-1] > 0)
							{
								shotRepeat[4-1]--;
							}
							else
							{
								if (option2Ammo >= 0)
								{
									if (option2AmmoRechargeWait > 0)
									{
										option2AmmoRechargeWait--;
									}
									else
									{
										option2AmmoRechargeWait = option2AmmoRechargeWaitMax;
										if (option2Ammo < options[option2Item].ammo)
											option2Ammo++;
										JE_barDrawDirect(284, option2Draw + 13, option2AmmoMax, 112, option2Ammo, 2, 2);
									}
								}
								
								if (option2Ammo > 0)
								{
									if (button[3-1])
									{
										JE_initPlayerShot(options[option2Item].wport, 4, option2X, option2Y,
										                  *mouseX_, *mouseY_,
										                  options[option2Item].wpnum + optionCharge2,
										                  playerNum_);
										if (optionCharge2 > 0)
										{
											shotMultiPos[4-1] = 0;
											optionCharge2 = 0;
										}
										optionCharge2Wait = 20;
										optionCharge2 = 0;
										optionAni2Go = true;
										option2Ammo--;
										JE_c_bar(284, option2Draw + 13, 312, option2Draw + 15, 0);
										JE_barDrawDirect(284, option2Draw + 13, option2AmmoMax, 112, option2Ammo, 2, 2);
									}
								}
								else if (option2Ammo < 0)
								{
									if (button[1-1] || button[3-1])
									{
										JE_initPlayerShot(options[option2Item].wport, 4, option2X, option2Y,
										                  *mouseX_, *mouseY_,
										                  options[option2Item].wpnum + optionCharge2,
										                  playerNum_);
										if (optionCharge2 > 0)
										{
											shotMultiPos[4-1] = 0;
											optionCharge2 = 0;
										}
										optionCharge2Wait = 20;
										optionAni2Go = true;
									}
								}
							}
						}
					}
				}  /* !endLevel */
			}

		}

		/* Draw Floating Options */
		if ((playerNum_ == 2 || !twoPlayerMode) && !endLevel)
		{
			if (options[option1Item].option > 0)
			{

				if (optionAni1Go)
				{
					optionAni1++;
					if (optionAni1 > options[option1Item].ani)
					{
						optionAni1 = 1;
						optionAni1Go = options[option1Item].option == 1;
					}
				}

				if (leftOptionIsSpecial == 1 || leftOptionIsSpecial == 2)
					blit_sprite2x2(VGAScreen, option1X - 6, option1Y, eShapes6, options[option1Item].gr[optionAni1-1] + optionCharge1);
				else
					blit_sprite2(VGAScreen, option1X, option1Y, shapes9, options[option1Item].gr[optionAni1-1] + optionCharge1);
			}

			if (options[option2Item].option > 0)
			{

				if (optionAni2Go)
				{
					optionAni2++;
					if (optionAni2 > options[option2Item].ani)
					{
						optionAni2 = 1;
						optionAni2Go = options[option2Item].option == 1;
					}
				}

				if (rightOptionIsSpecial == 1 || rightOptionIsSpecial == 2)
					blit_sprite2x2(VGAScreen, option2X - 6, option2Y, eShapes6, options[option2Item].gr[optionAni2-1] + optionCharge2);
				else
					blit_sprite2(VGAScreen, option2X, option2Y, shapes9, options[option2Item].gr[optionAni2-1] + optionCharge2);
			}

			optionCharge1Wait--;
			if (optionCharge1Wait == 0)
			{
				if (optionCharge1 < options[option1Item].pwr)
					optionCharge1++;
				optionCharge1Wait = 20;
			}
			optionCharge2Wait--;
			if (optionCharge2Wait == 0)
			{
				if (optionCharge2 < options[option2Item].pwr)
					optionCharge2++;
				optionCharge2Wait = 20;
			}
		}
}

void JE_mainGamePlayerFunctions( void )
{
	/*PLAYER MOVEMENT/MOUSE ROUTINES*/

	if (endLevel && levelEnd > 0)
	{
		levelEnd--;
		levelEndWarp++;
	}

	/*Reset Street-Fighter commands*/
	memset(SFExecuted, 0, sizeof(SFExecuted));

	portConfigChange = false;

	if (twoPlayerMode)
	{

		JE_playerMovement(!galagaMode ? inputDevice[0] : 0, 1, shipGr, shipGrPtr,
		                  &armorLevel, &baseArmor,
		                  &shield, &shieldMax,
		                  &playerInvulnerable1,
		                  &PX, &PY, &lastPX2, &lastPY2, &PXChange, &PYChange,
		                  &lastTurn, &lastTurn2, &stopWaitX, &stopWaitY,
		                  &mouseX, &mouseY,
		                  &playerAlive, &playerStillExploding, pItems);
		JE_playerMovement(!galagaMode ? inputDevice[1] : 0, 2, shipGr2, shipGr2ptr,
		                  &armorLevel2, &baseArmor2,
		                  &shield2, &shieldMax2,
		                  &playerInvulnerable2,
		                  &PXB, &PYB, &lastPX2B, &lastPY2B, &PXChangeB, &PYChangeB,
		                  &lastTurnB, &lastTurn2B, &stopWaitXB, &stopWaitYB,
		                  &mouseXB, &mouseYB,
		                  &playerAliveB, &playerStillExploding2, pItemsPlayer2);
	} else {
		JE_playerMovement(0, 1, shipGr, shipGrPtr,
		                  &armorLevel, &baseArmor,
		                  &shield, &shieldMax,
		                  &playerInvulnerable1,
		                  &PX, &PY, &lastPX2, &lastPY2, &PXChange, &PYChange,
		                  &lastTurn, &lastTurn2, &stopWaitX, &stopWaitY,
		                  &mouseX, &mouseY,
		                  &playerAlive, &playerStillExploding, pItems);
	}

	/* == Parallax Map Scrolling == */
	if (twoPlayerMode)
	{
		tempX = (PX + PXB) / 2;
	} else {
		tempX = PX;
	}

	tempW = floorf((260.0f - (tempX - 36.0f)) / (260.0f - 36.0f) * (24.0f * 3.0f) - 1.0f);
	mapX3Ofs   = tempW;
	mapX3Pos   = mapX3Ofs % 24;
	mapX3bpPos = 1 - (mapX3Ofs / 24);

	mapX2Ofs   = (tempW * 2) / 3;
	mapX2Pos   = mapX2Ofs % 24;
	mapX2bpPos = 1 - (mapX2Ofs / 24);

	oldMapXOfs = mapXOfs;
	mapXOfs    = mapX2Ofs / 2;
	mapXPos    = mapXOfs % 24;
	mapXbpPos  = 1 - (mapXOfs / 24);

	if (background3x1)
	{
		mapX3Ofs = mapXOfs;
		mapX3Pos = mapXPos;
		mapX3bpPos = mapXbpPos - 1;
	}
}

const char *JE_getName( JE_byte pnum )
{
	if (pnum == thisPlayerNum && network_player_name[0] != '\0')
		return network_player_name;
	else if (network_opponent_name[0] != '\0')
		return network_opponent_name;
	
	return miscText[47 + pnum];
}

void JE_playerCollide( JE_integer *PX_, JE_integer *PY_, JE_integer *lastTurn_, JE_integer *lastTurn2_,
                       JE_longint *score_, JE_integer *armorLevel_, JE_shortint *shield_, JE_boolean *playerAlive_,
                       JE_byte *playerStillExploding_, JE_byte playerNum_, JE_byte playerInvulnerable_ )
{
	char tempStr[256];
	
	for (int z = 0; z < 100; z++)
	{
		if (enemyAvail[z] != 1)
		{
			tempI3 = enemy[z].ex + enemy[z].mapoffset;
			
			if (abs(*PX_ - tempI3) < 12 && abs(*PY_ - enemy[z].ey) < 14)
			{   /*Collide*/
				tempI4 = enemy[z].evalue;
				if (tempI4 > 29999)
				{
					if (tempI4 == 30000)
					{
						*score_ += 100;
						
						if (!galagaMode)
						{
							if (purpleBallsRemaining[playerNum_ - 1] > 1)
							{
								purpleBallsRemaining[playerNum_ - 1]--;
							}
							else
							{
								JE_powerUp(playerNum_);
								JE_calcPurpleBall(playerNum_);
							}
						}
						else
						{
							if (twoPlayerMode)
								*score_ += 2400;
							twoPlayerMode = true;
							twoPlayerLinked = true;
							portPower[2-1] = 1;
							armorLevel2 = 10;
							playerAliveB = true;
						}
						enemyAvail[z] = 1;
						soundQueue[7] = S_POWERUP;
					}
					else if (superArcadeMode != SA_NONE && tempI4 > 30000)
					{
						shotMultiPos[1-1] = 0;
						shotRepeat[1-1] = 10;
						
						tempW = SAWeapon[superArcadeMode-1][tempI4 - 30000-1];
						
						if (tempW == pItems[P_FRONT])
						{
							*score_ += 1000;
							if (portPower[1-1] < 11)
								JE_powerUp(1);
							JE_calcPurpleBall(playerNum_);
						}
						else if (purpleBallsRemaining[playerNum_-1] > 1)
						{
							purpleBallsRemaining[playerNum_-1]--;
						}
						else
						{
							JE_powerUp(playerNum_);
							JE_calcPurpleBall(playerNum_);
						}
						
						pItems[P_FRONT] = tempW;
						*score_ += 200;
						soundQueue[7] = S_POWERUP;
						enemyAvail[z] = 1;
					}
					else if (tempI4 > 32100)
					{
						if (playerNum_ == 1)
						{
							*score_ += 250;
							pItems[P_SPECIAL] = tempI4 - 32100;
							shotMultiPos[9-1] = 0;
							shotRepeat[9-1] = 10;
							shotMultiPos[11-1] = 0;
							shotRepeat[11-1] = 0;
							
							if (isNetworkGame)
								sprintf(tempStr, "%s %s %s", JE_getName(1), miscTextB[4-1], special[tempI4 - 32100].name);
							else if (twoPlayerMode)
								sprintf(tempStr, "%s %s", miscText[43-1], special[tempI4 - 32100].name);
							else
								sprintf(tempStr, "%s %s", miscText[64-1], special[tempI4 - 32100].name);
							JE_drawTextWindow(tempStr);
							soundQueue[7] = S_POWERUP;
							enemyAvail[z] = 1;
						}
					}
					else if (tempI4 > 32000)
					{
						if (playerNum_ == 2)
						{
							enemyAvail[z] = 1;
							if (isNetworkGame)
								sprintf(tempStr, "%s %s %s", JE_getName(2), miscTextB[4-1], options[tempI4 - 32000].name);
							else
								sprintf(tempStr, "%s %s", miscText[44-1], options[tempI4 - 32000].name);
							JE_drawTextWindow(tempStr);
							
							if (tempI4 - 32000 != pItemsPlayer2[P2_SIDEKICK_TYPE])
								pItemsPlayer2[P2_SIDEKICK_MODE] = 100;
							pItemsPlayer2[P2_SIDEKICK_TYPE] = tempI4 - 32000;
							
							if (pItemsPlayer2[P2_SIDEKICK_MODE] < 103)
								pItemsPlayer2[P2_SIDEKICK_MODE]++;
							
							temp = pItemsPlayer2[P2_SIDEKICK_MODE] - 100;
							pItemsPlayer2[P_LEFT_SIDEKICK] = optionSelect[pItemsPlayer2[P2_SIDEKICK_TYPE]][temp-1][1-1];
							pItemsPlayer2[P_RIGHT_SIDEKICK] = optionSelect[pItemsPlayer2[P2_SIDEKICK_TYPE]][temp-1][2-1];
							
							
							shotMultiPos[3-1] = 0;
							shotMultiPos[4-1] = 0;
							tempScreenSeg = VGAScreenSeg;
							JE_drawOptions();
							soundQueue[7] = S_POWERUP;
							tempScreenSeg = VGAScreen;
						}
						else if (onePlayerAction)
						{
							enemyAvail[z] = 1;
							sprintf(tempStr, "%s %s", miscText[64-1], options[tempI4 - 32000].name);
							JE_drawTextWindow(tempStr);
							pItems[P_LEFT_SIDEKICK] = tempI4 - 32000;
							pItems[P_RIGHT_SIDEKICK] = tempI4 - 32000;
							shotMultiPos[3-1] = 0;
							shotMultiPos[4-1] = 0;
							tempScreenSeg = VGAScreenSeg;
							JE_drawOptions();
							soundQueue[7] = S_POWERUP;
						}
						if (enemyAvail[z] == 1)
							*score_ += 250;
					}
					else if (tempI4 > 31000)
					{
						*score_ += 250;
						if (playerNum_ == 2)
						{
							if (isNetworkGame)
								sprintf(tempStr, "%s %s %s", JE_getName(2), miscTextB[4-1], weaponPort[tempI4 - 31000].name);
							else
								sprintf(tempStr, "%s %s", miscText[44-1], weaponPort[tempI4 - 31000].name);
							JE_drawTextWindow(tempStr);
							pItemsPlayer2[P_REAR] = tempI4 - 31000;
							shotMultiPos[2-1] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;
						}
						else if (onePlayerAction)
						{
							sprintf(tempStr, "%s %s", miscText[64-1], weaponPort[tempI4 - 31000].name);
							JE_drawTextWindow(tempStr);
							pItems[P_REAR] = tempI4 - 31000;
							shotMultiPos[2] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;
							if (portPower[2-1] == 0)
								portPower[2-1] = 1;
						}
					}
					else if (tempI4 > 30000)
					{
						if (playerNum_ == 1 && twoPlayerMode)
						{
							if (isNetworkGame)
								sprintf(tempStr, "%s %s %s", JE_getName(1), miscTextB[4-1], weaponPort[tempI4 - 30000].name);
							else
								sprintf(tempStr, "%s %s", miscText[43-1], weaponPort[tempI4 - 30000].name);
							JE_drawTextWindow(tempStr);
							pItems[P_FRONT] = tempI4 - 30000;
							shotMultiPos[1-1] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;
						}
						else if (onePlayerAction)
						{
							sprintf(tempStr, "%s %s", miscText[64-1], weaponPort[tempI4 - 30000].name);
							JE_drawTextWindow(tempStr);
							pItems[P_FRONT] = tempI4 - 30000;
							shotMultiPos[1-1] = 0;
							enemyAvail[z] = 1;
							soundQueue[7] = S_POWERUP;
						}
						
						if (enemyAvail[z] == 1)
						{
							pItems[P_SPECIAL] = specialArcadeWeapon[tempI4 - 30000-1];
							if (pItems[P_SPECIAL] > 0)
							{
								shotMultiPos[9-1] = 0;
								shotRepeat[9-1] = 0;
								shotMultiPos[11-1] = 0;
								shotRepeat[11-1] = 0;
							}
							*score_ += 250;
						}
						
					}
				}
				else if (tempI4 > 20000)
				{
					if (twoPlayerLinked)
					{
						armorLevel += (tempI4 - 20000) / 2;
						if (armorLevel > 28)
							armorLevel = 28;
						armorLevel2 += (tempI4 - 20000) / 2;
						if (armorLevel2 > 28)
							armorLevel2 = 28;
					}
					else
					{
						*armorLevel_ += tempI4 - 20000;
						if (*armorLevel_ > 28)
							*armorLevel_ = 28;
					}
					enemyAvail[z] = 1;
					VGAScreen = VGAScreenSeg; /* side-effect of game_screen */
					JE_drawArmor();
					VGAScreen = game_screen; /* side-effect of game_screen */
					soundQueue[7] = S_POWERUP;
				}
				else if (tempI4 > 10000 && enemyAvail[z] == 2)
				{
					if (!bonusLevel)
					{
						play_song(30);  /*Zanac*/
						bonusLevel = true;
						nextLevel = tempI4 - 10000;
						enemyAvail[z] = 1;
						displayTime = 150;
					}
				}
				else if (enemy[z].scoreitem)
				{
					enemyAvail[z] = 1;
					soundQueue[7] = S_ITEM;
					if (tempI4 == 1)
					{
						cubeMax++;
						soundQueue[3] = V_DATA_CUBE;
					}
					else if (tempI4 == -1)
					{
						if (isNetworkGame)
							sprintf(tempStr, "%s %s %s", JE_getName(1), miscTextB[4-1], miscText[45-1]);
						else if (twoPlayerMode)
							sprintf(tempStr, "%s %s", miscText[43-1], miscText[45-1]);
						else
							strcpy(tempStr, miscText[45-1]);
						JE_drawTextWindow(tempStr);
						JE_powerUp(1);
						soundQueue[7] = S_POWERUP;
					}
					else if (tempI4 == -2)
					{
						if (isNetworkGame)
							sprintf(tempStr, "%s %s %s", JE_getName(2), miscTextB[4-1], miscText[46-1]);
						else if (twoPlayerMode)
							sprintf(tempStr, "%s %s", miscText[44-1], miscText[46-1]);
						else
							strcpy(tempStr, miscText[46-1]);
						JE_drawTextWindow(tempStr);
						JE_powerUp(2);
						soundQueue[7] = S_POWERUP;
					}
					else if (tempI4 == -3)
					{
						shotMultiPos[5-1] = 0;
						JE_initPlayerShot(0, 5, *PX_, *PY_, mouseX, mouseY, 104, playerNum_);
						shotAvail[z] = 0;
					}
					else if (tempI4 == -4)
					{
						if (superBomb[playerNum_-1] < 10)
							superBomb[playerNum_-1]++;
					}
					else if (tempI4 == -5)
					{
						pItems[P_FRONT] = 25;         /*HOT DOG!*/
						pItems[P_REAR] = 26;
						pItemsPlayer2[P_REAR] = 26;
						memcpy(pItemsBack2, pItems, sizeof(pItemsBack2));
						portConfig[2-1] = 1;
						memset(shotMultiPos, 0, sizeof(shotMultiPos));
					}
					else if (twoPlayerLinked)
					{
						score += tempI4 / 2;
						score2 += tempI4 / 2;
					}
					else
					{
						*score_ += tempI4;
					}
					JE_setupExplosion(tempI3, enemy[z].ey, 0, enemyDat[enemy[z].enemytype].explosiontype, true, false);
				}
				else if (playerInvulnerable_ == 0 && enemyAvail[z] == 0 &&
				         enemyDat[enemy[z].enemytype].explosiontype % 2 == 0)
				{
					
					tempI3 = enemy[z].armorleft;
					if (tempI3 > damageRate)
						tempI3 = damageRate;
					
					JE_playerDamage(tempI3, PX_, PY_, playerAlive_, playerStillExploding_, armorLevel_, shield_);
					
					if (enemy[z].armorleft > 0)
					{
						*lastTurn2_ += (enemy[z].exc * enemy[z].armorleft) / 2;
						*lastTurn_  += (enemy[z].eyc * enemy[z].armorleft) / 2;
					}
					
					tempI = enemy[z].armorleft;
					if (tempI == 255)
						tempI = 30000;
					
					temp = enemy[z].linknum;
					if (temp == 0)
						temp = 255;
					
					b = z;
					
					if (tempI > tempI2)
					{
						if (enemy[z].armorleft != 255)
							enemy[z].armorleft -= tempI3;
						soundQueue[5] = S_ENEMY_HIT;
					}
					else
					{
						for (temp2 = 0; temp2 < 100; temp2++)
						{
							if (enemyAvail[temp2] != 1)
							{
								temp3 = enemy[temp2].linknum;
								if (temp2 == b ||
									(temp != 255 &&
									 (temp == temp3 || temp - 100 == temp3
									  || (temp3 > 40 && temp3 / 20 == temp / 20 && temp3 <= temp))))
								{
									tempI3 = enemy[temp2].ex + enemy[temp2].mapoffset;
									
									enemy[temp2].linknum = 0;
									
									enemyAvail[temp2] = 1;
									
									if (enemyDat[enemy[temp2].enemytype].esize == 1)
									{
										JE_setupExplosionLarge(enemy[temp2].enemyground, enemy[temp2].explonum, tempI3, enemy[temp2].ey);
										soundQueue[6] = S_EXPLOSION_9;
									}
									else
									{
										JE_setupExplosion(tempI3, enemy[temp2].ey, 0, 1, false, false);
										soundQueue[5] = S_EXPLOSION_4;
									}
								}
							}
						}
						enemyAvail[z] = 1;
					}
				}
			}

		}
	}
}

// kate: tab-width 4; vim: set noet:
