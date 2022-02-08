#include <stdint.h>
#include "flashoffsets.h"
#include "VeritazzExtra.h"
#include "Engine.h"
#include "maps.h"
#include "tables.h"
#include "leveldata.h"

#ifdef AUDIO
#include <ATMlib2.h>
#include "song.h"
#endif

/*
 * debugging pins A5 (PF0), A4 (PF1)
 */
/*
 * need to add exitToBootloader when using the below macro
 */
ARDUBOY_NO_USB;

Arduboy2Ex arduboy;
Engine engine(arduboy);

/*---------------------------------------------------------------------------
 * frame timing
 *---------------------------------------------------------------------------*/

static uint8_t next_frame(void)
{
	if (arduboy.nextFrame()) {
		arduboy.pollButtons();
		return 1;
	}
	return 0;
}

static void finish_frame(uint8_t clear)
{
	Cart::disable();
	Cart::enableOLED();
	arduboy.display(clear);
	Cart::disableOLED();
}

/*---------------------------------------------------------------------------
 * setup
 *---------------------------------------------------------------------------*/
void setup(void)
{
	arduboy.setFrameRate(FPS);
	arduboy.begin();
#ifdef SERIAL_DEBUG
	Serial.begin(9600);
#endif
	Cart::disableOLED();
	Cart::begin(0xFF74);

#ifdef AUDIO
	/* enable Audio */
	arduboy.audio.on();
	/* initialize audio library */
	atm_synth_setup();
#endif

#ifdef GPIO_DEBUG
	/* set to low */
	PORTF &= ~(_BV(0) | _BV(1));
	DDRF |= (_BV(0) | _BV(1));
#endif
}

/* 
 *
 * + add display of mainscreen     (GAME_MENU)
 * + handle game menu (LOAD, PLAY) (GAME_MENU -> GAME_PLAY button A with transition or load from flash then GAME_PLAY)
 * + run engine                    (GAME_PLAY -> GAME_DONE:GAME_MENU)
 * + display death screen          (GAME_DONE -> GAME_MENU button A)
 *
 */

enum gameStates {
	GAME_MENU = 0,           /* show game start screen */
	GAME_PLAY,               /* run game */
	GAME_DIALOG,             /* dialog screen */
	GAME_QUEST,              /* quest screen */
	GAME_INVENTORY,          /* inventory screen */
	GAME_NEXT_LEVEL,         /* show some pic?, transit screen animation? */
	GAME_END_DEATH,          /* game ended deadly */
	GAME_END,                /* game ended successfully */
};

