#include <math.h>

#include "Engine.h"
#include "tables.h"
#include "maps.h"
#include "flashoffsets.h"
#include "leveldata.h"

#include "drawing.h"
#include "map.h"

#ifdef AUDIO
#include <ATMlib2.h>
#include "song.h"

struct atm_sfx_state sfx_state;

static const uint8_t * const mapSounds[] PROGMEM = {
	(const uint8_t *)&sfx2, /* door opening */
	(const uint8_t *)&sfx3, /* door closing */
	(const uint8_t *)&sfx4, /* door locked */
	(const uint8_t *)&sfx5, /* door closed */
	(const uint8_t *)&sfx6, /* trigger */
};
#endif

#ifdef CONFIG_ASM_OPTIMIZATIONS
uint24_t levelFlashOffset;
#endif

/*
 * way to put pseudo variable at a fixed address to prevent memory corruption
 * by variables placed by bootloaders
 */
//uint8_t __ramend __attribute__((address(0x8007ff), used));

/*
 * pretend this variable is the GPIOR0 register (free to use) to gain some speed,
 * not really required but fun to use (saves minimal PROGMEM)
 */
static uint8_t ray __attribute__((io(0x3e)));

/*
 * complete engine state in one struct so on retry we can just
 * wipe it with a single memset
 */
struct engineState es;

//#define RAY32_DEBUG

#define STRINGIFY(x) #x
#define STRING(x) STRINGIFY(x)
#define LINE_LABEL(name) #name "_" STRING(__LINE__) ":\n"
#define ASM_LABEL(name) asm volatile(LINE_LABEL(name))

/*
 * precalculated shifts of a byte with 1 bit set, must be aligned to an 8 byte boundary
 * so some "adc" calls can be saved in assembly code
 */
extern "C" const uint8_t bitshift_left[] __attribute__ ((aligned (8))) = {
  1, 2, 4, 8, 16, 32, 64, 128
};

static uint8_t minWallDistance = PLAYERS_MIN_WALL_DISTANCE;
static uint16_t rayAngle;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)	(sizeof((a)) / sizeof((a)[0]))
#endif

#define GNOMESORT(a, s, v, _dt) \
{ \
	uint8_t pos = 0; \
	while (pos < (s)) { \
		if (pos == 0 || (v)((a)[pos], (a)[pos - 1])) { \
			pos++; \
		} else { \
			_dt tmp = (a)[pos]; \
			(a)[pos] = (a)[pos - 1]; \
			(a)[pos - 1] = tmp; \
			pos--; \
		} \
	} \
}

Engine::Engine(Arduboy2Ex &a)
{
	arduboy = &a;
}

#define STATUS_KEY                    0
#define STATUS_WEAPON1                1
#define STATUS_WEAPON2                2
#define STATUS_WEAPON3                3
#define STATUS_AMMO                   4
#define STATUS_HEALTH                 5
#define STATUS_DOOR_REQUIRES_KEY      6
#define STATUS_DOOR_REQUIRES_TRIGGER  7
#define STATUS_QUEST_PENDING          8

/*
 * weapon damage
 */
static const uint8_t weaponDamage[NR_OF_WEAPONS] PROGMEM = {
	 1, /* weapon 1 */
	 3, /* weapon 2 */
	 5, /* weapon 3 */
	15, /* weapon 4 */
};

uint8_t Engine::simulateButtons(uint8_t buttons)
{
	if (es.killedBySprite) {
		// TODO let the player look at the sprite that killed it
	}

	return 0;
}

uint8_t Engine::pressed(uint8_t buttons)
{
	if (simulation)
		return simulateButtons(buttons);

	return arduboy->pressed(buttons);
}

void Engine::init(void)
{
	/* reset engine */
	memset(&es, 0, sizeof(es));

	/* set player's initial position, weapons, health and the like */
	es.playerHealth = PLAYERS_INITIAL_HEALTH;
	es.playerWeapons = HAS_WEAPON_1;   // weapon 1 is always there, ammo unlimited
	es.playerAmmo[0] = 99;
	es.currentDamageCategory = 1;      // by default only the player will take damage, 0 = god mode

	es.vMoveDirection = -8;            // head movement direction ( - = up, + = down), signed Q3.4
	es.itemDanceDir = -1;              // start by moving the items down

	/*
	 * load all init data in one shot
	 */
	uint24_t levelOffset = levelData_flashoffset + (levelDataAlignment * currentLevel);

	FX::readDataBytes(levelOffset + (MAP_WIDTH * MAP_HEIGHT), (uint8_t *)&es.ld, (size_t)sizeof(struct level_initdata));

	es.ld.playerAngle = 100;

#ifndef CONFIG_ASM_OPTIMIZATIONS
	levelFlashOffset = levelOffset;
#else
	levelFlashOffset = ((uint24_t)FX::programDataPage << 8) + levelOffset;
#endif


	/* by default no quest is active */
	activeQuestId = QUEST_NOT_ACTIVE;

	es.spriteRespawnTimeout = SPRITE_RESPAWN_TIMEOUT;

	/* initial setup */
	update();
}

/*
 * by inlining the below function a few more cycles can be saved
 * at the expense of PROGMEM
 *
 * replaces: dividend / 64
 */
uint16_t Engine::divU24ByBlocksize(uint32_t dividend)
{
#ifdef ARDUINO_ARCH_AVR
	uint16_t result;

	/*
	* 2 MSBs need to be zero for this to work. The unsigned 24 bit
	* value will be divided by 64. Usually a right shift by 6 but
	* on the Atmega32u these shifts are very CPU intense as there
	* is no barrel shifter.
	*
	* Assume each letter is a byte of the 24 bit value, e.g.
	* A = bits[ 7: 0]
	* B = bits[15: 8]
	* C = bits[23:16]
	*
	* The algorithm rotates each byte 2 bits to the left through
	* the carry bit. The result of the division is in CB and needs
	* to be moved to BA.
	*
	* CBA
	* rol A
	* rol B
	* rol C
	* repeat above rotates
	* result in CB
	*
	* This is what it looks in assembly (with cycle counts)
	*
	* This is the function:
	* 1    3b58:	ab 01       	movw	r20, r22
	* 1    3b5a:	bc 01       	movw	r22, r24
	* 1    3b5c:	44 1f       	adc	r20, r20
	* 1    3b5e:	55 1f       	adc	r21, r21
	* 1    3b60:	66 1f       	adc	r22, r22
	* 1    3b62:	44 1f       	adc	r20, r20
	* 1    3b64:	55 1f       	adc	r21, r21
	* 1    3b66:	66 1f       	adc	r22, r22
	* 1    3b68:	85 2f       	mov	r24, r21
	* 1    3b6a:	96 2f       	mov	r25, r22
	* 4    3b6c:	08 95       	ret
	*
	* This is the call to the function:
	* 1    603a:	bc 01       	movw	r22, r24
	* 1    603c:	80 e0       	ldi	r24, 0x00	; 0
	* 1    603e:	90 e0       	ldi	r25, 0x00	; 0
	* 4    6040:	0e 94 ac 1d 	call	0x3b58	; 0x3b58 <_ZN6Engine17divU24ByBlocksizeEm.constprop.44>
	*
	* Overall 21 cycles (32 bytes)
	*
	* This is the code generated when just shifting a 24bit (32bit) value:
	*
	* 1    6020:	1b 01       	movw	r2, r22
	* 1    6022:	2c 01       	movw	r4, r24
	* 1    6024:	86 e0       	ldi	r24, 0x06	; 6
	* 1    6026:	56 94       	lsr	r5
	* 1    6028:	47 94       	ror	r4
	* 1    602a:	37 94       	ror	r3
	* 1    602c:	27 94       	ror	r2
	* 1    602e:	8a 95       	dec	r24
	* 2    6030:	d1 f7       	brne	.-12     	; 0x6026 <loopto+0x1792>
	*
	* Overall (3 + 5 * 7 + 6) = 44 cycles (18 bytes)
	*
	* Saved 23cycles for each division by 64
	*/
	asm volatile (" rol %A[dividend]\n"
		      " rol %B[dividend]\n"
		      " rol %C[dividend]\n"
		      " rol %A[dividend]\n"
		      " rol %B[dividend]\n"
		      " rol %C[dividend]\n"
		      " mov %A[result], %B[dividend]\n"
		      " mov %B[result], %C[dividend]\n"
			: [result] "=r" (result), /* output */
			  [dividend] "+r" (dividend)
			:
			: /* clobber */);
	return result;
#else
	return dividend / BLOCK_SIZE;
#endif
}

// cacheRow ror, zero ror, [cacherow, zero]
uint16_t Engine::fastMul128(uint8_t value)
{
#ifdef ARDUINO_ARCH_AVR
	uint16_t result;
	asm volatile (" lsr %A[value]\n"
		      " mov %A[result], __zero_reg__\n"
		      " ror %A[result]\n"
		      " mov %B[result], %A[value]\n"
			: [result] "=r" (result), /* output */
			  [value] "+r" (value)
			:
			: /* clobber */);
	return result;
#else
	return ((uint16_t)value * 128);
#endif
}


uint8_t Engine::divU16ByBlocksize(uint16_t dividend)
{
#ifdef ARDUINO_ARCH_AVR
	uint8_t result;
	asm volatile (" rol %A[dividend]\n"
		      " rol %B[dividend]\n"
		      " rol %A[dividend]\n"
		      " rol %B[dividend]\n"
		      " mov %A[result], %B[dividend]\n"
			: [result] "=r" (result), /* output */
			  [dividend] "+r" (dividend)
			:
			: /* clobber */);
	return result;
#else
	return dividend / BLOCK_SIZE;
#endif
}

int16_t Engine::divS24ByBlocksize(int32_t dividend)
{
#ifdef ARDUINO_ARCH_AVR
	int16_t result;
	asm volatile (" rol %A[dividend]\n"
		      " rol %B[dividend]\n"
		      " rol %C[dividend]\n"
		      " rol %A[dividend]\n"
		      " rol %B[dividend]\n"
		      " rol %C[dividend]\n"
		      " mov %A[result], %B[dividend]\n"
		      " mov %B[result], %C[dividend]\n"
			: [result] "=r" (result), /* output */
			  [dividend] "+r" (dividend)
			:
			: /* clobber */);
	return result;
#else
	return dividend / BLOCK_SIZE;
#endif
}

/*
 * max dividend value is 127,
 * not used but wanted to keep it because it looks nice
 */
uint8_t Engine::divU8By8(uint8_t dividend)
{
#ifdef ARDUINO_ARCH_AVR
	asm volatile (" lsl %[dividend]\n"
		      " andi %[dividend], 0xf\n"
		      " swap %[dividend]\n"
			: [dividend] "+r" (dividend) /* output */
			:
			: /* clobber */);
	return dividend;
#else
	return (dividend / 8);
#endif
}

/*
 * can be used to create flip images
 */
uint8_t Engine::bitreverseU8(uint8_t byte)
{
	uint8_t result = byte;

	result = ((result >> 1) & 0x55) | ((result << 1) & 0xaa);
	result = ((result >> 2) & 0x33) | ((result << 2) & 0xcc);
#ifdef ARDUINO_ARCH_AVR
	asm volatile (" swap %[result]\n"
		      : [result] "+r" (result) /* output */
		      : /* input */
		      : /* clobber */);
#else
	result = ((result >> 4) & 0x0f) | ((result << 4) & 0xf0);
#endif
	return result;
}

/* TODO this is clumsy just because "door" is needed for activateDoor,
 * maybe just store the doorId in the struct
 */
void Engine::findAndActivateDoor(void)
{
	struct door *d = &es.ld.doors[0];
	for (uint8_t door = 0; door < MAX_DOORS; door++) {
		if (d == es.cd) {
			activateDoor(d, door);
			/* done */
			break;
		}
		d++;
	}
}

void Engine::activateDoor(struct door *d, uint8_t door)
{
	/* check if the player needs a key to open this door */
	if (d->flags & DOOR_FLAG_LOCKED && es.playerKeys == 0) {
		/* tell the player to search for a key */
		setStatusMessage(STATUS_DOOR_REQUIRES_KEY);

		/* play door locked effect */
		startAudioEffect(AUDIO_EFFECT_ID_MAP, 2);

	} else if (d->flags & DOOR_FLAG_TRIGGER) {
		/* tell the player to search for a trigger */
		setStatusMessage(STATUS_DOOR_REQUIRES_TRIGGER);

		/* play door locked effect */
		startAudioEffect(AUDIO_EFFECT_ID_MAP, 2);

	} else if (d->state == DOOR_CLOSED || d->state == DOOR_CLOSING) {
		/* if door is closed, open it */
		if (d->flags & DOOR_FLAG_LOCKED) {
			es.playerKeys--; /* player now has one key less */
			d->flags &= ~DOOR_FLAG_LOCKED; /* clear lock */
		}

		if (d->state == DOOR_CLOSED) {
			es.activeDoors[es.nrOfActiveDoors++] = door;
		}

		d->state = DOOR_OPENING;
		/*
		 * this variable will be used to detect if the
		 * doors have reached their maximum opening
		 * position
		 */
		d->openTimeout = BLOCK_SIZE - 1;

		/* play door open effect */
		startAudioEffect(AUDIO_EFFECT_ID_MAP, 0);

	} else if (d->state == DOOR_OPEN || d->state == DOOR_OPENING) {
		/* if door is open, close it */
		if (d->state == DOOR_OPEN)
			d->openTimeout = 0;
		d->state = DOOR_CLOSING;

		/* play door closing effect */
		startAudioEffect(AUDIO_EFFECT_ID_MAP, 1);
	}
}

void Engine::runTriggerAction(uint8_t old_state, struct trigger *t)
{
	if (old_state == (t->flags & TRIGGER_FLAG_STATE))
		return;

	/* play trigger effect */
	startAudioEffect(AUDIO_EFFECT_ID_MAP, 4);

	uint8_t object = t->flags & TRIGGER_FLAG_OBJ;
	if (object == TRIGGER_OBJ_DOOR) {
		/* door */
		struct door *d = &es.ld.doors[t->obj_id];
		/* clear trigger flag */
		d->flags &= ~DOOR_FLAG_TRIGGER;
		activateDoor(d, t->obj_id);
		/*
		 * if the trigger is of type switch, reenable the trigger flag
		 * of the door, so the player needs to be fast enough to pass
		 * the door while it is open. Otherwise it closes again and the
		 * player needs to trigger the switch again
		 */
		if ((t->flags & TRIGGER_FLAG_TYPE) == TRIGGER_TYPE_SWITCH) {
			/* set trigger flag */
			d->flags |= DOOR_FLAG_TRIGGER;
		}
	} else if (object == TRIGGER_OBJ_VMW) {
		/* moving wall */
		struct movingWall *mw = &es.ld.movingWalls[t->obj_id];
		/* toggle active flag */
		mw->flags ^= VMW_FLAG_ACTIVE;
	} else if (object == TRIGGER_OBJ_DIALOG) {
		/* set dialog system event */
		setSystemEvent(EVENT_DIALOG, t->obj_id);
	} else if (object == TRIGGER_OBJ_QUEST) {
		/*
		 * check if player does already has a quest or if
		 * the new quest is the current quest
		 */
		if (activeQuestId == QUEST_NOT_ACTIVE || activeQuestId == t->obj_id) {
			/* set quest system event for the new quest */
			setSystemEvent(EVENT_QUEST, t->obj_id);
		} else {
			/*
			 * print message that the player is busy with
			 * a quest
			 */
			setStatusMessage(STATUS_QUEST_PENDING);
		}
	} else if (object == TRIGGER_OBJ_NEXT_LEVEL) {
		/*
		 * set next level event
		 *   obj_id  = 0xff: next level from current one
		 *   obj_id != 0xff: level number given in obj_id
		 */
		setSystemEvent(EVENT_PLAYER_NEXT_LEVEL, t->obj_id);
	}
}

void Engine::activateTrigger(uint8_t mapX, uint8_t mapY)
{
	struct trigger *t = &es.ld.triggers[0];

	for (uint8_t trigger = 0; trigger < MAX_TRIGGERS; trigger++) {
		if (t->timeout == 0 && t->mapX == mapX && t->mapY == mapY) {
			uint8_t old_state;

			if ((t->flags & TRIGGER_FLAG_TYPE) == TRIGGER_TYPE_TOUCH) {
				/* old state is just the opposite of the current one */
				old_state = (t->flags & TRIGGER_FLAG_STATE) ^ TRIGGER_STATE_ON;
			} else {
				/* reset timeout */
				t->timeout = TRIGGER_TIMEOUT;

				old_state = t->flags & TRIGGER_FLAG_STATE;
			}

			/* if of type switch, toggle state */
			if ((t->flags & TRIGGER_FLAG_TYPE) == TRIGGER_TYPE_SWITCH) {
				/* it is a switch, so toggle state */
				t->flags ^= TRIGGER_STATE_ON;
			} else if ((t->flags & TRIGGER_FLAG_TYPE) == TRIGGER_TYPE_ONE_SHOT) {
				/* just switch it on */
				t->flags |= TRIGGER_STATE_ON;
			}

			/* now activate/deactivate object */
			runTriggerAction(old_state, t);

			break;
		}
		t++;
	}
}

void Engine::setStatusMessage(uint8_t msg_id)
{
	es.statusMessage.text = messageStrings_flashoffset + (msg_id * messageStringsAlignment);
	es.statusMessage.timeout = 120; /* in frames */
}

/*
 * Own implementation of drawBitmap to save some PROGMEM by reusing existing
 * functions from the main rendering loop. Probably slower than the original
 * but this function is only used rarely so speed does not matter.
 */
void Engine::drawBitmap(uint8_t x, uint8_t y, const uint24_t bitmap, uint8_t w, uint8_t h, uint8_t color)
{
	FX::seekData(bitmap);

	unsigned char *buffer = arduboy->getBuffer() + (x * HEIGHT_BYTES);

	/* check if copying starts at a multiple of 8 boundary */
	if ((y % 8) == 0) {
		/* for every column of the image */
		for (uint8_t cx = 0; cx < w; cx++) {

			uint8_t toDraw = h;
			unsigned char *b = buffer + (y / 8);
			buffer += HEIGHT_BYTES;
			for (uint8_t ph = 0; ph < ((h + 7) / 8); ph++) {
				uint8_t pixels = SPDR;
				SPDR = 0;

				uint8_t m;
				if (toDraw < 8) {
					m = 0xff >> toDraw;
				} else {
					m = 0xff;
					toDraw -= 8;
				}

				*b &= ~m;
				*b |= pixels;
				b++;
			}
		}
	} else {
		// TODO try same algorithm as for screen transition 2
		/* for every column of the image */
		for (uint8_t cx = 0; cx < w; cx++) {

			uint8_t pixels;
			for (uint8_t ph = 0; ph < h; ph++) {
				if ((ph % 8) == 0) {
					pixels = SPDR;
					SPDR = 0;
				}
				unsigned char *b = buffer + ((y + ph) / 8);
				const uint8_t shift = (y + ph) % 8;
				const uint8_t texShift = bitshift_left[ph % 8];
				const uint8_t color = pixels & texShift;

				const uint8_t m = bitshift_left[shift];
				if (color)
					*b |= m;
				else
					*b &= ~m;
			}
			buffer += HEIGHT_BYTES;
		}
	}
	FX::readEnd();
}

