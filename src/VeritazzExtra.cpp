#include "VeritazzExtra.h"

Arduboy2Ex::Arduboy2Ex()
{
}

void Arduboy2Ex::begin()
{
	boot();

	if (buttonsState() & UP_BUTTON) {
		power_timer0_disable();
		for (;;)
			idle();
	}
}