static void transitScreen(void)
{
	unsigned char *buffer = arduboy.getBuffer();

#define CONFIG_MENU_ANIMATION_2

#if defined(CONFIG_MENU_ANIMATION_1)
	uint8_t ypos[SCREEN_WIDTH];
	uint8_t done;

	memset(ypos, 0, sizeof(ypos));

	do {
		while (!next_frame())
			;

		done = 0;

		for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {
			uint8_t y = ypos[x];
			if (y == HEIGHT) {
				done++;
				continue;
			}
			uint8_t zero = rand() % 4 + 1;

			if (zero >= (HEIGHT - y))
				zero = HEIGHT - y;

			ypos[x] += zero;

			uint8_t shift = y % 8;
			const uint8_t *p = pixel_patterns + (((uint8_t)zero - 1) * 32 + 4 * shift);
			uint8_t b1 = pgm_read_uint8(p++);
			uint16_t offset = x * HEIGHT_BYTES + (y / 8);

			buffer[offset] |= b1;
			uint8_t b2 = pgm_read_uint8(p);
			buffer[offset + 1] |= b2;

		}

		finish_frame(false);
	} while (done != SCREEN_WIDTH);
#elif defined(CONFIG_MENU_ANIMATION_2)
/* must be a power of 2 */
#define __COLUMS         2
	uint8_t ypos[SCREEN_WIDTH / __COLUMS];
	uint8_t done;

	memset(ypos, 0, sizeof(ypos));

	/* lower framerate for a smoother animation */
	arduboy.setFrameRate(15);

	do {
		while (!next_frame())
			;

		done = 0;
		for (uint8_t x = 0; x < SCREEN_WIDTH / __COLUMS; x++) {
			uint8_t y = ypos[x];
			if (y == HEIGHT) {
				done++;
				continue;
			}
			/* maximum of 6 pixels shift */
			uint8_t shift = rand() % 6 + 1;

			if (shift >= (HEIGHT - y))
				shift = HEIGHT - y;

			ypos[x] += shift;

			for (uint8_t columns = 0; columns < __COLUMS; columns++) {
				uint8_t a, ashifted, b = 0;

				uint16_t offset = ((uint16_t)x * __COLUMS * HEIGHT_BYTES) + ((uint16_t)columns * HEIGHT_BYTES);
				for (uint8_t t = 0; t < 8; t++) {
					a = buffer[offset];
					ashifted = (b >> (8 - shift)) | (a << shift);
					buffer[offset] = ashifted;
					/* next page */
					offset++;
					/* swap b and a */
					uint8_t tmp = b;
					b = a;
					a = tmp;
				}
			}
		}

		finish_frame(false);
	} while (done != SCREEN_WIDTH / __COLUMS);
#undef __COLUMS
#endif

	/* reset back to real frame rate */
	arduboy.setFrameRate(FPS);
}

static enum gameStates gameStateMenu(void)
{
	uint8_t selection = 0;

	/* wait for user to start the game */
	do {
		while (!next_frame())
			;

		finish_frame(true);
		/* display game main screen */
		engine.drawBitmap(0, 8, mainscreen_flashoffset, 128, 24, WHITE);

		engine.drawString(24, 4, str_menu_flashoffset);
		engine.drawString(32, 7, str_credits_flashoffset);
		engine.drawBitmap(10 + selection * 60, 34, menu_arrow_flashoffset, 12, 6, WHITE);

		finish_frame(false);
		while (arduboy.notPressed(0xff))
			;

		if (arduboy.justPressed(LEFT_BUTTON))
			selection--;
		else if (arduboy.justPressed(RIGHT_BUTTON))
			selection++;

		selection &= 1;

	} while (arduboy.justPressed(A_BUTTON) == 0);

	transitScreen();

	engine.init();

	return GAME_PLAY;
}

static enum gameStates gameStatePlay(void)
{
	/* update world, sprites, etc. */
	engine.update();

	/* render the current frame */
	engine.render();

	uint8_t gameEvents = engine.getSystemEvents();
	if (gameEvents & EVENT_PLAYER_DEAD) {
		/* player died */
		arduboy.setFrameRate(FPS / 2);
		return GAME_END_DEATH;
	} else if (gameEvents & EVENT_PLAYER_NEXT_LEVEL) {
		/* player advances to next level */
		return GAME_NEXT_LEVEL;
	} else if (gameEvents & EVENT_DIALOG) {
		/* opened a dialog */
		return GAME_DIALOG;
	} else if (gameEvents & EVENT_QUEST) {
		/* start a quest */
		return GAME_QUEST;
	}

	/* just continue with next render loop */
	return GAME_PLAY;
}

static enum gameStates gameStateDialog(void)
{
	uint8_t dialogNr = engine.getSystemEventData(EVENT_DIALOG);

	engine.drawBitmap(0, 0, dialogs_flashoffset + (1024 * dialogNr), 128, 64, WHITE);

	/* display frame */
	finish_frame(false);

	while (arduboy.justPressed(A_BUTTON) == 0)
		arduboy.pollButtons();

	/* player read the dialog */

	transitScreen();
	return GAME_PLAY;
}

static enum gameStates gameStateQuest(void)
{
	/* read quest data and decide:
	 *  if quest is a new quest (not reached, catched by the engine)
	 *  if quest is active and finished
	 *     give reward, trigger other events
	 *  if quest is active and not finised
	 */