uint8_t Engine::drawString(uint8_t x, uint8_t page, uint24_t message)
{
	char c;
	uint8_t v;
	/* calculate buffer address */
	unsigned char *buffer = arduboy->getBuffer() + page + (x * HEIGHT_BYTES);

	/*
	 * print status string when the player collects items or triggers
	 * some events.
	 */
	for (;;) {
		/* TODO combination of both for faster single byte reads */
		/* speed it up by storing the string as image */
		FX::seekData(message++);
		c = FX::readEnd();
		if (!c)
			break;

		if (c == ' ')
			c = 36;
		else if (c < 'a')
			c -= '0';
		else
			c -= 'W';

		FX::seekData(characters_3x4_flashoffset + (c * 3));

		v = FX::readPendingUInt8();
		buffer[0] = v >> 1;
		v = FX::readPendingUInt8();
		buffer[HEIGHT_BYTES * 1] = v >> 1;
		v = FX::readEnd();
		buffer[HEIGHT_BYTES * 2] = v >> 1;
		buffer[HEIGHT_BYTES * 3] = 0;

		buffer += HEIGHT_BYTES * 4;

		x += 4;
	}

	return x;
}

void Engine::drawNumber(uint8_t x, uint8_t y, uint8_t number)
{
	uint8_t digit;
	uint8_t v;
	uint8_t divider = 100;
	/* calculate buffer address */
	unsigned char *buffer = arduboy->getBuffer() + (y / 8) + (x * HEIGHT_BYTES);

	while (divider) {
		digit = number / divider;
		FX::seekData(characters_3x4_flashoffset + (digit * 3));
		v = FX::readPendingUInt8();

		buffer[0] |= v;
		v = FX::readPendingUInt8();
		buffer[8] |= v;
		v = FX::readEnd();
		buffer[16] |= v;

		buffer += 32;

		number %= divider;
		divider /= 10;
	}
}

void Engine::enemyTurnToPlayer(struct current_sprite *cs)
{
	cs->viewAngle = cs->spriteAngle - 180;
	if (cs->viewAngle < 0)
		cs->viewAngle += 360;
}

uint8_t Engine::enemyStateIdle(struct current_sprite *cs, uint8_t speed)
{
	uint8_t state = ENEMY_IDLE;

	if (cs->distance < 256) {
		state = ENEMY_FOLLOW;
		enemyTurnToPlayer(cs);
	}

	return state;
}

uint8_t Engine::enemyStateAttack(struct current_sprite *cs, uint8_t speed)
{
	uint8_t state = ENEMY_ATTACK;

	if (cs->distance > 128) {
		state = ENEMY_FOLLOW;
		enemyTurnToPlayer(cs);
	} else {
		/*
		 * if cooldown has expired and attack threshold is reached
		 * the sprite can do damage
		 */
		if (attackCoolDown == 0) {
			if (attackLevel > ENEMY_ATTACK_THRESHOLD) {
				/* Do dummy move with range attack step (range - minWallDistance)
				 * to check if enemy has free line of sight, if not, do random
				 * moves.
				 * This is not an exact but cheap way.
				 */

				/* move, if no path -> random moves */
				uint16_t x = cs->x, xo = cs->x;
				uint16_t y = cs->y, yo = cs->y;

				/* set movement distance to attack distance - minWallDistance */
				uint8_t distance = 120;
				/* disable damage at all, god mode */
				es.currentDamageCategory = 0;
				/* reduce minWallDistance to get more agility for moves */
				minWallDistance = 8;
				speed = 8;

				for (;;) {

					if (distance < 8)
						speed = distance;

					/* try to move */
					move(cs->viewAngle, 1, &x, &y, speed);

					/*
					 * if x and y are unchanged we could not move directly
					 * and thus are not allowed to attack
					 */
					if (x == xo && y == yo)
						break;

					distance -= speed;

					if (distance == 0)
						break;

					xo = x;
					yo = y;
				}

				/* set minWallDistance back to normal */
				minWallDistance = PLAYERS_MIN_WALL_DISTANCE;
				/* only the player will take damage */
				es.currentDamageCategory = 1;

				/* if distance is not zero we could not directly move */
				if (distance != 0) {
					/* just do random moves */
					state = ENEMY_RANDOM_MOVE;
				} else {
					/* inflict damage on the player */
#ifndef CONFIG_GOD_MODE
					es.playerHealth--;
					es.blinkScreen = 1;
#endif

					/* remember who killed the player */
					if (es.playerHealth == 0)
						es.killedBySprite = cs->id + 1;
				}
			} else {
				/* if attack level not yet reached or got lower, randomly move */
				state = ENEMY_RANDOM_MOVE;
			}
		}
	}
	return state;
}

uint8_t Engine::enemyStateFollow(struct current_sprite *cs, uint8_t speed)
{
	uint8_t state = ENEMY_FOLLOW;

	/* TODO should use a per enemy type attack distance */
	if (cs->distance < 96) {
		state = ENEMY_ATTACK;
	} else if (cs->distance > 270) {
		state = ENEMY_RANDOM_MOVE;
	} else {
		enemyTurnToPlayer(cs);

		uint8_t twist = rand() % 16;
		if (twist & 1)
			cs->viewAngle += twist;
		else
			cs->viewAngle -= twist;
		moveSprite(cs, speed);

		/* only the player will take damage */
		es.currentDamageCategory = 1;
	}
	return state;
}

uint8_t Engine::enemyStateRandomMove(struct current_sprite *cs, uint8_t speed)
{
	uint8_t state = ENEMY_RANDOM_MOVE;

	if (cs->distance < 256) {
		state = ENEMY_FOLLOW;
	} else {
		moveSprite(cs, speed);

		/* ca. every 4 frames turn random degrees */
		if ((es.frame % FPS) == 0) {
			cs->viewAngle += rand();
			if (cs->viewAngle >= 360)
				cs->viewAngle -= 360;
		}
	}
	return state;
}

void Engine::doDamageToSprite(uint8_t id, uint8_t damage)
{
	uint8_t health = es.ld.dynamic_sprite_flags[id];
	struct sprite *s = &es.ld.dynamic_sprites[id];

	/* inflict enemy damage */
	if (health <= damage) {
		/* TODO sprite death animation */
		s->flags |= S_INACTIVE;
	} else {
		health -= damage;
	}
	es.ld.dynamic_sprite_flags[id] = health;
}

void Engine::checkAndDoDamageToSpriteByObjects(uint8_t id)
{
	if (es.doDamageFlags & es.currentDamageCategory) {
		es.doDamageFlags &= ~2;
		doDamageToSprite(id, 10);
	}
}

uint8_t Engine::moveSprite(struct current_sprite *cs, uint8_t speed)
{
	uint8_t ret;

	/* only enemies will take damage */
	es.currentDamageCategory = 2;

	/* move, if no path -> random moves */
	ret = move(cs->viewAngle, 1, &cs->x, &cs->y, speed);

	/* inflict damage to the sprite if necessary */
	checkAndDoDamageToSpriteByObjects(cs->id);

	/* only the player will take damage */
	es.currentDamageCategory = 1;

	return ret;
}

/* translates an index to a block side
 *
 *       1
 *     +---+
 *   0 |   | 2
 *     +---+
 *       3
 */
static const uint8_t xlateBlockHitToSide[] = {1, 0, 1, 2, 3, 2, 3, 0};

/*
 * index into texture_ptrs for each side of the block
 *
 *       1
 *     +---+
 *   0 |   | 2
 *     +---+
 *       3
 *
 * position 0: left
 * position 1: up
 * position 2: right
 * position 3: down
 */
static uint8_t blockTextures[WALL_BLOCKS * 4] = {
	 11,  11,  11,  11, // W0
	  1,   1,   1,   1, // W1
	  7,   6,   6,   0, // W2
	  0,   0,   0,   0, // W3
	  0,   0,   0,   0, // W4
	  0,   0,   0,   0, // W5
	  0,   0,   0,   0, // W6
	  1,   1,   1,   1, // W7
	  9,   0,   9,   0, // W8
	  9,   0,   0,   0, // W9
	  0,   0,   9,   0, // W10
	  0,   9,   0,   9, // W11
	  0,   9,   0,   0, // W12
	  0,   0,   0,   9, // W13
	  7,   7,   7,   7, // W14
	  0,   1,   2,   3, // W15
	  0,   1,   2,   3, // W16
	  0,   1,   2,   3, // W17
	  0,   1,   2,   3, // W18
	  0,   1,   2,   3, // W19
	  0,   1,   2,   3, // W20
	  0,   1,   2,   3, // W21
	  0,   1,   2,   3, // W22
	  0,   1,   2,   3, // W23
	  0,   1,   2,   3, // W24
	  0,   1,   2,   3, // W25
	  0,   1,   2,   3, // W26
	  0,   1,   2,   3, // W27
	  0,   1,   2,   3, // W28
	  0,   1,   2,   3, // W29
	  0,   1,   2,   3, // W30
};

/*
 * effect ids applied to textures before beeing drawn on the screen
 *
 * bit[7:4] effect id on the texture index
 * bit[3:0] effect id on the texture data
 *
 */