	if (engine.isActiveQuestFinished()) {
		// TODO reward screen
		/* apply quests reward */
		engine.rewardActiveQuest();
	} else {
		uint8_t questID = engine.getSystemEventData(EVENT_QUEST);

		/* display quest screen */
		engine.drawBitmap(0, 0, quests_flashoffset + (1024 * questID), 128, 64, WHITE);

		/* display frame */
		finish_frame(false);

		while (arduboy.justPressed(A_BUTTON) == 0 && arduboy.justPressed(B_BUTTON) == 0)
			arduboy.pollButtons();

		if (arduboy.pressed(A_BUTTON)) {
			/* quest accepted */
			engine.setActiveQuest(questID);
		} else {
			/* quest rejected */
			// TODO display frame to give player some feedback?
		}

		transitScreen();
	}
	return GAME_PLAY;
}

/* TODO */
static enum gameStates gameStateInventory(void)
{
	static uint8_t delay = 200;

	/* show some animation or transit screen! */
	engine.drawBitmap(0, 8, mainscreen_flashoffset, 128, 24, WHITE);

	if (delay--) {
		return GAME_INVENTORY;
	}
	delay = 200;
	return GAME_PLAY;
}

static enum gameStates gameStateNextLevel(void)
{
	/* show some animation or transit screen! */
	engine.drawBitmap(0, 8, mainscreen_flashoffset, 128, 24, WHITE);
	transitScreen();

	uint8_t level = engine.getSystemEventData(EVENT_PLAYER_NEXT_LEVEL);
	if (level == 0xff) {
		/*
		 * proceed just to the next level, if not possible
		 * then the player won
		 */
		if(!engine.nextLevel())
			return GAME_END;
	} else {
		/* proceed to the given level */
		engine.jumpToLevel(level);
	}

	/* initialize the engine for the new level */
	engine.init();

	return GAME_PLAY;
}

static enum gameStates gameStateEndDeath(void)
{
	engine.simulation = 1;

	if (arduboy.notPressed(A_BUTTON)) {
		/* update world, sprites, etc. */
		engine.update();

		/* render scene */
		engine.render();
		engine.drawString(56, 3, str_dead_flashoffset);
		return GAME_END_DEATH;
	}

	engine.simulation = 0;

	return GAME_MENU;
}

static enum gameStates gameStateEnd(void)
{
	engine.simulation = 1;
	/* TODO there is some bug when setting vMove */
//	engine.vMove = -8;

	if (arduboy.notPressed(A_BUTTON)) {
		/* update world, sprites, etc. */
		engine.update();

		/* render scene */
		engine.render();
		return GAME_END;
	}

	engine.simulation = 0;
//	engine.vMove = 0;

	return GAME_MENU;
}

/* state of the game */
static enum gameStates game_state = GAME_MENU;

/*---------------------------------------------------------------------------
 * loop
 *---------------------------------------------------------------------------*/
void loop(void)
{
	if (!next_frame())
		return;

	if (game_state == GAME_MENU)
		game_state = gameStateMenu();
	else if (game_state == GAME_PLAY)
		game_state = gameStatePlay();
	else if (game_state == GAME_DIALOG)
		game_state = gameStateDialog();
	else if (game_state == GAME_QUEST)
		game_state = gameStateQuest();
	else if (game_state == GAME_INVENTORY)
		game_state = gameStateInventory();
	else if (game_state == GAME_NEXT_LEVEL)
		game_state = gameStateNextLevel();
	else if (game_state == GAME_END_DEATH)
		game_state = gameStateEndDeath();
	else
		game_state = gameStateEnd();

	/*
	 * display current frame and reload framebuffer with "hud_special" image data
	 * which consists of 0xff except for the last line which is the HUD already
	 * drawn
	 */
	Cart::displayPrefetch(hud_special_flashoffset, Arduboy2::sBuffer, 1024, false);
}