static uint8_t textureEffects[WALL_BLOCKS] = {
	0,                     /* W0 */
	0 | 1,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

/*
 * special functions for block texture index effects, runs during update
 */

/*
 * rotate positions to the left (counter clockwise)
 * e.g. 0->1, 1->2, 2->3, 3->0
 *
 *       1
 *     +---+
 *   0 |   | 2
 *     +---+
 *       3
 */
void Engine::texturesRotateLeft(uint8_t offset)
{
	uint8_t tmp = blockTextures[offset + 0];

	blockTextures[offset + 0] = blockTextures[offset + 1];
	blockTextures[offset + 1] = blockTextures[offset + 2];
	blockTextures[offset + 2] = blockTextures[offset + 3];
	blockTextures[offset + 3] = tmp;
}

/*
 * rotate positions to the right (clockwise)
 * e.g. 0->3, 1->0, 2->1, 3->2
 *
 *       1
 *     +---+
 *   0 |   | 2
 *     +---+
 *       3
 */
void Engine::texturesRotateRight(uint8_t offset)
{
	uint8_t tmp = blockTextures[offset + 0];

	blockTextures[offset + 0] = blockTextures[offset + 3];
	blockTextures[offset + 3] = blockTextures[offset + 2];
	blockTextures[offset + 2] = blockTextures[offset + 1];
	blockTextures[offset + 1] = tmp;
}

/*
 * exchange positions 1 and 3
 * e.g. 1->3, 3->1
 *
 *       1
 *     +---+
 *   0 |   | 2
 *     +---+
 *       3
 */
void Engine::texturesExchangeUpDown(uint8_t offset)
{
	uint8_t tmp = blockTextures[offset + 3];

	blockTextures[offset + 3] = blockTextures[offset + 1];
	blockTextures[offset + 1] = tmp;
}

/*
 * exchange positions 0 and 2
 * e.g. 0->2, 2->0
 *
 *       1
 *     +---+
 *   0 |   | 2
 *     +---+
 *       3
 */
void Engine::texturesExchangeLeftRight(uint8_t offset)
{
	uint8_t tmp = blockTextures[offset + 0];

	blockTextures[offset + 0] = blockTextures[offset + 2];
	blockTextures[offset + 2] = tmp;
}

/*
 * special functions for texture data manipulations
 */
void Engine::textureEffectNone(const uint24_t p, uint8_t texX)
{
	FX::seekData(p + texX);
	es.texColumn[4] = FX::readPendingUInt8();
	es.texColumn[5] = FX::readPendingUInt8();
	es.texColumn[6] = FX::readPendingUInt8();
	es.texColumn[7] = FX::readEnd();
}

void Engine::textureEffectVFlip(const uint24_t p, uint8_t texX)
{
}

void Engine::textureEffectHFlip(const uint24_t p, uint8_t texX)
{
}

void Engine::textureEffectInvert(const uint24_t p, uint8_t texX)
{
	FX::seekData(p + texX);
	es.texColumn[4] = ~FX::readPendingUInt8();
	es.texColumn[5] = ~FX::readPendingUInt8();
	es.texColumn[6] = ~FX::readPendingUInt8();
	es.texColumn[7] = ~FX::readEnd();
}

void Engine::textureEffectRotateLeft(const uint24_t p, uint8_t texX)
{
	texX = (texX + es.textureRotateLeftOffset) % TEXTURE_SIZE;

	textureEffectNone(p, texX);
}


/*
 * movement speed of enemies in pixel
 */
static const uint8_t enemyMovementSpeeds [] PROGMEM = {
	ENEMY0_MOVEMENT_SPEED,
	ENEMY1_MOVEMENT_SPEED,
	ENEMY2_MOVEMENT_SPEED,
	ENEMY3_MOVEMENT_SPEED,
};

/*
 * movement speed of projectiles in pixel
 */
static const uint8_t projectileMovementSpeeds [] PROGMEM = {
	PROJECTILE00_MOVEMENT_SPEED,
	PROJECTILE01_MOVEMENT_SPEED,
	PROJECTILE02_MOVEMENT_SPEED,
	PROJECTILE03_MOVEMENT_SPEED,
	PROJECTILE04_MOVEMENT_SPEED,
	PROJECTILE05_MOVEMENT_SPEED,
	PROJECTILE06_MOVEMENT_SPEED,
	PROJECTILE07_MOVEMENT_SPEED,
	PROJECTILE08_MOVEMENT_SPEED,
	PROJECTILE09_MOVEMENT_SPEED,
	PROJECTILE10_MOVEMENT_SPEED,
	PROJECTILE11_MOVEMENT_SPEED,
	PROJECTILE12_MOVEMENT_SPEED,
	PROJECTILE13_MOVEMENT_SPEED,
	PROJECTILE14_MOVEMENT_SPEED,
	PROJECTILE15_MOVEMENT_SPEED,
};

void Engine::updateSpecialWalls(void)
{
	/* update shooting walls */
	if (shootingWallCoolDown == 0)
		shootingWallCoolDown = SHOOTING_WALL_COOLDOWN;
	else
		shootingWallCoolDown--;

	if (shootingWallCoolDown == 0) {
		FX::seekData(level1_specialWalls_flashoffset + (currentLevel * specialWallsDataAlignment));
		for (uint8_t p = es.ld.nr_of_sprites; p < (es.ld.nr_of_sprites + es.ld.maxSpecialWalls); p++) {
			struct sprite *s = &es.ld.dynamic_sprites[p];
			if (!IS_INACTIVE(s->flags)) {
				/* TODO what a waste, maybe seek? */
				FX::readPendingUInt8();
				FX::readPendingUInt8();
				FX::readPendingUInt8();
				continue;
			}

			/* set x/y position of the projectile based on view angle */
			uint24_t x = FX::readPendingUInt8() * 64;
			uint24_t y = FX::readPendingUInt8() * 64;
			switch (SPRITE_VIEWANGLE_GET(s)) {
			case 0:
				x += 64 + 16;
				y += 32;
				break;
			case 1:
				x += 32;
				y += 64 + 16;
				break;
			case 2:
				x -= 16;
				y += 32;
				break;
			case 3:
				x += 32;
				y -= 16;
				break;
			}
			SPRITE_XY_SET(s, x, y);
			s->flags = FX::readPendingUInt8();
			break;
		}
		FX::readEnd();
	}
}

void Engine::updateTextureEffects(void)
{
	/*
	 * run texture index effects
	 */
#if 0
	/*
	 * here its possible to change the effects dynamically, not sure if this is
	 * useful for anybody
	 */
	if ((es.frame % FPS) == 0) {
		textureEffects[0]++;
		textureEffects[0] &= 0xf1;
	}
#endif

	/* every X run the effects */
	if ((es.frame % (FPS / 8)) == 0) {

		/* update offsets for texture rotations */
		es.textureRotateLeftOffset = ((es.textureRotateLeftOffset + TEXTURE_HEIGHT_BYTES) % TEXTURE_SIZE);
		/*
		 * run texture index effects for all blocks
		 */
		for (uint8_t blockSideEffects = 0; blockSideEffects < WALL_BLOCKS; blockSideEffects++) {
			uint8_t effect = textureEffects[blockSideEffects] >> 4;
			uint8_t offset = blockSideEffects * 4;

			if (effect == 1)
				texturesRotateLeft(offset);
			else if (effect == 2)
				texturesRotateRight(offset);
			else if (effect == 3)
				texturesExchangeUpDown(offset);
			else if (effect == 4)
				texturesExchangeLeftRight(offset);
		}
	}
}

void Engine::updateTriggers(void)
{
	/*
	 * update triggers
	 */
	struct trigger *t = &es.ld.triggers[0];

	for (uint8_t trigger = 0; trigger < MAX_TRIGGERS; trigger++) {

		/* decrement timeout */
		if (t->timeout) {
			t->timeout--;
			/* if it is a switch, toggle it back to original position */
			if (t->timeout == 0 && ((t->flags & TRIGGER_FLAG_TYPE) == TRIGGER_TYPE_SWITCH))
				t->flags ^= TRIGGER_STATE_ON;
		}
		t++;
	}
}

void Engine::updateDoors(void)
{
	/*
	 * update doors
	 */
	uint8_t newNrOfActiveDoors = es.nrOfActiveDoors;
	struct door *d = &es.ld.doors[es.activeDoors[0]];

	for (uint8_t door = 0; door < es.nrOfActiveDoors; door++) {
		if (d->state == DOOR_OPENING) {
			d->offset += 1;
			if (d->offset == BLOCK_SIZE - 1) {
				/* door was fully opened */
				d->state = DOOR_OPEN;
				d->openTimeout = DOOR_OPEN_TIMEOUT;
			} else if (d->offset == d->openTimeout) {
				/* flaky, not completely opened, close right away */
				d->state = DOOR_CLOSING;
			}
		} else if (d->state == DOOR_OPEN) {
			if (d->openTimeout == 0) {
				/* TODO this check is not accurate, e.g. if the player stands
				 * close to the block border
				 */
				if (es.ld.playerMapX != d->mapX || es.ld.playerMapY != d->mapY) {
					d->state = DOOR_CLOSING;
					/* play door close effect */
					startAudioEffect(AUDIO_EFFECT_ID_MAP, 3);
				}
			} else {
				d->openTimeout -= 1;
			}
		} else if (d->state == DOOR_CLOSING) {
			d->offset -= 1;
			if (d->offset == 0) {
				d->state = DOOR_CLOSED;
				es.activeDoors[door] = 255;
				newNrOfActiveDoors--;
			}
		}
		d++;
	}

	/*
	 * sort in ascending order so all inactive doors in the list (marked with
	 * 255) will be moved to the end
	 */
	GNOMESORT(es.activeDoors, es.nrOfActiveDoors, compareGreaterOrEqual, uint8_t);

	/*
	 * Activate flaky doors (at least a few)
	 *
	 * if door is closed and a flaky one, then start opening it
	 * if random number allows it
	 */
	uint8_t maxNrOfActiveFlakyDoors = MAX_NR_OF_FLAKY_DOORS_TO_ACTIVATE;
	if ((es.randomNumber & 0x3) == 0) {
		struct door *d = &es.ld.doors[0];
		for (uint8_t door = 0; door < MAX_DOORS; door++) {
			if (d->flags & DOOR_FLAG_FLAKY && d->state == DOOR_CLOSED) {
				d->state = DOOR_OPENING;
				/*
				 * make sure to select a timeout value so the door never
				 * fully opens (0 < openTimeout < 63)
				 */
				d->openTimeout = ((es.randomNumber + 0x3) & 0x3f) - 1;
				es.activeDoors[newNrOfActiveDoors++] = door;
				maxNrOfActiveFlakyDoors--;
				/*
				 * if we have reached the maximum number of opening doors
				 * then stop here
				 */
				if (maxNrOfActiveFlakyDoors == 0)
					break;
			}
			d++;
		}
	}
	es.nrOfActiveDoors = newNrOfActiveDoors;
}

void Engine::update(void)
{
	/* reset system events */
	resetSystemEvents();

	if (es.itemDance == -4)
		es.itemDanceDir = 1;
	else if (es.itemDance == 4)
		es.itemDanceDir = -1;

	es.itemDance += es.itemDanceDir;

	/* generate random number for hit/evade randomization */
	/* random number from 0 - 63 */
	es.randomNumber = rand() % 64;

	/*
	 * work on local copies as this generates more efficient
	 * code for variable whos value gets changed due to some
	 * operation (not just assignment)
	 */
	int8_t movementSpeedDuration = es.movementSpeedDuration;
	/*
	 * cool down movement, turning and head movement
	 */
	if (movementSpeedDuration > 0)
		movementSpeedDuration -= 1;
	else
		es.direction = 0; /* stop movement */

	int8_t turn = es.turn;

	/* check if the player is still turning */
	if (turn) {
		if (turn > 0)
			turn -= 0x4;
		else
			turn += 0x4;
	}

	int8_t vHeadPosition = es.vHeadPosition;
	uint8_t vHeadPositionTimeout = es.vHeadPositionTimeout;

	/* timeout the head displacement */
	if (vHeadPositionTimeout == 0)
		vHeadPosition = 0;
	else
		vHeadPositionTimeout -= 1;

	/*
	 * input handling
	 */
	uint8_t movementUpdate = 0;
	if (pressed(UP_BUTTON)) {
		if (es.direction != 1)
			movementSpeedDuration = 0;
		es.direction = 1;
		movementUpdate = 1;
	} else if (pressed(DOWN_BUTTON)) {
		if (es.direction != 2)
			movementSpeedDuration = 0;
		es.direction = 2;
		movementUpdate = 1;
	} else if (pressed(LEFT_BUTTON)) {
		if (pressed(A_BUTTON)) {
			es.direction = 3;
			movementUpdate = 1;
		} else {
			turn -= 12;
		}
	} else if (pressed(RIGHT_BUTTON)) {
		if (pressed(A_BUTTON)) {
			es.direction = 4;
			movementUpdate = 1;
		} else {
			turn += 12;
		}
	}

	if (movementUpdate) {
		/* make sure movement is not too fast */
		if (movementSpeedDuration < 14) {
			/* extend the slide period */
			movementSpeedDuration += 2;
		}

		if (vHeadPosition == 2 << 4) {
			/* if the head is at the upper edge, change direction */
			es.vMoveDirection = -8;
		} else if (vHeadPosition == -2 << 4) {
			/* if the head is at the lower edge, change direction */
			es.vMoveDirection = 8;
		}

		/* update head position */
		vHeadPosition += es.vMoveDirection;

		/* set timeout at which the head jumps back to zero position */
		vHeadPositionTimeout = 5;
	}

	es.vHeadPositionTimeout = vHeadPositionTimeout;
	es.vHeadPosition = vHeadPosition;
	es.movementSpeedDuration = movementSpeedDuration;

	/* make sure we do not exceed the maximum turn rate */
	if (turn > 64)
		turn = 64;

	if (turn < -64)
		turn = -64;

	es.turn = turn;

	updateDoors();
	updateTriggers();
	updateSpecialWalls();

	/*
	 * update attack counters
	 */
	attackLevel += es.randomNumber;
	if (attackCoolDown == 0) {
		attackCoolDown = FPS;
	} else {
		attackCoolDown--;
	}

	updateTextureEffects();

	/*
	 * calculate players new view angle
	 *
	 * turning angle can be positive or negative (Q3.4 format)
	 */
	es.ld.playerAngle += es.turn / 16;
	if (es.ld.playerAngle < 0)
		es.ld.playerAngle += 360;
	else if (es.ld.playerAngle >= 360)
		es.ld.playerAngle -= 360;


	/****************************************************
	 *
	 * calculate players weapon offset and cooldown
	 *
	 */
	/*
	 * set the countdown to double the field of view
	 * so the weapon actually does not fire as this
	 * countdown is decremented in the render loop which
	 * loops over the field of view
	 */
	es.fireCountdown = FIELD_OF_VIEW * 2;
	es.rectOffset = 0;
	uint8_t weaponCoolDown = es.weaponCoolDown;

	if (weaponCoolDown) {
		/* weapon is still cooling down */
		if (es.weaponVisibleOffset)
			es.weaponVisibleOffset -= 4;
		if (es.weaponVisibleOffset <= 16)
			es.rectOffset = 16;
		else
			es.rectOffset = 32;
		weaponCoolDown -= 1;
	} else if (pressed(B_BUTTON)) {
		/*
		 * set the countdown to half the field of view
		 * so the weapon fires in the middle of the screen
		 */
		es.fireCountdown = FIELD_OF_VIEW / 2;
		/* check for ammo */
		if (es.playerActiveWeapon != 0) {
			if (es.playerAmmo[es.playerActiveWeapon] == 0) {
				es.fireCountdown = FIELD_OF_VIEW * 2;
				/* play out of ammo sound */
				startAudioEffect(AUDIO_EFFECT_ID_WEAPON, es.playerActiveWeapon + NR_OF_WEAPONS);
			} else {
				es.playerAmmo[es.playerActiveWeapon] -= 1;
			}
		}
		/* check if player can fire */
		if (es.fireCountdown == (FIELD_OF_VIEW / 2)) {
			es.weaponVisibleOffset = 4 << 4;
			weaponCoolDown = 0 + es.weaponVisibleOffset / 4;
			startAudioEffect(AUDIO_EFFECT_ID_WEAPON, es.playerActiveWeapon);
		}
	} else {
		es.weaponVisibleOffset = es.vHeadPosition;
	}
	es.weaponCoolDown = weaponCoolDown;

	/*
	 * calculate tile in players view direction, the resulting tile can be
	 * used to check if doors need to open or triggers are toggled
	 */
	if (pressed(A_BUTTON)) {
		int16_t sinPlayerAngle = es.ld.playerAngle - 90;

		if (sinPlayerAngle < 0)
			sinPlayerAngle += 360;

		uint16_t newPlayerX = es.ld.playerX + pgm_cosByX(es.ld.playerAngle) * BLOCK_SIZE / COSBYX;
		uint16_t newPlayerY = es.ld.playerY + pgm_cosByX(sinPlayerAngle) * BLOCK_SIZE / COSBYX;

		uint8_t blockX = divU16ByBlocksize(newPlayerX);
		uint8_t blockY = divU16ByBlocksize(newPlayerY);

		uint8_t actionTile = checkSolidBlockCheap(blockX, blockY);

		/*
		 * check if door needs to be activated
		 *
		 */
		// TODO this is still effective in simulation mode
		if (actionTile == H_DOOR && arduboy->justPressed(A_BUTTON)) {
			/* open the door */
			findAndActivateDoor();
		} else if (actionTile == TRIGGER || actionTile == V_M_W) {
			/* trigger some event */
			activateTrigger(blockX, blockY);
		} else if (es.direction < 3) {
			/*
			 * TODO if player presses A to strafe the weapon will be changed
			 */
			if (es.weaponChangeCooldown == 0) {
				/* change weapon if weapon is available */
				uint8_t w = (es.playerActiveWeapon + 1) % NR_OF_WEAPONS;
				for (; w < NR_OF_WEAPONS; w++) {
					if (es.playerWeapons & bitshift_left[w])
						break;
				}
				es.playerActiveWeapon = w % NR_OF_WEAPONS;
				es.weaponChangeCooldown = FPS / 2;
			}
		}
	}

	uint8_t tile = checkIgnoreBlock(es.ld.playerMapX, es.ld.playerMapY);
	if (tile == FLOOR_TRIGGER) {
		struct trigger *t = &es.ld.triggers[MAX_TRIGGERS - 1];

		/* floor triggers are sorted to the end of the triggers array */
		for (;;) {
			if (t->mapX == es.ld.playerMapX && t->mapY == es.ld.playerMapY) {
				/* set state */
				uint8_t old_state = t->flags & TRIGGER_FLAG_STATE;
				t->flags |= TRIGGER_STATE_ON;
				runTriggerAction(old_state, t);
				break;
			}
			t--;
		}
	}

	/* handle sprite respawn */
	/*
	 * clip es.randomNumber to nr_of_sprites
	 * if sprite is inactive, restore health/flags, stop
	 */
	if (es.spriteRespawnTimeout == 0) {
		uint8_t sprite_id = es.randomNumber < es.ld.nr_of_sprites? es.randomNumber: es.ld.nr_of_sprites - 1;
		struct sprite *s = &es.ld.dynamic_sprites[sprite_id];
		if (IS_INACTIVE(s->flags)) {
			/* calculate position of leveldata in flash */
			uint24_t levelOffset = levelData_flashoffset + (levelDataAlignment * currentLevel);

			/* reload positon and flags */
			FX::readDataBytes(levelOffset + (MAP_WIDTH * MAP_HEIGHT) + offsetof(struct level_initdata, dynamic_sprites) + (sizeof(struct sprite) * sprite_id),
					  (uint8_t *)s,
					  (size_t)sizeof(struct sprite));
			/* reload health */
			FX::readDataBytes(levelOffset + (MAP_WIDTH * MAP_HEIGHT) + offsetof(struct level_initdata, dynamic_sprite_flags) + sprite_id,
					  (uint8_t *)&es.ld.dynamic_sprite_flags[sprite_id],
					  (size_t)1);

			es.spriteRespawnTimeout = SPRITE_RESPAWN_TIMEOUT;
		} else {
			/* try again next second */
			es.spriteRespawnTimeout++;
		}
	}

	/* update timer based on seconds */
	if ((es.frame % FPS) == 0) {
		es.spriteRespawnTimeout--;
	}
}

void Engine::doDamageForVMW(struct movingWall *mw)
{
	/* do set damage flags based on the category */
	if (mw->flags & VMW_FLAG_DAMAGE)
		es.doDamageFlags |= es.currentDamageCategory;
}

/*
 * return 0, all good
 * return 1, sprite was pushed back
 * return 2, sprite was pushed into a wall
 */
uint8_t Engine::movingWallPushBack(uint16_t *playerX, uint16_t *playerY, struct movingWall *mw, uint8_t flags)
{
	uint8_t ret = 0;
	uint16_t xs, ys, xe, ye;

	/*
	 * FIXME
	 * something is odd when wall speed is != 1, when player is walking in the same direction
	 * the wall is moving it cannot reach its min distance, then when the wall changes direction
	 * the min distance can be reached (e.g. the texture is suddenly closer)
	 */

	/* add offset to x if it is a horizontally moving wall */
	if ((mw->flags & (1 << 3)) != 0)
		xs = mw->mapX * BLOCK_SIZE + mw->offset;
	else
		xs = mw->mapX * BLOCK_SIZE;

	xe = xs + BLOCK_SIZE + minWallDistance;
	if (xs < minWallDistance)
		xs = 0;
	else
		xs -= minWallDistance;

	/* add offset to y if it is a vertically moving wall */
	if ((mw->flags & (1 << 3)) == 0)
		ys = mw->mapY * BLOCK_SIZE + mw->offset;
	else
		ys = mw->mapY * BLOCK_SIZE;

	ye = ys + BLOCK_SIZE + minWallDistance;
	if (ys < minWallDistance)
		ys = 0;
	else
		ys -= minWallDistance;

	if (*playerX > xs && *playerX < xe && *playerY > ys && *playerY < ye) {

		/* inside */
		uint16_t movingAngle;
		uint16_t nPlayerXY;
		uint8_t diffxy;

		if (flags & (1 << 3)) {
			/* moving angles for horizontally moving walls */
			movingAngle = flags & MW_DIRECTION_INC? 0: 180;
		} else {
			/* moving angles for vertically moving walls */
			movingAngle = flags & MW_DIRECTION_INC? 90: 270;
		}

		if (movingAngle == 90) {
			diffxy = ye - *playerY;
			nPlayerXY = *playerY + diffxy;
		} else if (movingAngle == 270) {
			diffxy = *playerY - ys;
			nPlayerXY = *playerY - diffxy;
		} else if (movingAngle == 180) {
			diffxy = *playerX - xs;
			nPlayerXY = *playerX - diffxy;
		} else {
			diffxy = xe - *playerX;
			nPlayerXY = *playerX + diffxy;
		}

		if (flags & (1 << 3)) {
			/* horizontally moving wall */
			ret = move(movingAngle, 1, playerX, playerY, diffxy);
			if (*playerX != nPlayerXY) {
				/* sprite/player died */
				ret = 2;
			}
		} else {
			/* vertically moving wall */
			ret = move(movingAngle, 1, playerX, playerY, diffxy);
			if (*playerY != nPlayerXY) {
				/* sprite/player died */
				ret = 2;
			}
		}
	}
	return ret;
}

/*
 * Speed translation table for moving walls. All speeds
 * must be a power of 2
 */
static const uint8_t movingWallSpeedXlate[4] PROGMEM = {
	1, 2, 4, 8,
};

void Engine::updateMoveables(void)
{
	/*
	 * update moving walls
	 */
	for (uint8_t mwi = 0; mwi < MAX_MOVING_WALLS; mwi++) {
		struct movingWall *mw = &es.ld.movingWalls[mwi];
		uint8_t flags = mw->flags;

		/* make sure wall is active */
		if ((flags & VMW_FLAG_ACTIVE) == 0)
			continue;

		uint8_t speed = (mw->flags >> 1) & 0x3;
		/* translate speed value */
		speed = pgm_read_uint8(&movingWallSpeedXlate[speed]);

		uint8_t mapXY;
		/* check if it is a horizontally or vertically moving wall */
		if (flags & (1 << 3))
			mapXY = mw->mapX;
		else
			mapXY = mw->mapY;

		if (mw->flags & MW_DIRECTION_INC) {
			if (mapXY == mw->max) {
				/* change direction */
				mw->flags &= ~MW_DIRECTION_INC;
				/* clear active flag if oneshot */
				if (mw->flags & VMW_FLAG_ONESHOT)
					mw->flags &= ~VMW_FLAG_ACTIVE;

			} else {
				/* moving down/right */
				mw->offset += speed;
				if (mw->offset == BLOCK_SIZE) {
					mw->offset = 0;
					mapXY++;
				}
			}
		} else {
			if (mapXY == mw->min && mw->offset == 0) {
				/* change direction */
				mw->flags |= MW_DIRECTION_INC;
				/* clear active flag if oneshot */
				if (mw->flags & VMW_FLAG_ONESHOT)
					mw->flags &= ~VMW_FLAG_ACTIVE;

			} else {
				/* moving up/left */
				if (mw->offset == 0) {
					mw->offset = BLOCK_SIZE;
					mapXY--;
				}

				mw->offset -= speed;
			}
		}
		/* write back the changed value */
		if (flags & (1 << 3)) {
			/* horizontally moving wall */
			mw->mapX = mapXY;
		} else {
			/* vertically moving wall */
			mw->mapY = mapXY;
		}

		/*
		 * check if the player takes damage from a moving wall or
		 * gets at least pushed back
		 *
		 * if MW_DIRECTION_INC the wall is moving down
		 */
		uint8_t ret = movingWallPushBack(&es.ld.playerX, &es.ld.playerY, mw, flags);
		switch (ret) {
		case 1:
			doDamageForVMW(mw);
			break;
		case 2:
			/* player dead */
			break;
		}

		/* now check for all sprites if they take damage or will be pushed
		 * back */
		for (uint8_t i = 0; i < es.ld.nr_of_sprites; i++) {
			struct sprite *s = &es.ld.dynamic_sprites[i];

			/* only sprites will take damage */
			es.currentDamageCategory = 2;

			uint16_t x = s->xy & 0xfff;
			uint16_t y = s->xy >> 12;
			uint8_t ret = movingWallPushBack(&x, &y, mw, flags);
			SPRITE_XY_SET(s, x, y);
			switch (ret) {
			case 1:
				/* inflict damage to the sprite */
				checkAndDoDamageToSpriteByObjects(i);
				break;
			case 2:
				/* sprite dead */
				// TODO
				break;
			}

			/* only the player will take damage */
			es.currentDamageCategory = 1;
		}
	}
}

/*
 * max tsize is 511
 * min tsize is 2
 */
uint16_t Engine::arc_s8(int8_t value, const int8_t *table, uint16_t tsize)
{
	uint8_t size = tsize / 2;
	uint16_t index = size - 1;
	uint16_t previous_index = index;
	int8_t tmp;

	/* binary search for best match of value */
	for (;;) {
		if (index >= tsize) {
			index = tsize - 1;
			break;
		}

		tmp = pgm_read_int8(&table[index]);
		if (tmp < value) {
			previous_index = index;
			index += (size + 1) / 2;
		} else if (tmp > value) {
			previous_index = index;
			index -= (size + 1) / 2;
		} else
			break;
		if (size < 2)
			break;
		size /= 2;
	}

	/* if difference of value and current index is smaller than the difference of the previous
	 * index and value then return the current index, otherwise the previous index is returned */
	if (abs(pgm_read_int8(&table[index]) - value) <= abs(tmp - value))
		return index;

	return previous_index;
}

/*
 * max tsize is 255
 * min tsize is 2
 */
// TODO could be the same code as for arc_s8 but for speed reasons some PROGMEM is wasted
uint16_t Engine::arc_u16(uint16_t value, const uint16_t *table, uint8_t tsize)
{
	uint8_t size = tsize;
	uint8_t index = size - 1;
	uint8_t previous_index = index;
	uint16_t tmp;

	/* binary search for best match of value */
	for (;;) {
		size /= 2;
		if (size == 0)
			break;
		if (index >= tsize) {
			index = tsize - 1;
			break;
		}

		tmp = pgm_read_uint16(&table[index]);
		if (tmp < value) {
			previous_index = index;
			index += (size + 1) / 2;
		} else if (tmp > value) {
			previous_index = index;
			index -= (size + 1) / 2;
		} else
			break;
	}
	/* if difference of value and current index is smaller than the difference of the previous
	 * index and value then return the current index, otherwise the previous index is returned */
	if (abs(pgm_read_uint16(&table[index]) - value) <= abs(tmp - value))
		return index;

	return previous_index;
}

uint8_t Engine::movingWallCheckHitVertical(uint8_t mapY, uint16_t a)
{
	uint8_t hit = 0;
	uint16_t b = 0;
	uint16_t mY = (uint16_t)mapY * BLOCK_SIZE;

	/*
	 *                  +-------------+-------------+
	 *                  |   :         |   :         |
	 *                  |   +         |   :         |
	 *                  |   |  b      |   :         |
	 *  P(x, y) +-----------+         |   :         |
	 *             a    |   :         |   :         |
	 *                  +-------------+-------------+
	 */

	if (es.nTempRayAngle != 90) {
		b = divU24ByBlocksize((uint32_t)(pgm_tanByX(es.tempRayAngle)) * a);
	} else {
		/*
		 * corner cases for angles of 90 degrees
		 *
		 *   a == 0: player faces block at 90 degrees from up or down,
		 *           this is a horizontal hit of the ray
		 *   a != 0: player faces block at 90 degrees from left or right,
		 *           this is a vertical hit of the ray
		 */
		if (a == 0)
			b = abs(es.ld.playerY - mY);
	}

	/*
	 * Check if playerY +- b is inside the block of a moving wall which would
	 * mean the ray would hit it somewhere and we need to calculate wallX and
	 * raylength.
	 * Just checking the upper and lower sides are ok because the wall is only
	 * moving horizontally.
	 */
	if (rayAngle < 180) {
		/*
		 * if ray is above the block
		 * (blockY + width of the block)
		 */
		hit = !!(((uint16_t)es.ld.playerY + b) <= (mY + BLOCK_SIZE - 1));
	} else {
		/* if ray is below the block */
		/*
		 * corner case:
		 *   if b > playerY: the ray cannot hit the block,
		 *   mostly the case when es.tempRayAngle gets close to 90
		 */
		if (b <= es.ld.playerY)
			hit = !!((es.ld.playerY - b) >= mY);
	}

	if (hit) {
		/* hit */
		if (rayAngle != 90) {
			// TODO use viewDirection for these kind of checks
			if (rayAngle < 180)
				es.wallX = es.ld.playerY + b - mY;
			else
				es.wallX = es.ld.playerY - b - mY;
		} else {
			es.wallX = es.ld.playerY % BLOCK_SIZE;
		}
		/*
		 * TODO
		 *
		 * split triangle into two so we can use multiplication
		 * with cosinus for the raylength calculation
		 */
		if (a == 0)
			es.renderRayLength = b;
		else
			es.renderRayLength = (uint32_t)a * COSBYX / pgm_cosByX(es.tempRayAngle);

	} else {
		/* pass */
		return 0;
	}
	/* hit */
	return 1;
}

uint8_t Engine::movingWallCheckHitHorizontal(uint8_t mapX, uint16_t a)
{
	uint8_t hit = 0;
	uint16_t b = 0;
	uint16_t mX = (uint16_t)mapX * BLOCK_SIZE;

	/*
	 *   P(x, y)
	 *    +
	 *    |\
	 *    | \
	 *  a |  \
	 * +--|---\------+
	 * |  |    \     |
	 * |~~+-----+~~~~|
	 * |     b       |
	 * |             |
	 * |             |
	 * +-------------+
	 * |             |
	 * |~~~~~~~~~~~~~|
	 * |             |
	 * |             |
	 * |             |
	 * +-------------+
	 */

	if (es.tempRayAngle != 90) {
		b = divU24ByBlocksize((uint32_t)(pgm_tanByX(es.nTempRayAngle)) * a);
	} else {
		/*
		 * corner cases for angles of 90 degrees
		 *
		 *   a == 0: player faces block at 90 degrees from left or right,
		 *           this is a vertical hit of the ray
		 *   a != 0: player faces block at 90 degrees from up or down,
		 *           this is a horizontal hit of the ray
		 */
		if (a == 0)
			b = abs(es.ld.playerX - mX);
	}

	/*
	 * Check if playerX +- b is inside the block of a moving wall which would
	 * mean the ray would hit it somewhere and we need to calculate wallX and
	 * raylength.
	 * Just checking the left and right sides are ok because the wall is only
	 * moving vertically.
	 */
	if (rayAngle < 90 || rayAngle >= 270) {
		/*
		 * if ray is on the left side of the block
		 * (blockX + width of the block)
		 */
		hit = !!(((uint16_t)es.ld.playerX + b) <= (mX + BLOCK_SIZE - 1));
	} else {
		/* if ray if on the right side of the block */
		/*
		 * corner case:
		 *   if b > playerX: the ray cannot hit the block,
		 *   mostly the case when es.tempRayAngle gets close to 90
		 */
		if (b <= es.ld.playerX)
			hit = !!((es.ld.playerX - b) >= mX);
	}

	if (hit) {
		/* hit */
		if (rayAngle != 90) {
			// TODO use viewDirection for these kind of checks
			if (rayAngle < 90 || rayAngle >= 270)
				es.wallX = es.ld.playerX + b - mX;
			else
				es.wallX = es.ld.playerX - b - mX;
		} else {
			es.wallX = es.ld.playerX % BLOCK_SIZE;
		}
		/*
		 * TODO
		 *
		 * split triangle into two so we can use multiplication
		 * with cosinus for the raylength calculation
		 */
		if (a == 0)
			es.renderRayLength = b;
		else
			es.renderRayLength = (uint32_t)a * COSBYX / pgm_cosByX(es.nTempRayAngle);

	} else {
		/* pass */
		return 0;
	}
	/* hit */
	return 1;
}

/*
 * execute dedicated renderer for special objects like e.g. moving walls and
 * calculate vertical intersections with them (if the players coordinates are
 * inside the block to render)
 */
uint8_t Engine::checkIgnoreBlockInnerVertical(uint8_t pMapX, uint8_t pMapY, uint8_t run)
{
	if (run)
		return F0;

	uint8_t tile = checkIgnoreBlock(pMapX, pMapY);

	/* only handle moving walls at the moment */
	if (tile != V_M_W)
		return F0;

	struct movingWall *mw;
	uint8_t mXinc = 0;
	uint8_t mwi;

	/* find the moving wall tile the player is in */
	for (mwi = 0; mwi < MAX_MOVING_WALLS; mwi++) {

		mw = &es.ld.movingWalls[mwi];

		/* ignore if it is a vertical moving wall */
		if ((mw->flags & (1 << 3)) == 0)
			continue;

		if (mw->offset == 0)
			continue;
		/*
		 * If the offset is not zero we need to check if
		 * the the current block is the current position
		 * of the moving wall or if it is the second
		 * block in which the moving wall is partially in.
		 *
		 * shortcut: as we are inside the block it is clear that
		 *
		 * block1: playerAngle (< 90  || >= 270) -> ray will miss the block
		 * block2: playerAngle (>= 90 && < 270) -> ray will miss the block
		 *
		 */
		if (pMapY != mw->mapY)
			continue;

		if (pMapX != mw->mapX) {

			if (pMapX != mw->mapX + 1)
				continue;

			if (rayAngle < 90 || rayAngle >= 270) {
				/* miss */
				continue;
			}

			/* we are in block2 */
			mXinc = 1;
		} else if (rayAngle >= 90 && rayAngle < 270) {
			/* miss */
			continue;
		}

		/* found a matching wall */
		break;
	}

	/* nothing found */
	if (mwi == MAX_MOVING_WALLS)
		return F0;

	/* check if ray hits the wall */
	uint16_t mX = (mw->mapX + mXinc) * BLOCK_SIZE;
	uint16_t a;

	if (mXinc) {
		/*
		 * ================ block 2 ==============
		 */
		a = es.ld.playerX - mX - mw->offset + 1;
	} else {
		/*
		 * ================ block 1 ==============
		 */
		a = mX + mw->offset - es.ld.playerX;
	}

	if (movingWallCheckHitVertical(mw->mapY, a)) {
		/* the wall found is the current moving wall */
		es.cmw = mw;
		return H_M_W_V;
	}

	/* ray missed the wall */
	return F0;
}

/*
 * execute dedicated renderer for special objects like e.g. moving walls and
 * calculate horizontal intersections with them (if the players coordinates are
 * inside the block to render)
 */
uint8_t Engine::checkIgnoreBlockInnerHorizontal(uint8_t pMapX, uint8_t pMapY, uint8_t run)
{
	if (run)
		return F0;

	uint8_t tile = checkIgnoreBlock(pMapX, pMapY);

	/* only handle moving walls at the moment */
	if (tile != V_M_W)
		return F0;

	struct movingWall *mw;
	uint8_t mYinc = 0;
	uint8_t mwi;

	/* find the moving wall tile the player is in */
	for (mwi = 0; mwi < MAX_MOVING_WALLS; mwi++) {

		mw = &es.ld.movingWalls[mwi];

		/* ignore if it is a horizontal moving wall */
		if ((mw->flags & (1 << 3)) != 0)
			continue;

		if (mw->offset == 0)
			continue;
		/*
		 * If the offset is not zero we need to check if
		 * the the current block is the current position
		 * of the moving wall or if it is the second
		 * block in which the moving wall is partially in.
		 *
		 * shortcut: as we are inside the block it is clear that
		 *
		 * block1: playerAngle >= 180 -> ray will miss the block
		 * block2: playerAngle < 180 -> ray will miss the block
		 *
		 */
		if (pMapX != mw->mapX)
			continue;

		if (pMapY != mw->mapY) {

			if (pMapY != mw->mapY + 1)
				continue;

			if (rayAngle < 180) {
				/* miss */
				continue;
			}

			/* we are in block2 */
			mYinc = 1;
		} else if (rayAngle >= 180) {
			/* miss */
			continue;
		}

		/* found a matching wall */
		break;
	}

	/* nothing found */
	if (mwi == MAX_MOVING_WALLS)
		return F0;

	/* check if ray hits the wall */
	uint16_t mY = (mw->mapY + mYinc) * BLOCK_SIZE;
	uint16_t a;

	if (mYinc) {
		/*
		 * ================ block 2 ==============
		 */
		a = es.ld.playerY - mY - mw->offset + 1;
	} else {
		/*
		 * ================ block 1 ==============
		 */
		a = mY + mw->offset - es.ld.playerY;
	}

	if (movingWallCheckHitHorizontal(mw->mapX, a)) {
		/* the wall found is the current moving wall */
		es.cmw = mw;
		return V_M_W_H;
	}

	/* ray missed the wall */
	return F0;
}

uint8_t Engine::checkIfMovingWallHit(uint8_t mapX, uint8_t mapY, uint16_t hX, uint16_t hY)
{
	uint8_t tile;

	/*
	 * handle moving walls
	 */
	struct movingWall *mw;
	uint8_t block2;

	tile = F0;
	for (uint8_t mwi = 0; mwi < MAX_MOVING_WALLS; mwi++) {
		uint8_t blockY;

		mw = &es.ld.movingWalls[mwi];

		if (mw->flags & (1 << 3)) {
			/* horizontal moving walls */
			/*
			 * no need to check map corner case as all maps should have a
			 * block border!
			 */
			blockY = mw->mapX;

			/*
			 * check if block we hit on the map is the current block1 of
			 * a moving wall
			 */
			if (mapY != mw->mapY)
				continue;

			if (mw->mapX != mapX) {

				if (mw->offset == 0)
					continue;

				if (mapX != mw->mapX + 1)
					continue;

				block2 = 1;
			} else {
				block2 = 0;
			}

			blockY += block2;

			uint16_t mX = (uint16_t)blockY * BLOCK_SIZE;
			uint16_t offset = mX + mw->offset;

			/* decide to either hit/pass/calc from the left side and right side */
			if (rayAngle < 90 || rayAngle >= 270) {
				/* Q1/Q4 left side */
				if (block2) {
					/* block 2 */
					if (hX <= offset) {
						/* hit */
						tile = H_M_W;
					} else {
						/* pass */
					}
				} else {
					/* block 1 */
					if (hX <= offset) {
						uint16_t a = mX - es.ld.playerX + mw->offset;
						/* calc */
						if (movingWallCheckHitVertical(mapY, a)) {
							tile = H_M_W_V;
						}
					} else {
						/* hit */
						tile = H_M_W;
					}
				}
			} else {
				/* Q2/Q3 right side */
				if (block2) {
					/* block 2 */
					if (hX >= offset) {
						uint16_t a = es.ld.playerX - mX - mw->offset + 1;
						/* calc */
						if (movingWallCheckHitVertical(mapY, a)) {
							tile = H_M_W_V;
						}
					} else {
						/* hit */
						tile = H_M_W;
					}
				} else {
					/* block 1 */
					if (hX >= offset) {
						/* hit */
						tile = H_M_W;
					} else {
						/* pass */
					}
				}
			}
		} else {
			/* vertical moving walls */
			/*
			 * no need to check map corner case as all maps should have a
			 * block border!
			 */
			blockY = mw->mapY;

			/*
			 * check if block we hit on the map is the current block1 of
			 * a moving wall
			 */
			if (mapX != mw->mapX)
				continue;

			if (mw->mapY != mapY) {

				if (mw->offset == 0)
					continue;

				if (mapY != mw->mapY + 1)
					continue;

				block2 = 1;
			} else {
				block2 = 0;
			}

			blockY += block2;

			uint16_t mY = (uint16_t)blockY * BLOCK_SIZE;
			uint16_t offset = mY + mw->offset;

			/* decide to either hit/pass/calc */
			if (rayAngle < 180) {
				/* Q1/Q2 */
				if (block2) {
					/* block 2 */
					if (hY <= offset) {
						/* hit */
						tile = V_M_W;
					} else {
						/* pass */
					}
				} else {
					/* block 1 */
					if (hY <= offset) {
						uint16_t a = mY - es.ld.playerY + mw->offset;
						/* calc */
						if (movingWallCheckHitHorizontal(mapX, a)) {
							tile = V_M_W_H;
						}
					} else {
						/* hit */
						tile = V_M_W;
					}
				}
			} else {
				/* Q3/Q4 */
				if (block2) {
					/* block 2 */
					if (hY >= offset) {
						uint16_t a = es.ld.playerY - mY - mw->offset + 1;
						/* calc */
						if (movingWallCheckHitHorizontal(mapX, a)) {
							tile = V_M_W_H;
						}
					} else {
						/* hit */
						tile = V_M_W;
					}
				} else {
					/* block 1 */
					if (hY >= offset) {
						/* hit */
						tile = V_M_W;
					} else {
						/* pass */
					}
				}
			}
		}
		break;
	}
	if (tile != F0) {
		if (mw->flags & (1 << 3)) {
			/* calculate wallX for horizontal moving walls */
			/* hit */
			if (es.wallX == -1) {
				/*
				 * calculate wallX for all hits not done via
				 * movingWallCheckHitVertical. If hX is on a block boundary and
				 * the moving wall offset is also at a block boundary then the wallX
				 * must be derived from hY instead of hX.
				 */
				if ((mw->offset == 0) &&
				    ((hX % BLOCK_SIZE == 0) || ((hX + 1) % BLOCK_SIZE == 0))) {
					es.wallX = hY % BLOCK_SIZE;
				} else {
					if (block2)
						es.wallX = hX % BLOCK_SIZE + BLOCK_SIZE - mw->offset;
					else
						es.wallX = hX % BLOCK_SIZE - mw->offset;
				}
			}
		} else {
			/* calculate wallX for vertical moving walls */
			/* hit */
			if (es.wallX == -1) {
				/*
				 * calculate wallX for all hits not done via
				 * movingWallCheckHitHorizontal. If hY is on a block boundary and
				 * the moving wall offset is also at a block boundary then the wallX
				 * must be derived from hX instead of hY.
				 */
				if ((mw->offset == 0) &&
				    ((hY + 1) % BLOCK_SIZE == 0)) {
					es.wallX = hX % BLOCK_SIZE;
				} else {
					if (block2)
						es.wallX = hY % BLOCK_SIZE + BLOCK_SIZE - mw->offset;
					else
						es.wallX = hY % BLOCK_SIZE - mw->offset;
				}
			}
		}
		es.cmw = mw;
	}
	return tile;
}

uint8_t Engine::checkIgnoreBlockFast(uint8_t mapX, uint8_t mapY)
{
#if defined(CONFIG_MAP_CACHE)
	uint8_t tile = readThroughCache(mapX, mapY);
#else
	/* ~4ms */
	uint8_t tile = checkIgnoreBlock(mapX, mapY);
#endif
	if (tile == H_DOOR) {
		/*
		 * check if we can just ignore that door
		 */
		tile = checkIfDoorToBeIgnored(mapX, mapY);

	} else if (tile == TRIGGER) {
		for (uint8_t t = 0; t < MAX_TRIGGERS; t++) {
			struct trigger *trig = &es.ld.triggers[t];
			if (trig->mapX == mapX && trig->mapY == mapY) {
				es.ct = trig;
				break;
			}
		}
	} else if (tile == FLOOR_TRIGGER) {
		tile = F0;
	}
	return tile;
}

#ifndef CONFIG_ASM_OPTIMIZATIONS
uint8_t Engine::checkIgnoreBlock(uint8_t mapX, uint8_t mapY)
{
	FX::seekData(levelFlashOffset + (MAP_WIDTH * mapY + mapX));
	return FX::readEnd();
}
#endif

int8_t Engine::checkDoor(uint8_t wallX)
{
	for (uint8_t door = 0; door < es.nrOfActiveDoors; door++) {
		uint8_t doorId = es.activeDoors[door];
		struct door *d = &es.ld.doors[doorId];
		if (d != es.cd)
			continue;
		if (wallX >= d->offset) {
			/* ray hits the door */
			return wallX - d->offset;
		} else {
			es.ignoreBlock[es.blocksToIgnore++] = doorId;
			return -1;
		}
	}
	return wallX;
}

void Engine::cleanIgnoreBlock(void)
{
	uint8_t newBlocksToIgnore = 0;

	/* TODO problematic for objects other than doors */
	for (uint8_t block = 0; block < es.blocksToIgnore; block++) {
		uint8_t blockNr = es.ignoreBlock[block];
		struct door *d = &es.ld.doors[blockNr];
		if (d->state == DOOR_OPEN)
			es.ignoreBlock[newBlocksToIgnore++] = blockNr;
	}
	es.blocksToIgnore = newBlocksToIgnore;
}

uint8_t Engine::checkIfDoorToBeIgnored(uint8_t mapX, uint8_t mapY)
{
	uint8_t door;

	/* find the door at mapX/mapY */
	for (door = 0; door < MAX_DOORS; door++) {
		struct door *d = &es.ld.doors[door];
		if (d->mapX == mapX && d->mapY == mapY) {

			/* check if the door is on the ignore list */
			for (uint8_t block = 0; block < es.blocksToIgnore; block++) {
				uint8_t blockNr = es.ignoreBlock[block];
				if (door == blockNr) {
					return F0;
				}
			}

			/* set current door so it can be rendered correctly */
			es.cd = d;

			break;
		}
	}

	// TODO what if door not found?

	return H_DOOR;
}

// TODO use cache function!
uint8_t Engine::checkSolidBlockCheap(uint8_t mapX, uint8_t mapY)
{
	uint8_t tile = checkIgnoreBlock(mapX, mapY);

	/* reset current moving wall */
	es.cmw = NULL;

	if (tile >= SPRITES_START || tile == FLOOR_TRIGGER)
		return F0;

	/* check if it is an open door */
	if (tile == H_DOOR) {
		/*
		 * check if we can just ignore that door
		 */
		tile = checkIfDoorToBeIgnored(mapX, mapY);
	}

	/* if not a moving wall, return immediately */
	if (tile != V_M_W)
		return tile;

	struct movingWall *mw;
	uint8_t mwi;

	mw = &es.ld.movingWalls[0];

	/* find the moving wall tile the player is in */
	for (mwi = 0; mwi < MAX_MOVING_WALLS; mwi++, mw++) {

		if (mw->flags & (1 << 3)) {
			if (mapY != mw->mapY)
				continue;

			if (mapX != mw->mapX) {

				if (mapX != mw->mapX + 1)
					continue;

				/* we are in block2 */
			}
		} else {
			if (mapX != mw->mapX)
				continue;

			if (mapY != mw->mapY) {

				if (mapY != mw->mapY + 1)
					continue;

				/* we are in block2 */
			}
		}
		/* else: we are in block1 */

		/* found a matching wall */
		/* set current moving wall */
		es.cmw = mw;

		return V_M_W;
	}
	return F0;
}

/*
 * the corners of each block are named in the following order
 *
 *       A--------------B
 *       |              |
 *       |              |
 *       |              |
 *       |              |
 *       |              |
 *       |              |
 *       D--------------C
 *
 *   +---+---+---+
 *   | 7 | 8 | 9 |
 *   +---+---+---+
 *   | 6 | 1 | 2 |           +------ 0°
 *   +---+---+---+           |
 *   | 5 | 4 | 3 |           |
 *   +---+---+---+           |
 *
 *                          90°
 */
/* viewAngle < 90 */
/*
 *   .   +---+
 *       | A |
 *   +---+---+
 *   | C | B |
 *   +---+---+
 *
 */
/* viewAngle < 180 */
/*
 *   +---+   .
 *   | C |
 *   +---+---+
 *   | B | A |
 *   +---+---+
 *
 */
/* viewAngle < 270 */
/*
 *   +---+---+
 *   | B | C |
 *   +---+---+
 *   | A |
 *   +---+   .
 *
 */
/* viewAngle < 360 */
/*
 *   +---+---+
 *   | A | B |
 *   +---+---+
 *       | C |
 *   .   +---+
 *
 */
uint8_t Engine::calcDistance(uint16_t pA, uint16_t pB)
{
	if (pA < pB)
		return (uint8_t)(pB - pA);

	return (uint8_t)(pA - pB);
}

/*
 * the corners of each block are named in the following order
 *
 *       A--------------B
 *       |              |
 *       |              |
 *       |              |
 *       |              |
 *       |              |
 *       |              |
 *       D--------------C
 *
 *   +---+---+---+
 *   | 6 | 7 | 8 |
 *   +---+---+---+
 *   | 5 | 0 | 1 |           +------ 0°
 *   +---+---+---+           |
 *   | 4 | 3 | 2 |           |
 *   +---+---+---+           |
 *
 *                          90°
 */
static const uint8_t quadrant2blockId[4 * 6] PROGMEM = {
	0, 8, 1, 2, 3, 4,
	0, 2, 3, 4, 5, 6,
	0, 4, 5, 6, 7, 8,
	0, 6, 7, 8, 1, 2,
};

/*
 * quadrant number to map direction (columns are (x:y))
 */
static const int8_t q2m[4*2] PROGMEM = {
	 1,  1,
	-1,  1,
	-1, -1,
	 1, -1,
};

/*
 * blockId to map direction (columns are (x:y))
 */
static const int8_t blockId2mapOffset[9 * 2] PROGMEM = {
	 0,  0, /* 0 */
	 1,  0, /* 1 */
	 1,  1, /* 2 */
	 0,  1, /* 3 */
	-1,  1, /* 4 */
	-1,  0, /* 5 */
	-1, -1, /* 6 */
	 0, -1, /* 7 */
	 1, -1, /* 8 */
};

/*
 * do a bounding box check, below example for y-intersection
 *
 *  ulp  +-------------------+
 *       |                   ip
 *       |                   | \
 *       |                 A |  \ mmd
 *       |                   |   \
 *       |                   |    \
 *       |                   b-----c
 *       |                   |  B
 *       |                   |
 *       |                   |
 *       +-------------------+ lrp
 *
 * ulp/lrp are the outer bounds of the box including the
 *         minimal distance to the block
 * ml      is the leftmost/upmost coordinate of the block
 *         inside the box defined by ulp/lrp, finally it will
 *         be set to either ulp or lrp as movement limit
 * B       distance from point b to c
 * A       distance from point b to ip
 * mmd     distance from point c to ip, maximum allowed movement distance
 * ip      intersection point with desired axis
 */
void Engine::checkIntersect(struct intersect *i)
{
	uint32_t mmd, td;

	/*
	 * calculate upper/left point of the bounding box
	 * by substracting the minimal wall distance
	 */
	if (i->ml <= minWallDistance)
		i->ulp = 0;
	else
		i->ulp = i->ml - minWallDistance;

	/*
	 * calculate lower/right point of the bounding box
	 * by adding the block size and the minimum wall
	 * distance
	 */
	i->lrp = i->ml + BLOCK_SIZE + minWallDistance;

	/*
	 * if the current view it torwards the upper/left xy-limit
	 * of the bounding box then the movement limit will be
	 * set to it, otherwise the lower/right xy-limit will be chosen
	 */
	if (i->quadrant == 0 || i->quadrant == (1 + i->s * 2)) {
		i->ml = i->ulp;
	} else {
		i->ml = i->lrp;
	}
	mmd = calcDistance(i->oX, i->ml); /* calc B */

	if (i->quadrant == 1 || i->quadrant == 3) {
		td = ((uint32_t)pgm_tanByX(i->sin) * mmd); /* calc A */
		/* if td gets zero then mmd is simply B */
		if (td) {
			uint8_t a = pgm_cosByX(i->cos);
			mmd = td;
			if (a)
				mmd /= a;
		}
	} else {
		td = ((uint32_t)pgm_tanByX(i->cos) * mmd); /* calc A */
		/* if td gets zero then mmd is simply B */
		if (td) {
			uint8_t a = pgm_cosByX(i->sin);
			mmd = td;
			if (a)
				mmd /= a; /* calc mmd */
		}
	}
	td = divU24ByBlocksize(td); /* fixup A */
	/*
	 * calculate intersection point (a) with bounding box,
	 * depending on (s) it is either an x or an y intersection
	 */
	i->ip = i->oY + (td * pgm_read_int8(&q2m[i->quadrant * 2 + i->s]));
	/* if mmd is shorter than the movement distance (distance) then
	 * mmd will be returned as result, otherwise the max distance
	 * is assumed (will fallthrough the intersection test later)
	 * and returned as result
	 */
	if (mmd < i->distance) {
		i->mmd = mmd;
	} else
		i->mmd = i->distance;
}

uint8_t Engine::move(uint16_t viewAngle, uint8_t direction, uint16_t *x, uint16_t *y, uint8_t distance)
{
	int16_t nX, nY;
	int16_t dX, dY;
	uint16_t oX = *x;
	uint16_t oY = *y;
	uint8_t hit = 0;

	if (!direction)
		return 0;

	/* calculate new position */
	if (direction == 4) {
		/* strafe right
		 * viewAngle + 90
		 */
		direction = 1;
		/*
		 * sinViewAngle = viewAngle - 90, so set it now as
		 * viewAngle will be increased by 90
		 */
		viewAngle += 90;
		if (viewAngle >= 360)
			viewAngle -= 360;
	} else if (direction == 3) {
		/* strafe left
		 * viewAngle - 90
		 */
		direction = 1;
		/*
		 * viewAngle = sinViewAngle - 90, so set it now as
		 * sinViewAngle will be decreased by 90
		 */
		if (viewAngle < 90)
			viewAngle += 360;
		viewAngle -= 90;
	}

	if (direction == 2) {
		/*
		 * invert viewAngle when the object is moving backward
		 * to simplify the below if/else statements
		 */
		viewAngle += 180;
		if (viewAngle >= 360)
			viewAngle -= 360;
	}

	/* calc sinViewAngle from original view angle */
	int16_t oSinViewAngle = (int16_t)viewAngle - 90;
	if (oSinViewAngle < 0)
		oSinViewAngle += 360;

	dX = divS24ByBlocksize(pgm_cosByX(viewAngle) * distance);
	dY = divS24ByBlocksize(pgm_cosByX(oSinViewAngle) * distance);

	nX = oX + dX;
	nY = oY + dY;

	/*
	 * check for wall collision depending on the objects direction
	 */
	uint8_t pMapX, pMapY;

	/* calculate current block position on the map */
	pMapX = divU16ByBlocksize(oX);
	pMapY = divU16ByBlocksize(oY);

	uint8_t quadrant;
	uint8_t cosViewAngle;

	if (viewAngle < 90) {
		quadrant = 0;
		cosViewAngle = viewAngle;
	} else if (viewAngle < 180) {
		quadrant = 1;
		cosViewAngle = viewAngle - 90;
	} else if (viewAngle < 270) {
		quadrant = 2;
		cosViewAngle = viewAngle - 180;
	} else {
		quadrant = 3;
		cosViewAngle = viewAngle - 270;
	}

	uint8_t sinViewAngle = 90 - cosViewAngle;

	/* use the start of the frame buffer as intermediate buffer */
	struct intersect *xI = (struct intersect *)(arduboy->getBuffer() + 0);
	struct intersect *yI = xI + 1;

	xI->oX = oY;
	xI->oY = oX;
	xI->s = 0;
	xI->quadrant = quadrant;
	xI->cos = sinViewAngle;
	xI->sin = cosViewAngle;
	xI->distance = distance;

	yI->oX = oX;
	yI->oY = oY;
	yI->s = 1;
	yI->quadrant = quadrant;
	yI->cos = cosViewAngle;
	yI->sin = sinViewAngle;
	yI->distance = distance;

	uint8_t is = quadrant * 6, ie = is + 6;

	for (uint8_t i = is; i < ie; i++) {
		uint8_t blockId = pgm_read_uint8(&quadrant2blockId[i]);
		int8_t mapXoff = pgm_read_int8(&blockId2mapOffset[blockId * 2 + 0]);
		int8_t mapYoff = pgm_read_int8(&blockId2mapOffset[blockId * 2 + 1]);
		uint16_t bx, by;
		uint8_t cMapX = pMapX + mapXoff;
		uint8_t cMapY = pMapY + mapYoff;

		if (checkSolidBlockCheap(cMapX, cMapY) == F0)
			continue;

		/*
		 *  if moving wall, properly add the offset
		 */
		if (es.cmw && ((es.cmw->flags & (1 << 3)) == 0)) {
			by = es.cmw->mapY * BLOCK_SIZE + es.cmw->offset;
		} else {
			by = cMapY * BLOCK_SIZE;
		}


		xI->ml = by;

		checkIntersect(xI);

		/*
		 * calculate values for y-axis intersection check
		 */
		/*
		 *  if moving wall, properly add the offset
		 */
		if (es.cmw && ((es.cmw->flags & (1 << 3)) != 0)) {
			bx = es.cmw->mapX * BLOCK_SIZE + es.cmw->offset;
		} else {
			bx = cMapX * BLOCK_SIZE;
		}


		yI->ml = bx;

		checkIntersect(yI);

		/*
		 * the ray may hit both sides, so always take the shortest
		 * ray
		 */
		if (xI->mmd <= yI->mmd)
			yI->mmd = distance;
		if (yI->mmd <= xI->mmd)
			xI->mmd = distance;

		/*
		 * Check for x-axis intersection:
		 *  If distance from P to intersection point is less than the
		 *  movement distance (e.g. the number of pixels per move) then
		 *  we would intersect
		 */
		if (xI->ip > (int16_t)yI->ulp && xI->ip < (int16_t)yI->lrp && xI->mmd < distance) {
			/* hit on x-axis */
			nY = xI->ml;
			hit++;
		}
		/* check for y-axis intersection */
		if (yI->ip > (int16_t)xI->ulp && yI->ip < (int16_t)xI->lrp && yI->mmd < distance) {
			/* hit on y-axis */
			nX = yI->ml;
			hit++;
		}

		if (hit == 2)
			break;
	}

	/* set new position */
	*x = nX;
	*y = nY;

	return !!hit;
}

/*
 * draw wall slices without texture because they are too far away
 *
 * maybe use some dithering laster instead of a solid column
 */
void Engine::drawNoTexture(int16_t screenY, uint16_t wallHeight, struct renderInfo *re)
{
	uint8_t bit;
	unsigned char *buffer = es.screenColumn + (uint8_t)screenY / 8;

	bit = bitshift_left[screenY % 8];

	/*
	 * draw wall textures that are less than MIN_WALL_HEIGHT pixels high
	 * TODO optimize this by drawing the pixels at once
	 */
	/* backward counting loops result in less code */
	for (uint8_t yOffset = wallHeight; yOffset > 0; yOffset--) {
		asm volatile (
			/*
			 * buffer[offset    ] |= bit;
			 */
			"ld __tmp_reg__, Z\n"
			"or __tmp_reg__, %[bit]\n"
			"st Z, __tmp_reg__\n"
			/*
			 * bit rol 1, add 1 to buffer address if MSB -> LSB
			 */
			"lsl %[bit]\n"
			"brcc 1f\n"
			"adc %[bit], __zero_reg__\n"
			"adiw %A[buffer], 1\n"
			"1:\n"
			: [bit] "+r" (bit), // output
			  [buffer] "+z" (buffer)
			: //input
			:
		);
	}
}

/*
 * compare two values
 */
uint8_t Engine::compareGreaterOrEqual(uint8_t index1, uint8_t index2)
{
	return !!(index1 >= index2);
}

uint8_t Engine::readThroughCache(uint8_t mapX, uint8_t mapY)
{
	uint8_t cacheRow = mapX % CACHE_ROWS;
	uint8_t cacheColumn = mapY % CACHE_COLUMNS;
	/* calculate cache offset */
	uint16_t cacheOffset = ((uint16_t)cacheRow * (CACHE_COLUMNS * sizeof(cache_entry))) + (cacheColumn * sizeof(cache_entry));
	unsigned char *cache = arduboy->getBuffer();

	struct cache_entry *cEntry = (struct cache_entry *)(cache + cacheOffset);
	if (cEntry->mapX == mapX && cEntry->mapY == mapY) {
		return cEntry->tile;
	}
	/* read map tile */
	uint8_t tile = checkIgnoreBlock(mapX, mapY);

	/*
	 * if tile is a static sprite then put an empty tile in the cache but remember
	 * the static sprite for the render stage
	 */
	if (tile >= SPRITES_START) {
		uint8_t sprite_id = tile & ~SPRITES_START;
		/*
		 * if there is space and the sprite is still active then
		 * add it to the heavyweight sprite lists
		 */
		if (es.nrOfVisibleSprites < MAX_VISIBLE_SPRITES && (es.ld.static_sprites[sprite_id] & 1)) {
			struct heavyweight_sprite *hw_s = &es.hw_sprites[es.nrOfVisibleSprites];
			hw_s->id = tile;
			HWSPRITE_XY_SET(hw_s, mapX * BLOCK_SIZE + 32, mapY * BLOCK_SIZE + 32);
			es.nrOfVisibleSprites++;
		}
		tile = F0;
	}

	/* store in cache */
	cEntry->mapX = mapX;
	cEntry->mapY = mapY;
	cEntry->tile = tile;

	return tile;
}

void Engine::resetSystemEvents(void)
{
	es.systemEvent = EVENT_NONE;
}

void Engine::setSystemEvent(uint8_t event, uint8_t data)
{
	es.systemEvent |= event;

	for (uint8_t bit = 0; bit < 8; bit++) {
		if (event & (1 << bit))
			es.systemEventData[bit] = data;
	}
}

uint8_t Engine::getSystemEventData(uint8_t event)
{
	if (!event)
		return 0;

	for (uint8_t bit = 0; bit < 8; bit++) {
		if (event & (1 << bit))
			return es.systemEventData[bit];
	}
	return 0;
}

uint8_t Engine::getSystemEvents(void)
{
	return es.systemEvent;
}

uint8_t Engine::jumpToLevel(uint8_t level)
{
	currentLevel = level;
	if (currentLevel >= MAX_LEVELS) {
		currentLevel = MAX_LEVELS - 1;
		return 0;
	}
	return 1;
}

uint8_t Engine::nextLevel(void)
{
	return jumpToLevel(currentLevel + 1);
}

uint8_t Engine::setActiveQuest(uint8_t questId)
{
	if (activeQuestId == QUEST_NOT_ACTIVE && questId != activeQuestId) {
		activeQuestId = questId;
	}

	return !!(activeQuestId == questId);
}

uint8_t Engine::isActiveQuestFinished(void)
{
	return !!(questsFinished[activeQuestId / 8] | (1 << (activeQuestId % 8)));
}

void Engine::evaluateActiveQuest(void)
{
	if (activeQuestId == QUEST_NOT_ACTIVE || isActiveQuestFinished())
		return;

	// TODO read quest log from flash and proceed, update questlog entry

	/* finish quest if log entry says finished */
	questsFinished[activeQuestId / 8] |= 1 << (activeQuestId % 8);
}

void Engine::rewardActiveQuest(void)
{
	// TODO read reward section of active quest and apply it

	/* reset quest ID */
	activeQuestId = QUEST_NOT_ACTIVE;
}

void Engine::startAudioEffect(uint8_t id, uint8_t data)
{
	struct audio_effect *ae = &audioEffects[id];

	// TODO check if distance to object is ok?

	ae->trigger = AUDIO_EFFECT_TRIGGER_START;
	ae->data = data & 0x3f;
}

void Engine::stopAudioEffect(uint8_t id)
{
	struct audio_effect *ae = &audioEffects[id];

	ae->trigger = AUDIO_EFFECT_TRIGGER_STOP;
}

/*
 * drawing loop for visible sprites
 */
void Engine::handleSprites(uint16_t rayLength, uint16_t fovLeft, struct renderInfo *re)
{
	bitSet(PORTF, 0);
	re->ystart = 0;
	re->yend = SCREEN_HEIGHT;
	re->yfirst = SCREEN_HEIGHT;
	re->ylast = SCREEN_HEIGHT;

	struct sprite _s;

	for (uint8_t i = 0; i < es.nrOfVisibleSprites; i++) {
		struct heavyweight_sprite *hw_s = &es.hw_sprites[i];
		struct sprite *s;
		if (hw_s->id < SPRITES_START) {
			s = &es.ld.dynamic_sprites[hw_s->id];
		} else {
			s = &_s;
			_s.flags = es.ld.static_sprites[hw_s->id - SPRITES_START];
		}
		/*
		 * this ray might not reach the sprite
		 */
		if (hw_s->distance >= rayLength) {
			continue;
		}

		int16_t diffAngle = rayAngle - hw_s->spriteAngle;
		if (diffAngle < 0)
			diffAngle += 360;

		/* TODO diffAngle > 90, improve this */
		uint16_t angle = diffAngle <= 90 ? 90 - diffAngle: 450 - diffAngle;
		int16_t h = divS24ByBlocksize((int32_t)pgm_cosByX(angle) * (int16_t)hw_s->distance);

		if (abs(h) < SPRITE_WIDTH / 2) {
			int16_t spriteX;

			/******************************************************
			 *
			 * draw texture onto the sprite
			 *
			 */

			/* h = negative: right side */
			spriteX = ((int16_t)SPRITE_WIDTH / 2) + h;

			/*
			 * find x coordinate of the texture, height of sprites is 16 pixel
			 * so multiply by sprite height in bytes (image data is stored columnwise)
			 */
			uint8_t texX = spriteX * SPRITE_HEIGHT_BYTES;

			/*
			 * draw vertical slice of the texture
			 */
			uint24_t p = hw_s->p;

			FX::seekData(p + texX);
			/* load the sprite mask from flash */
			es.texColumn[0] = FX::readPendingUInt8();
			es.texColumn[1] = FX::readPendingUInt8();
			/* load the sprite data from flash */
			es.texColumn[4] = FX::readPendingUInt8();
			es.texColumn[5] = FX::readEnd();

			/* read scale and px_base from flash (+2 to skip the wallHeight field) */
			FX::seekData(rayLengths_flashoffset + 2 + hw_s->distance * 9);
			uint16_t scale = FX::readPendingUInt8();
			scale |= FX::readPendingUInt8() << 8;
			uint8_t px_base = FX::readPendingUInt8();

			uint8_t dh = hw_s->spriteDisplayHeight;

			/* draw the texture column */
			if (scale < TEXTURE_SCALE_UP_LIMIT) {
				drawTextureColumnScaleUp2(hw_s->screenY, dh, px_base, re);
			} else {
				drawAll(hw_s->screenY, scale, dh, re);
			}

			/* deselect cart */
			FX::readEnd();

			/*
			 * shooting will happen in the middle of the screen (64 rays / 2 = 32)
			 *   items and projectiles will be ignored
			 */
			if (es.fireCountdown == 0 && !SPRITE_IS_PROJECTILE(s)) {
				/* TODO weapon effective distance */

				/* if player shoots, enemy will follow and eventually attack */
				SPRITE_STATE_SET(s, ENEMY_FOLLOW);

				/* TODO, maybe just draw some splatter, set frame for hit animation */

				/* do damage based on current weapon */
				uint8_t damage = pgm_read_uint8(&weaponDamage[es.playerActiveWeapon]);

				/* inflict damage to the sprite */
				doDamageToSprite(hw_s->id, damage);
			}
		}
	}
	bitClear(PORTF, 0);
}

extern "C" const uint16_t xlateQuadrantToAngle[] __attribute__ ((aligned (8))) = {
  90, 90, 270, 270,
};

void Engine::updateSprites(int16_t screenYStart, uint16_t fovLeft, uint16_t maxRayLength)
{
	/*
	 * clip maxRayLength if bigger than the biggest distance possible,
	 * this distance is calculated so the min display height is 6 pixels,
	 * anything below is considered too far away
	 */
	if (maxRayLength > (SPRITE_HEIGHT * DIST_TO_PROJECTION_PLANE / 6))
		maxRayLength = (SPRITE_HEIGHT * DIST_TO_PROJECTION_PLANE / 6);


	/* rightmost angle of the players current field of view */
	int16_t fovRight = fovLeft + FIELD_OF_VIEW;
	if (fovRight >= 360)
		fovRight -= 360;

	struct heavyweight_sprite *hw_s = &es.hw_sprites[0];
	uint8_t invisibleSprites = 0;
	/*
	 * Loop through the actual visible sprites and calculate distance and other
	 * parameters. The sprites that are now in the list are static sprites that
	 * have been found during the raycasting.
	 */
	for (uint8_t i = 0; i < es.nrOfVisibleSprites; i++) {
		uint16_t x, y;

		/*
		 * get sprite x/y position from p
		 * (was stored there during the raycast)
		 */
		x = hw_s->p & 0xfff;
		y = hw_s->p >> 12;

		uint16_t dX, dY;
		int16_t spriteAngle;
		uint8_t quadrant;

		if (es.ld.playerX < x) {
			/* dX would be positive */
			dX = x - es.ld.playerX;
			quadrant = 1;
		} else {
			/* dX would be negative */
			dX = es.ld.playerX - x;
			quadrant = 2;
		}

		if (es.ld.playerY < y) {
			/* dY would be positive */
			dY = y - es.ld.playerY;
			quadrant += 0;
		} else {
			/* dY would be negative */
			dY = es.ld.playerY - y;
			quadrant += 2;
		}

		uint16_t distance = 0xffff;
		uint8_t qAngle;
		int16_t minAngle;

		/*
		 * do not read the precalculated distance from flash if it is
		 * expected to be too big
		 */
		if (dX < SPRITE_MAX_VDISTANCE && dY < SPRITE_MAX_VDISTANCE) {
			FX::seekData(distances_flashoffset + (SPRITE_MAX_VDISTANCE * 4 * (uint24_t)dX) + (4 * dY));
			distance = FX::readPendingUInt8();
			distance |= FX::readPendingUInt8() << 8;
			minAngle = FX::readPendingUInt8();
			qAngle = FX::readEnd();
		}

		if (distance > maxRayLength) {
			/*
			 * remove the sprite from hw_s list as it is too
			 * far away to be displayed
			 */
			invisibleSprites++;
			memcpy(hw_s, hw_s + 1, sizeof(hw_s) * (es.nrOfVisibleSprites - i));
			continue;
		}

		spriteAngle = xlateQuadrantToAngle[quadrant - 1];
		if (quadrant == 1 || quadrant == 4)
			spriteAngle -= qAngle;
		else
			spriteAngle += qAngle;

		hw_s->distance = distance;
		hw_s->spriteAngle = spriteAngle;

		uint8_t sprite_id = hw_s->id & ~0x80;
		uint8_t flags = es.ld.static_sprites[sprite_id];
		uint8_t type = (flags >> 1) & 0xf;

		if (distance < 48) { //itemCollectableDistance:
			/*
			 * for collectable items make the screen blink, everything
			 * else is just for decoration
			 */
			if (type < V_OF_S(I_TYPE14)) {
				es.blinkScreen = 1;
				setStatusMessage(type - V_OF_S(STATUS_MSG_OFFSET));
				/*
				 * remove from sprite list, not very clever as
				 * we always need to loop over all the sprites
				 */
				invisibleSprites++;
				memcpy(hw_s, hw_s + 1, sizeof(hw_s) * (es.nrOfVisibleSprites - i));

				/* clear active flag */
				es.ld.static_sprites[sprite_id] &= ~1;
			}

			uint8_t playerWeapons = es.playerWeapons;

			if (type == V_OF_S(I_TYPE4)) {
				/* increase health */
				es.playerHealth += 12;
				if (es.playerHealth > 100)
					es.playerHealth = 100;
			} else if (type == V_OF_S(I_TYPE5)) {
				/*  add ammo */
				/* start at 1 because weapon 0 is always set */
				for (uint8_t w = 1; w < NR_OF_WEAPONS; w++) {
					uint8_t ammo = es.playerAmmo[w];
					if (playerWeapons | bitshift_left[w])
						ammo += 11 - ((w - 1) * 5);

					if (ammo > 99)
						ammo = 99;

					es.playerAmmo[w] = ammo;
				}
			} else if (type == V_OF_S(I_TYPE0)) {
				/* increase number of keys */
				es.playerKeys++;
			} else if (type == V_OF_S(I_TYPE1)) {
				/* add weapon */
				playerWeapons |= HAS_WEAPON_2;
			} else if (type == V_OF_S(I_TYPE2)) {
				/* add weapon */
				playerWeapons |= HAS_WEAPON_3;
			} else if (type == V_OF_S(I_TYPE3)) {
				/* add weapon */
				playerWeapons |= HAS_WEAPON_4;
			}
			es.playerWeapons = playerWeapons;
		}

		/*
		 * save the pointer into flash for convenience
		 */
		hw_s->p = itemsSpriteData_flashoffset + (type * spriteDataAlignment);

		hw_s++;
	}

	es.nrOfVisibleSprites -= invisibleSprites;

	struct sprite *s = &es.ld.dynamic_sprites[0];
	/* current sprite pointer at the end of the first half of the framebuffer memory */
	struct current_sprite *cs = (struct current_sprite *)(arduboy->getBuffer() + 512 - sizeof(struct current_sprite));

	/*
	 * loop through dynamic sprites and projectiles from special walls
	 */
	for (uint8_t i = 0; i < (es.ld.nr_of_sprites + es.ld.maxSpecialWalls); i++, s++) {
		/* stop if the maximum number of visible sprites has been reached */
		if (es.nrOfVisibleSprites == MAX_VISIBLE_SPRITES)
			break;

		/* sprite is not active */
		if (IS_INACTIVE(s->flags))
			continue;

		/* calculate new distance */
		/* TODO do this only if the sprite is inside a bounding box
		 *      around the player
		 *      - bounding box around a circle with radius
		 *        (SPRITE_HEIGHT * DIST_TO_PROJECTION_PLANE / 6) = max visible distance
		 */

		uint16_t dX, dY;
		int16_t spriteAngle;
		uint8_t quadrant;

		/*
		 * load current sprite values
		 */
		cs->x = s->xy & 0xfff;
		cs->y = s->xy >> 12;
		cs->flags = s->flags;
		cs->viewAngle = (uint16_t)SPRITE_VIEWANGLE_GET(s) * 90;
		cs->id = i;

		/*
		 * calculate in which quadrant the sprite is in, relative to the
		 * player
		 *
		 * dY negative:     3  (Q4)
		 * both negative:   4  (Q3)
		 * dX negative:     2  (Q2)
		 * both positive:   1  (Q1)
		 */
		if (es.ld.playerX < cs->x) {
			/* dX would be positive */
			dX = cs->x - es.ld.playerX;
			quadrant = 1;
		} else {
			/* dX would be negative */
			dX = es.ld.playerX - cs->x;
			quadrant = 2;
		}

		if (es.ld.playerY < cs->y) {
			/* dY would be positive */
			dY = cs->y - es.ld.playerY;
			quadrant += 0;
		} else {
			/* dY would be negative */
			dY = es.ld.playerY - cs->y;
			quadrant += 2;
		}

		cs->distance = 0xffff;
		uint8_t qAngle;
		int16_t minAngle;

		if (dX < SPRITE_MAX_VDISTANCE && dY < SPRITE_MAX_VDISTANCE) {
			FX::seekData(distances_flashoffset + (SPRITE_MAX_VDISTANCE * 4 * (uint24_t)dX) + (4 * dY));
			cs->distance = FX::readPendingUInt8();
			cs->distance |= FX::readPendingUInt8() << 8;
			minAngle = FX::readPendingUInt8();
			qAngle = FX::readEnd();
		}

		/*
		 * nothing to do if sprite is too far away
		 */
		if ((cs->distance > maxRayLength) || (cs->distance == 0)) {
			continue;
		}

		spriteAngle = xlateQuadrantToAngle[quadrant - 1];
		if (quadrant == 1 || quadrant == 4)
			spriteAngle -= qAngle;
		else
			spriteAngle += qAngle;

		/*
		 * extend left border of field of view by the
		 * min angle required to see the sprite
		 */
		int16_t spriteL = fovLeft - minAngle;
		if (spriteL < 0)
			spriteL += 360;
		/*
		 * extend right border of field of view by the
		 * min angle required to see the sprite
		 */
		int16_t spriteR = fovRight + minAngle;
		if (spriteR >= 360)
			spriteR -= 360;

		/*
		 * If the right boundary of the FOV is less than the left
		 * then we add 360 degrees to the right for the boundary check.
		 * Same we do for the spriteAngle if it is less than the left
		 * and less than the right boundary.
		 */
		if (spriteR < spriteL) {
			if (spriteAngle < spriteL && spriteAngle < spriteR)
				spriteAngle += 360;
			spriteR += 360;
		}

		uint8_t visible = (spriteAngle >= spriteL && spriteAngle <= spriteR);

		/* fix spriteAngle due to boundary checks */
		if (spriteAngle >= 360)
			spriteAngle -= 360;

		if (visible) {
			/* add to visible sprite list */
			hw_s->id = i;
			hw_s->distance = cs->distance;
			hw_s->spriteAngle = spriteAngle;
			/*
			 * save the pointer into flash for convenience
			 */
			if (SPRITE_IS_PROJECTILE(cs))
				hw_s->p = projectilesSpriteData_flashoffset + (SPRITE_TYPE_GET(s) * spriteDataAlignment);
			else
				hw_s->p = enemySpriteData_flashoffset + (SPRITE_TYPE_GET(s) * spriteDataAlignment);
			hw_s++;
			es.nrOfVisibleSprites++;
		}

		/* store current sprite angle */
		cs->spriteAngle = spriteAngle;

		/*
		 * the sprite is a projectile if it is not a regular non static
		 * sprite
		 */
		uint8_t new_state = SPRITE_STATE_GET(cs);
		if (SPRITE_IS_PROJECTILE(cs)) {
			if (cs->distance < 16) {
				/* inflict damage on the player if close enough */
#ifndef CONFIG_GOD_MODE
				es.playerHealth--;
				es.blinkScreen = 1;
#endif

				if (es.playerHealth == 0)
					es.killedBySprite = 0;

				/* set inactive */
				cs->flags |= S_INACTIVE;
			} else {
				uint8_t speed = pgm_read_uint8(&projectileMovementSpeeds[SPRITE_TYPE_GET(cs)]);
				if (moveSprite(cs, speed)) {
					/* sprite hit some object, mark it inactive */
					cs->flags |= S_INACTIVE;
				}
			}
		} else {
			/* set enemies movement speed */
			uint8_t speed = pgm_read_uint8(&enemyMovementSpeeds[SPRITE_TYPE_GET(s)]);

			switch (new_state) {
			case ENEMY_IDLE:
				new_state = enemyStateIdle(cs, speed);
				break;
			case ENEMY_ATTACK:
				new_state = enemyStateAttack(cs, speed);
				break;
			case ENEMY_FOLLOW:
				new_state = enemyStateFollow(cs, speed);
				break;
			case ENEMY_RANDOM_MOVE:
				new_state = enemyStateRandomMove(cs, speed);
				break;
			}
			SPRITE_STATE_SET(cs, new_state);
		}
		/* save updated values */
		s->flags = cs->flags;
		uint8_t viewAngle = cs->viewAngle / 90;
		SPRITE_VIEWANGLE_SET(s, viewAngle);
		SPRITE_XY_SET(s, cs->x, cs->y);
	}

	/*
	 * sort visible sprites in ascending order
	 */
	uint8_t pos = 0;
	hw_s = &es.hw_sprites[0];
	while (pos < es.nrOfVisibleSprites) {
		if (pos == 0 || (hw_s[pos].distance <= hw_s[pos - 1].distance)) {
			pos++;
		} else {
			struct heavyweight_sprite tmp;

			/* swap entries */
			memcpy(&tmp, &hw_s[pos], sizeof(struct heavyweight_sprite));
			memcpy(&hw_s[pos], &hw_s[pos - 1], sizeof(struct heavyweight_sprite));
			memcpy(&hw_s[pos - 1], &tmp, sizeof(struct heavyweight_sprite));

			pos--;
		}
	}

	/*
	 * go through all visible sprites and precalc some rendering attributes and
	 * assign hwid
	 */
	struct sprite _s;
	for (uint8_t i = 0; i < es.nrOfVisibleSprites; i++) {
		struct heavyweight_sprite *hw_s = &es.hw_sprites[i];
		struct sprite *s;
		if (hw_s->id < SPRITES_START) {
			s = &es.ld.dynamic_sprites[hw_s->id];
		} else {
			s = &_s;
			_s.flags = es.ld.static_sprites[hw_s->id - SPRITES_START];
		}

		/* add offset to skip mask */
		hw_s->p += SPRITE_SIZE;

		/*
		 * calculate the side the player is looking at the sprite
		 *   only do this if sprite is visible and not a simple one (e.g. item)
		 */
		if (!SPRITE_IS_PROJECTILE(s)) {
			/*
			 * calculate the angle the player is looking at the sprite
			 */
			/*
			 *
			 * angles the player will have to look at the
			 * sprite in order to see the different sides
			 *
			 * 225 - (255 + 90) = front
			 * 315 - (315 + 90) = right
			 *  45 - ( 45 + 90) = back
			 * 135 - (135 + 90) = left
			 *
			 * TODO could start with 45 and add 90 each turn
			 *      that way we do not need an array but side
			 *      value might get complicated
			 * This is kind of bloated, maybe rethink it
			 */
			/* front right back left */
			const uint16_t llimits[] = {225, 315, 45, 135};

			uint8_t side;
			for (side = 0; side < 4; side++) {
				uint16_t l, r;
				l = llimits[side];
				uint16_t viewAngle = SPRITE_VIEWANGLE_GET(s) * 90;
				l += viewAngle;
				r = l + 90;
				if (l >= 360)
					l -= 360;
				if (r >= 360)
					r -= 360;
				if ((uint16_t)hw_s->spriteAngle >= l && (uint16_t)hw_s->spriteAngle < r) {
					/* add offset for the correct side */
					hw_s->p += SPRITE_SIZE * side;
					break;
				}
			}
		}

		/*
		 * precalculate some values to speed up rendering loop
		 */
		int16_t vSpriteMove = 0; // TODO (int16_t)s->vMove;

		/*
		 * e.g. spriteheight is 32, so shift the sprite 16 pixel to the bottom
		 * calculate sprite vertical move
		 */
		// TODO this is for static sprites
		//if (s->type >= V_OF_S(I_TYPE0))
		//	vSpriteMove += es.itemDance;

		vSpriteMove = vSpriteMove * (int16_t)DIST_TO_PROJECTION_PLANE / (int16_t)hw_s->distance;

		/*
		 * read height, scale value from flash
		 */
		FX::seekData(rayLengths_flashoffset + hw_s->distance * 9);
		hw_s->spriteDisplayHeight = FX::readPendingUInt8();
		hw_s->spriteDisplayHeight |= FX::readPendingUInt8() << 8;
		uint16_t scale = FX::readPendingUInt8();
		scale |= FX::readEnd() << 8;

		/* sprites are half the size of a regular sprite */
		hw_s->spriteDisplayHeight /= 2;

		/*
		 * if spriteheight is bigger than screenheight we need to clip it
		 * to screenheight and add the offset into the texture to texY
		 *
		 * take vSpriteMove into account otherwise sprite will be clipped
		 * when sprite is outside of visible area
		 *
		 */
		hw_s->screenY = (uint16_t)((int16_t)(SCREEN_HEIGHT - hw_s->spriteDisplayHeight) / 2 + vSpriteMove) + screenYStart;

		/*
		 * if the displayheight plus the screen y offset exceeds the
		 * screenheight then we need to clip it
		 */
		int16_t screenY = hw_s->screenY;
		if (screenY < 0) {
			hw_s->spriteDisplayHeight += screenY;
			screenY = 0;
		}
		if ((hw_s->spriteDisplayHeight + screenY) > SCREEN_HEIGHT) {
			hw_s->spriteDisplayHeight = SCREEN_HEIGHT - screenY;
		}
	}
}

/*
 * main render loop
 */
void Engine::render(void)
{
#if defined(CONFIG_FPS_MEASUREMENT)
	unsigned long render_calc_start, render_calc;
	unsigned long render_draw_start, render_draw;
	unsigned long total_start, total = 0;
	unsigned long drawing_start, drawing = 0;
#endif
#if defined(CONFIG_FPS_MEASUREMENT)
	total_start = millis();
#endif

	/* check if we need to do damage to the player */
	if ((es.frame % PLAYERS_DAMAGE_TIMEOUT) == 0) {
		if (es.doDamageFlags & 1) {
			/* wall damage */
#ifndef CONFIG_GOD_MODE
			es.blinkScreen = 1;
			es.playerHealth -= 10;
#endif
		}
		es.doDamageFlags = 0;
	}
	/* player has been hit, indicate that with a white frame (or maybe led?) */
	/* IDEA: pulse led when low on health? */
	if (es.blinkScreen) {
		es.blinkScreen = 0;

		/* check if player is dead */
		if (es.playerHealth <= 0) {
			/* dead */
			es.playerHealth = 0;
		} else {
			/* fill screen with white pixels */
			arduboy->fillScreen(WHITE);
			return;
		}
	}

	/* calculate screen Y start depending on the head position */
	int16_t screenYStart = 0; //(int16_t)((es.vHeadPosition >> 4) + vMove);

	/* leftmost angle of the players current field of view */
	uint16_t playerFOVLeftAngle = 360 + es.ld.playerAngle - FIELD_OF_VIEW / 2;
	if (playerFOVLeftAngle >= 360)
		playerFOVLeftAngle -= 360;

	/* move the player, list of blocks to ignore must be clean before this call */
	move(es.ld.playerAngle, es.direction, &es.ld.playerX, &es.ld.playerY, PLAYERS_SPEED);

	/*
	 * update moveable objects, do this after moving the player so
	 * in case a moveable object moves inside the minDistance area
	 * the players position can be corrected
	 */
	updateMoveables();

	/*
	 * divide player x and y coordinate by the blocksize to get the
	 * map x and y coordinate
	 */
	es.ld.playerMapX = divU16ByBlocksize(es.ld.playerX);
	es.ld.playerMapY = divU16ByBlocksize(es.ld.playerY);

	/*
	 * cast rays
	 */
	rayAngle = playerFOVLeftAngle;

#if defined(CONFIG_FPS_MEASUREMENT)
	render_calc_start = millis();
#endif
	/* set rayinfo pointer and skip the first 64 columns as they are used for the map cache */
	struct rayinfo *ri = (struct rayinfo *)(arduboy->getBuffer() + (512));

	uint16_t maxRayLength = 0;

	for (uint8_t ray = 0; ray < FIELD_OF_VIEW; ray++) {
		/*
		 *       1
		 *     +---+
		 *   0 |   | 2
		 *     +---+
		 *       3
		 *
		 *                                   h v
		 * Q1: can hit sides 0 (v), 1 (h) -> 1,0
		 * Q2: can hit sides 1 (h), 2 (v) -> 1,2
		 * Q3: can hit sides 2 (v), 3 (h) -> 3,2
		 * Q4: can hit sides 3 (h), 0 (v) -> 3,0
		 *
		 * 1   2   3   4
		 * 1,0,1,2,3,2,3,0
		 */

		uint16_t hX = es.ld.playerX;
		int16_t hStepX = 0;
		int8_t hStepY;
		uint16_t vY = es.ld.playerY;
		int16_t vStepY = 0;
		int8_t vStepX;
		uint8_t blockSideIndex;
		uint8_t hTextureOrientation;
		uint8_t vTextureOrientation;

		/*
		 * handle every quadrant individually and calculate first horizontal/vertical
		 * intersection
		 */
		if (rayAngle < 90) {
			es.tempRayAngle = rayAngle;
			es.nTempRayAngle = 90 - es.tempRayAngle;
			/*
			 * 0 .. 89 degree
			 * calculate values for horizontal intersection test
			 */
			blockSideIndex = 0;

			/* view direction down/right */
			es.viewDirection = VD_DOWN_RIGHT;

			hStepY = 1;
			if (es.tempRayAngle != 0) {
				/* result u16 inside () all u8 */
				uint32_t dY = (uint32_t)(BLOCK_SIZE - (es.ld.playerY & (BLOCK_SIZE - 1))) * pgm_tanByX(es.nTempRayAngle);
				hX += divU24ByBlocksize(dY);
				hStepX = (int32_t)pgm_tanByX(es.nTempRayAngle) * BLOCK_SIZE / BYX;
			}

			hTextureOrientation = TEXTURE_RIGHT_TO_LEFT;

			/*
			 * calculate values for vertical intersection test
			 */
			vStepX = 1;
			if (es.tempRayAngle != 0) {
				/* result u16 inside () all u8 */
				uint32_t dX = (uint32_t)(BLOCK_SIZE - (es.ld.playerX & (BLOCK_SIZE - 1))) * pgm_tanByX(es.tempRayAngle);
				vY += divU24ByBlocksize(dX);
				vStepY = (int32_t)pgm_tanByX(es.tempRayAngle) * BLOCK_SIZE / BYX;
			}

			vTextureOrientation = TEXTURE_LEFT_TO_RIGHT;

		} else if (rayAngle < 180) {
			es.tempRayAngle = 180 - rayAngle;
			es.nTempRayAngle = 90 - es.tempRayAngle;
			/*
			 * 90 .. 179 degree
			 * calculate values for horizontal intersection test
			 */
			blockSideIndex = 2;

			/* view direction down/left */
			es.viewDirection = VD_DOWN_LEFT;

			hStepY = 1;

			if (es.tempRayAngle != 90) {
				/* result u16 inside () all u8 */
				uint32_t dY = (uint32_t)(BLOCK_SIZE - (es.ld.playerY & (BLOCK_SIZE - 1))) * pgm_tanByX(es.nTempRayAngle);
				hX -= divU24ByBlocksize(dY);
				hStepX = (int32_t)-pgm_tanByX(es.nTempRayAngle) * BLOCK_SIZE / BYX;
			}

			hTextureOrientation = TEXTURE_RIGHT_TO_LEFT;

			/*
			 * calculate values for vertical intersection test
			 */
			vStepX = -1;

			if (es.tempRayAngle != 90) {
				/* result u16 inside () all u8 */
				uint32_t dX = (uint32_t)((es.ld.playerX & (BLOCK_SIZE - 1)) + 1) * pgm_tanByX(es.tempRayAngle);
				vY += divU24ByBlocksize(dX);
				vStepY = (int32_t)pgm_tanByX(es.tempRayAngle) * BLOCK_SIZE / BYX;
			}

			vTextureOrientation = TEXTURE_RIGHT_TO_LEFT;

		} else if (rayAngle < 270) {
			es.tempRayAngle = rayAngle - 180;
			es.nTempRayAngle = 90 - es.tempRayAngle;
			/*
			 * 180 .. 269 degree
			 * calculate values for horizontal intersection test
			 */
			blockSideIndex = 4;

			/* view direction down/left */
			es.viewDirection = VD_UP_LEFT;

			hStepY = -1;

			if (es.tempRayAngle != 0) {
				/* result u16 inside () all u8 */
				uint32_t dY = (uint32_t)((es.ld.playerY & (BLOCK_SIZE - 1)) + 1) * pgm_tanByX(es.nTempRayAngle);
				hX -= divU24ByBlocksize(dY);
				hStepX = (int32_t)-pgm_tanByX(es.nTempRayAngle) * BLOCK_SIZE / BYX;
			}

			hTextureOrientation = TEXTURE_LEFT_TO_RIGHT;

			/*
			 * calculate values for vertical intersection test
			 */
			vStepX = -1;

			if (es.tempRayAngle != 0) {
				/* result u16 inside () all u8 */
				uint32_t dX = (uint32_t)((es.ld.playerX & (BLOCK_SIZE - 1)) + 1) * pgm_tanByX(es.tempRayAngle);
				vY -= divU24ByBlocksize(dX);
				vStepY = (int32_t)-pgm_tanByX(es.tempRayAngle) * BLOCK_SIZE / BYX;
			}

			vTextureOrientation = TEXTURE_RIGHT_TO_LEFT;

		} else {
			es.tempRayAngle = 360 - rayAngle;
			es.nTempRayAngle = 90 - es.tempRayAngle;
			/*
			 * 270 .. 359 degree
			 * calculate values for horizontal intersection test
			 */
			blockSideIndex = 6;

			/* view direction down/left */
			es.viewDirection = VD_UP_RIGHT;

			hStepY = -1;

			if (es.tempRayAngle != 90) {
				/* result u16 inside () all u8 */
				uint32_t dY = (uint32_t)((es.ld.playerY & (BLOCK_SIZE - 1)) + 1) * pgm_tanByX(es.nTempRayAngle);
				hX += divU24ByBlocksize(dY);
				hStepX = (int32_t)pgm_tanByX(es.nTempRayAngle) * BLOCK_SIZE / BYX;
			}

			hTextureOrientation = TEXTURE_LEFT_TO_RIGHT;

			/*
			 * calculate values for vertical intersection test
			 */
			vStepX = 1;

			if (es.tempRayAngle != 90) {
				/* result u16 inside () all u8 */
				uint32_t dX = (uint32_t)(BLOCK_SIZE - (es.ld.playerX & (BLOCK_SIZE - 1))) * pgm_tanByX(es.tempRayAngle);
				vY -= divU24ByBlocksize(dX);
				// IDEA increase accuracy by generating a table with *BLOCK_SIZE
				vStepY = (int32_t)-pgm_tanByX(es.tempRayAngle) * BLOCK_SIZE / BYX;
			}

			vTextureOrientation = TEXTURE_LEFT_TO_RIGHT;

		}

		/*
		 * cast ray through the map, handle doors
		 */
		uint8_t blockSideInc;
		uint32_t rayLength;
		uint8_t tile;
		int8_t wallX = 0, hWallX = 0, vWallX = 0;
		uint16_t cosAngle;
		uint16_t cosAngle2;
		uint8_t textureOrientation;
		uint8_t hBlockY = es.ld.playerMapY + hStepY;
		uint8_t vBlockX = es.ld.playerMapX + vStepX;


		cosAngle = pgm_fineCosByX(es.nTempRayAngle);
		cosAngle2 = pgm_fineCosByX(es.tempRayAngle);

		for (uint8_t run = 0; ; run++) {

			uint16_t hRayLength;
			uint16_t hRenderRayLength;
			uint16_t vRayLength;
			uint16_t vRenderRayLength;

			/*
			 * search horizontal intersections until a wall is hit
			 */
			uint8_t hTile = F0;

			es.cd = NULL;
			es.cmw = NULL;
			es.ct = NULL;


			if ((rayAngle != 0) && (rayAngle != 180)) {

				uint16_t hY;
				uint8_t hops = 0;
				uint8_t hBlockX;

				hBlockX = divU16ByBlocksize(hX);

				/* reset render raylength and wallx */
				es.renderRayLength = -1;
				es.wallX = -1;

				/*
				 * Check for corner case if player is inside a block that
				 * is not aligned to a BLOCK_SIZE boundary. This is the
				 * case for e.g. moving walls which needs a dedicated renderer
				 * when the player is close to them.
				 */
				hTile = checkIgnoreBlockInnerHorizontal(es.ld.playerMapX, es.ld.playerMapY, run);
				if (hTile != F0) {
					hRayLength = 1;
					goto horizontal_intersection_done2;
				}

				hops = pgm_read_uint8(&hopsPerAngle[es.nTempRayAngle]);
				while (((hBlockX | hBlockY) & 0xc0) == 0 && hops--) {
					hTile = checkIgnoreBlockFast(hBlockX, hBlockY);
					if (hTile) {
						if (hTile == V_M_W) {
							hY = hBlockY * BLOCK_SIZE;
							if (hStepY < 0)
								hY += BLOCK_SIZE - 1;
							hTile = checkIfMovingWallHit(hBlockX, hBlockY, hX, hY);
							if (hTile)
								break;
						} else {
							break;
						}
					}

					/* go to next horizontal intersection */
					hBlockY += hStepY;
					hX += hStepX;
					hBlockX = divU16ByBlocksize(hX);
				}

				hY = hBlockY * BLOCK_SIZE;
				if (hStepY < 0)
					hY += BLOCK_SIZE - 1;

				/*
				 * INFO: theoretically we should check for hTile != 0 but
				 * practically all maps have a wall border
				 */
				uint16_t diffX;
				if (es.ld.playerX < hX)
					diffX = hX - es.ld.playerX;
				else
					diffX = es.ld.playerX - hX;

				uint16_t diffY;
				if (es.ld.playerY < hY)
					diffY = hY - es.ld.playerY;
				else
					diffY = es.ld.playerY - hY;

				/* use fine cosinus table to increase precision */
				hRayLength  = ((uint32_t)diffY * cosAngle + (uint32_t)diffX * cosAngle2 + (FINE_BY_X / 2)) / FINE_BY_X;

horizontal_intersection_done2:
				/* take whatever was calculated (or not) from the block functions */
				hWallX = es.wallX;

				if (hTile & HALF_BLOCKS_START) {
					/*
					 * horizontal door or half block
					 *   add half step to x,y position
					 */
					uint16_t hStepRayLength = pgm_read_uint16(&stepRaylength[es.nTempRayAngle]);
					if (hTile == H_DOOR)
						hRenderRayLength = hRayLength + hStepRayLength / DOOR_STEP_DIV_BY;
					else
						hRenderRayLength = hRayLength + hStepRayLength / HALF_BLOCK_STEP_DIV_BY;
				} else {
					/*
					 * some checkXXX functions set the renderRayLength (e.g. for
					 * moving walls). In this case copy the value. In all other cases
					 * set it to the same value as the real ray length
					 */
					if (es.renderRayLength != -1)
						hRenderRayLength = es.renderRayLength;
					else
						hRenderRayLength = hRayLength;
				}
				/* remember the moving wall that was hit horizontally */
				es.hcmw = es.cmw;
				es.hcd = es.cd;
				es.hct = es.ct;
			}

			/*
			 * search vertical intersections until a wall is hit
			 */
			uint8_t vTile = F0;

			if ((rayAngle != 90) && (rayAngle != 270)) {

				uint8_t vBlockY;
				uint16_t vX;
				uint8_t hops = 0;

				/*
				 * Check for corner case if player is inside a block that
				 * is not aligned to a BLOCK_SIZE boundary. This is the
				 * case for e.g. moving walls which needs a dedicated renderer
				 * when the player is close to them.
				 */
				vTile = checkIgnoreBlockInnerVertical(es.ld.playerMapX, es.ld.playerMapY, run);
				if (vTile != F0) {
					vRayLength = 1;
					goto vertical_intersection_done2;
				}

				vBlockY = divU16ByBlocksize(vY);

				hops = pgm_read_uint8(&hopsPerAngle[es.tempRayAngle]);
				es.renderRayLength = -1;
				es.wallX = -1;

				while (((vBlockY | vBlockX) & 0xc0) == 0 && hops--) {
					/* TODO can safely pass 0 for vX because this is only required
					 * for horizontal hits, maybe think about which parameters are
					 * really needed!
					 */
					vTile = checkIgnoreBlockFast(vBlockX, vBlockY);
					if (vTile) {
						if (vTile == V_M_W) {
							vX = vBlockX * BLOCK_SIZE;
							if (vStepX < 0)
								vX += BLOCK_SIZE - 1;
							vTile = checkIfMovingWallHit(vBlockX, vBlockY, vX, vY);
							if (vTile)
								break;
						} else {
							break;
						}
					}

					vBlockX += vStepX;
					vY += vStepY;
					vBlockY = divU16ByBlocksize(vY);
				}
				/*
				 * INFO: theoretically we should check for vTile != 0 but
				 * practically all maps have a wall as border
				 */
				vX = vBlockX * BLOCK_SIZE;
				if (vStepX < 0)
					vX += BLOCK_SIZE - 1;

				uint16_t diffX;
				if (es.ld.playerX < vX)
					diffX = vX - es.ld.playerX;
				else
					diffX = es.ld.playerX - vX;

				uint16_t diffY;
				if (es.ld.playerY < vY)
					diffY = vY - es.ld.playerY;
				else
					diffY = es.ld.playerY - vY;

				/* use fine cosinus table to increase precision */
				vRayLength  = ((uint32_t)diffY * cosAngle + (uint32_t)diffX * cosAngle2 + (FINE_BY_X / 2)) / FINE_BY_X;

vertical_intersection_done2:
				/* take whatever was calculated (or not) from the block functions */
				vWallX = es.wallX;


				if (vTile & HALF_BLOCKS_START) {
					/*
					 * vertical door or half block
					 *   add half step to x,y position
					 */
					uint16_t vStepRayLength = pgm_read_uint16(&stepRaylength[es.tempRayAngle]);
					if (vTile == H_DOOR)
						vRenderRayLength = vRayLength + vStepRayLength / DOOR_STEP_DIV_BY;
					else
						vRenderRayLength = vRayLength + vStepRayLength / HALF_BLOCK_STEP_DIV_BY;
				} else {
					/*
					 * some checkXXX functions set the renderRayLength (e.g. for
					 * moving walls). In this case copy the value. In all other cases
					 * set it to the same value as the real ray length
					 */
					if (es.renderRayLength != -1)
						vRenderRayLength = es.renderRayLength;
					else
						vRenderRayLength = vRayLength;
				}
				/* remember the moving wall that was hit vertically */
				es.vcmw = es.cmw;
				es.vcd = es.cd;
				es.vct = es.ct;
			}

			/*
			 * If horizontal rendering ray is shorter than the
			 * vertical rendering ray, then pretend the vertical
			 * ray is very long. This causes the the engine to
			 * render the wall on the horizontal hit which is in
			 * case of doors the texture that is making up the
			 * door frame.
			 */
			if (hRenderRayLength < vRenderRayLength) {
				vRayLength = 0xffff;
			}
			/*
			 * same like above but for vertical rendering frames
			 */
			if (vRenderRayLength < hRenderRayLength) {
				hRayLength = 0xffff;
			}

			/*
			 * take shortest ray
			 * let only the tile (h/v) of the shortest ray survive
			 */

			/*
			 * if both rays are of equal length then prefer the vertical
			 * ray otherwise moving walls are not rendered correctly
			 */
			if (hRayLength < vRayLength) {
				blockSideInc = 0;
				rayLength = hRenderRayLength;
				textureOrientation = hTextureOrientation;

				tile = hTile;

				if (tile >= V_M_W && tile <= H_M_W_V) {
					wallX = hWallX;
					/*
					 * fix texture orientation and block side for vertically hit
					 * horizontally moving walls
					 */
					if (tile == H_M_W_V) {
						blockSideInc = 1;
						textureOrientation = vTextureOrientation;
					}
					tile = V_M_W; /* set to generic tile */
				} else
					wallX = hX % BLOCK_SIZE;

				es.cmw = es.hcmw;
				es.cd = es.hcd;
				es.ct = es.hct;
			} else {
				blockSideInc = 1;
				rayLength = vRenderRayLength;
				textureOrientation = vTextureOrientation;
				tile = vTile;

				if (tile >= V_M_W && tile <= H_M_W_V) {
					wallX = vWallX;
					/*
					 * fix texture orientation and block side for horizontally hit
					 * vertically moving walls
					 */
					if (tile == V_M_W_H) {
						blockSideInc = 0;
						textureOrientation = hTextureOrientation;
					}
					tile = V_M_W; /* set to generic tile */
				} else
					wallX = vY % BLOCK_SIZE;

				es.cmw = es.vcmw;
				es.cd = es.vcd;
				es.ct = es.vct;

				/* mark bit 7 to indicate we hit a vertical tile */
				wallX |= 0x80;
			}

			if (rayLength > FOG_OF_WAR_DISTANCE) {
				rayLength = FOG_OF_WAR_DISTANCE;
				tile = FOG;
				wallX = 0;
				break;
			}

			/*
			 * calculate wallX for doors
			 */
			int16_t diff;
			uint8_t rayDiff;

			/* check if this is an half block (e.g. door or a style element) */
			if (tile & HALF_BLOCKS_START) {

				if ((wallX & 0x80) == 0) {
					/*
					 * horizontal hit
					 */
					/*
					 *                  +-----------------------+  ---
					 *                  |                       |
					 *                  |                       |
					 *                  |                      /|   B
					 *                  |                     / |   l
					 *                  |   (real wallX)     /  |   o
					 * (door texture)   +-------------------*---+   c
					 *                  |                  /|   |   k
					 *                  |                 / |   |
					 *                  |      (rayDiff) /  |   |   1
					 *                  |               /   |   |
					 *                  |              /    |   |
					 *                  +-------------*=====+---+  ---
					 *                  |   (wallX)  /  diff    |
					 *                  |           /           |
					 *                  |          /            |   B
					 *                  |         /             |   l
					 *                  |        /              |   o
					 *                  |       /               |   c
					 *                  |      /                |   k
					 *                  |     /                 |
					 *                  |  P x                  |   2
					 *                  |                       |
					 *                  |                       |
					 *                  +--+--------------------+  ---
					 *
					 * - from (x) to first (*) = ray length
					 * - from (x) to second (*) = render ray length
					 * - from left (+) to first (*) on the right = wallX based on the ray
					 *
					 */
					rayDiff = hRenderRayLength - hRayLength;
					diff = ((uint32_t)cosAngle2 * rayDiff) / FINE_BY_X;
					if ((rayAngle >= 270) || (rayAngle < 90)) {
						wallX += diff;
					} else {
						wallX -= diff;
					}
				} else {
					/*
					 * vertical hit
					 */
					/* clear vertical hit indicator bit */
					wallX &= ~0x80;

					rayDiff = vRenderRayLength - vRayLength;

					diff = ((uint32_t)cosAngle * rayDiff) / FINE_BY_X;
					if ((rayAngle >= 0) && (rayAngle < 180)) {
						wallX += diff;
					} else {
						wallX -= diff;
					}
				}
			} else {
				/* clear vertical hit indicator bit */
				wallX &= ~0x80;
			}

			/*
			 * This ugly fix is required because the loss of precision due to the
			 * fixed point calculation (byX) is causing some situations where rays
			 * hit blocks that they would not hit during calculation on the paper.
			 *
			 * This causes for e.g. door frames the situation that the vertical ray
			 * and the horizontal ray do not hit their blocks as expected and thus
			 * frame rendering is not correct and might lead to visual artifacts.
			 *
			 * So wallX sometimes gets negative or bigger than the blocksize.
			 */
			/* FIXME: something is odd when this is required */
			if (wallX < 0)
				wallX = 0;
			else
				wallX &= BLOCK_SIZE - 1;

			/* check if the ray can pass the door */
			if (tile == H_DOOR) {
				int8_t doorWallX = checkDoor(wallX);
				if (doorWallX == -1)
					continue;

				/* take new doors texture x position */
				wallX = doorWallX;
				break;
			} else {
				break;
			}
		}
		if (tile != FOG) {
			/*
			 * correct wall x positions depending on texture orientation
			 */
			if (textureOrientation == TEXTURE_RIGHT_TO_LEFT) {
				wallX = (BLOCK_SIZE - 1) - wallX;
			}

			/* correct blockside index */
			blockSideIndex += blockSideInc;

			/*
			 * correct fishbowl effect
			 */
			// TODO remove abs here
			uint32_t tmp = (uint32_t)rayLength * pgm_fineCosByX(abs(FIELD_OF_VIEW / 2 - ray));
			rayLength = tmp / FINE_BY_X;
		}

		/*
		 * remember the maximum ray length where displaying sprites would make
		 * sense
		 */
// XXX
#if 0
		if ((rayLength > maxRayLength) && ((rayLength < SPRITE_HEIGHT * DIST_TO_PROJECTION_PLANE / 6)))
			maxRayLength = rayLength;
#endif
		maxRayLength = rayLength > maxRayLength? rayLength: maxRayLength;
		/*
		 * save ray information into frame buffer
		 *
		 * The data is store columnwise starting at column 64 of the
		 * framebuffer.
		 */
		ri->rayLength = rayLength;
		ri->wallX = wallX;

		uint8_t flags = blockSideIndex;

		void *ptr;
		if (tile == TRIGGER) {
			ptr = (void *)es.ct;
		} else if (tile == V_M_W) {
			ptr = (void *)es.cmw;
		} else if (tile == H_DOOR) {
			ptr = (void *)es.cd;
		} else {
			ptr = NULL;
		}
		ri->ptr = ptr;
		ri->flags = flags;
		ri->tile = tile;

		ri++;

		/* increment ray angle */
		rayAngle++;
		if (rayAngle >= 360)
			rayAngle -= 360;

		/* clean list of blocks to ignore */
		cleanIgnoreBlock();
	}
#if defined(CONFIG_FPS_MEASUREMENT)
	render_calc = millis() - render_calc_start;
#endif

	/* update all sprites (items, enemies, projectiles) */
	updateSprites(screenYStart, playerFOVLeftAngle, maxRayLength);

#if defined(CONFIG_FPS_MEASUREMENT)
	render_draw_start = millis();
#endif

	rayAngle = playerFOVLeftAngle;

	/* set ray info pointer back to start */
	ri = (struct rayinfo *)(arduboy->getBuffer() + (512));

	for (ray = 0; ray < FIELD_OF_VIEW; ray++) {
bitSet(PORTF, 1);
		/* decrement fire, on zero the weapon will fire */
		es.fireCountdown--;

		uint16_t rayLength = ri->rayLength;
		uint8_t tile = ri->tile;
		uint8_t wallX = ri->wallX;
		uint8_t blockSideIndex = ri->flags & 0x7;

		uint8_t texX; // TODO might need texWidth - texX in some cases, /2 as texturewidth is half the blocksize
		uint24_t p;

		/*
		 * texture width is 32 but blocksize/wallX is 64, so divide by 2,
		 * textures are stored columnwise so we need to multiply by height
		 * in bytes (32 pixel high, 32 / 8= 4)
		 */
		texX = wallX / 2 * TEXTURE_HEIGHT_BYTES;

		uint8_t side = xlateBlockHitToSide[blockSideIndex];
		uint8_t block_id;
		uint8_t textureIndexOffset = 0;

		void *ptrValue = ri->ptr;

		/*
		 * memory that ri is pointing to is free here until the end of the loop
		 * so use it for some rendering information meanwhile
		 */
		struct renderInfo *re = (struct renderInfo *)ri;
		re->ystart = 0;
		re->yend = SCREEN_HEIGHT;

		ri++;

		if (rayLength >= DIST_TO_PROJECTION_PLANE) {
			/* TODO:
			 *   simulate load of sky and floor from flash
			 *   (this is about ~1.2ms of the drawing time)
			 *   sky+floor is a 128x64px picture
			 */
			FX::seekData(background_flashoffset + ray * 8);
			/* copy the sky */
			es.screenColumn[0] = FX::readPendingUInt8();
			es.screenColumn[1] = FX::readPendingUInt8();
			es.screenColumn[2] = FX::readPendingUInt8();
			es.screenColumn[3] = FX::readPendingUInt8();
			/* copy the floor */
			es.screenColumn[4] = FX::readPendingUInt8();
			es.screenColumn[5] = FX::readPendingUInt8();
			es.screenColumn[6] = FX::readPendingUInt8();
			es.screenColumn[7] = FX::readEnd();
		}

		if (tile == V_M_W) {
			struct movingWall *cmw = (struct movingWall *)ptrValue;
			/*
			 * for moving walls we take the block id and not the
			 * tile number as the latter one is the same for all
			 * moving walls
			 */
			block_id = cmw->block_id;
		} else if (tile == H_DOOR) {
			struct door *cd = (struct door *)ptrValue;
			/*
			 * for doors we take the block id and not the tile
			 * number as the latter one is the same for all doors
			 */
			block_id = cd->flags >> 4;
		} else if (tile == TRIGGER) {
			struct trigger *ct = (struct trigger *)ptrValue;
			/*
			 * for triggers we take the block id and not the tile
			 * number as the latter one is the same for all triggers
			 */
			block_id = ct->block_id;
			/* depending on the triggers state the texture changes */
			textureIndexOffset = ct->flags & 1;
		} else {
			/*
			 * for regualar tiles we take the tile number as block
			 * id for the textures
			 *
			 * (clear half block indicator bit)
			 */
			block_id = (tile & 0x1f) - 1; //(0x1f & ~HALF_BLOCKS_START)) - 1;
		}
		uint8_t textureIndex = blockTextures[block_id * 4 + side] + textureIndexOffset;

		p = textureData_flashoffset + (textureIndex * textureDataAlignment);

		/*
		 * BLOCK_SIZE / rayLength * DIST_TO_PROJECTION_PLANE
		 * raylength min = 10
		 * raylength max = 65536
		 * wallHeight = 6528
		 * wallHeight min = 6528 / 65536 = 0 = MIN_WALL_HEIGHT
		 * wallHeight max = 6528 / 10 = 652
		 */
#ifdef CONFIG_LOD
		FX::seekData(rayLengths_flashoffset + rayLength * 9);
		uint16_t wallHeight = FX::readPendingUInt8();
		wallHeight |= FX::readPendingUInt8() << 8;
		uint16_t scale = FX::readPendingUInt8();
		scale |= FX::readPendingUInt8() << 8;
		/* TODO this is only required for scale up */
		uint8_t px_base = FX::readPendingUInt8();

		/* select texture depending on level of detail */
		if (wallHeight < TEXTURE_HEIGHT) {
			/* if texture is smaller than real height */
			scale /= 2;
			p += TEXTURE_SIZE; /* skip size 1 texture */
			texX = wallX / 4 * 2;  /* 64 / 4 = 16 (*2 because 16bits in height) */
		} else if (wallHeight < (TEXTURE_HEIGHT / 2)) {
			/* if texture is smaller than real height / 2 */
			scale /= 4;
			p += TEXTURE_SIZE + TEXTURE_SIZE / 4; /* skip size 1 and 2 texture */
			texX = wallX / 8;  /* 64 / 8 = 8 */
		}

		/* deselect cart */
		FX::readEnd();
#endif
		/* TODO read effect id from table */
		uint8_t effect = textureEffects[block_id] & 0xf;

		// TODO remove this for walls
		memset(es.texColumn, 0xff, 4);

		if (tile == FOG) {
			memset(&es.texColumn[4], 0x00, 4);
		} else {
			/* load texture (flash access) */
			if (effect == 0)
				textureEffectNone(p, texX);
			else if (effect == 1)
				textureEffectRotateLeft(p, texX);
		}
#ifdef RAY32_DEBUG
		if (ray == 32)
			memset(es.texColumn, 0xff, 4);
#endif

		/* skip wallHeight, scale and px_base */
#ifdef CONFIG_LOD
		FX::seekData(rayLengths_flashoffset + rayLength * 9 + 5);
#else
		FX::seekData(rayLengths_flashoffset + rayLength * 9);
		uint16_t wallHeight = FX::readPendingUInt8();
		wallHeight |= FX::readPendingUInt8() << 8;
		uint16_t scale = FX::readPendingUInt8();
		scale |= FX::readPendingUInt8() << 8;
		/* TODO this is only required for scale up */
		uint8_t px_base = FX::readPendingUInt8();
#endif
		/**************************************************************
		 *
		 * draw texture onto the wall
		 *
		 * find x coordinate of the texture
		 *   + textures are always 32x32 pixels in size
		 */
		int16_t screenY;

		screenY = (SCREEN_HEIGHT - (int16_t)wallHeight) / 2 + screenYStart;

		/**************************************************************
		 *
		 * draw vertical slice of the texture
		 *
		 * handle horizontal doors
		 *
		 *
		 * IDEA remember rendered columns per tile/texX/texX -> wallHeight
		 * per tile - > wallHeight -> (texX, texY)
		 *
		 *
		 * just the regular wall textures
		 */
		/*
		 * only draw textures when wall is close enough
		 * as details are anyway not visible at this resolution
		 */
		if (wallHeight >= MIN_WALL_HEIGHT) {
			if (scale < TEXTURE_SCALE_UP_LIMIT)
				drawTextureColumnScaleUp2(screenY, wallHeight, px_base, re);
			else
				drawAll(screenY, scale, wallHeight, re);
		} else {

			drawNoTexture(screenY, wallHeight, re);
		}
		/* deselect cart */
		FX::readEnd();

		/* handle sprites for this ray */
		handleSprites(rayLength, playerFOVLeftAngle, re);

		/* copy current screen column into next column */
		unsigned char *buffer = arduboy->getBuffer() + (ray * 2 * HEIGHT_BYTES);
		/* manually unrolled */
		*buffer++ = es.screenColumn[0];
		*buffer++ = es.screenColumn[1];
		*buffer++ = es.screenColumn[2];
		*buffer++ = es.screenColumn[3];
		*buffer++ = es.screenColumn[4];
		*buffer++ = es.screenColumn[5];
		*buffer++ = es.screenColumn[6];
		buffer++; /* [7] is HUD */
		*buffer++ = es.screenColumn[0];
		*buffer++ = es.screenColumn[1];
		*buffer++ = es.screenColumn[2];
		*buffer++ = es.screenColumn[3];
		*buffer++ = es.screenColumn[4];
		*buffer++ = es.screenColumn[5];
		*buffer   = es.screenColumn[6];

		/* increment ray angle */
		rayAngle++;
		if (rayAngle >= 360)
			rayAngle -= 360;
bitClear(PORTF, 1);
	}

	es.nrOfVisibleSprites = 0;

#if defined(CONFIG_FPS_MEASUREMENT)
	drawing_start = millis();
	render_draw = drawing_start - render_draw_start;
#endif

	/***************************************************************
	 *
	 * display players weapon
	 */
	drawBitmap(56,
		   42 - (es.weaponVisibleOffset >> 4),
		   player_weapons_flashoffset + (es.playerActiveWeapon * 96)  + (es.rectOffset / 16 *32),
		   16, 16, WHITE);

	/***************************************************************
	 *
	 * display HUD (Heads Up Display)
	 *   + main HUD image is already loaded to display buffer by displayPrefetch
	 *
	 */
	/* first HUD portion of currently selected weapon */
	drawBitmap(0, 56, icons_hud_weapon_flashoffset + (es.playerActiveWeapon * 24), 24, 8, WHITE);
	/* display health */
	drawNumber(62, 56, es.playerHealth);
	/* display ammo of current weapon */
	drawNumber(37, 56, es.playerAmmo[es.playerActiveWeapon]);


	/***************************************************************
	 *
	 * update timeouts
	 *
	 */
	/* countdown until weapon can be changed again */
	if (es.weaponChangeCooldown != 0) {
		es.weaponChangeCooldown--;
	}

	/* countdown until status message disappears */
	if (es.statusMessage.timeout) {
		es.statusMessage.timeout--;
		drawString(1, 0, es.statusMessage.text);
	}

	/* next frame */
	es.frame++;
#ifdef CONFIG_FPS_MEASUREMENT
	drawing = millis() - drawing_start;
#endif
#ifdef AUDIO
	/* handle audio */
	struct audio_effect *ae = &audioEffects[0];
	const uint8_t *sound;

	/* environments */
	switch (ae->trigger) {
	case AUDIO_EFFECT_TRIGGER_START:
		break;
	}
	ae->trigger = 0;
	ae++;

	/* weapon */
	switch (ae->trigger) {
	case AUDIO_EFFECT_TRIGGER_START:
		// TODO depending on ae->data select sfx
		atm_synth_play_sfx_track(OSC_CH_2, 0, (const uint8_t*)&sfx1);
		break;
	}
	ae->trigger = 0;
	ae++;

	/* map */
	switch (ae->trigger) {
	case AUDIO_EFFECT_TRIGGER_START:
		// TODO depending on ae->data select sfx
		sound = (const uint8_t *)pgm_read_ptr(&mapSounds[ae->data]);
		atm_synth_play_sfx_track(OSC_CH_3, 0, sound); //(const uint8_t*)&sfx2);
		break;
	}
	ae->trigger = 0;
	ae++;

	/* music */
	switch (ae->trigger) {
	case AUDIO_EFFECT_TRIGGER_START:
		break;
	default:
		/* check if score is still player and restart if not */
		if (!atm_synth_is_score_playing())
			atm_synth_start_score((const uint8_t*)&score);
		break;
	}
	ae->trigger = 0;
#endif

	/* evaluate quests */
	evaluateActiveQuest();

	/* determine current game state */
	if (es.playerHealth == 0) {
		/* player is dead */
		setSystemEvent(EVENT_PLAYER_DEAD, es.killedBySprite);
	}
#if defined(CONFIG_FPS_MEASUREMENT)
	total = millis() - total_start;
#endif
#if defined(CONFIG_FPS_MEASUREMENT)
	drawNumber(10, 56, render_calc);
	drawNumber(85, 56, render_draw);
	drawNumber(100, 56, drawing);
	drawNumber(115, 56, total);
#endif
}
