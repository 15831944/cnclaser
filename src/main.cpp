#include "stepper.h"
#include "laser.h"
#include "rasterizer.h"
#include "timer.h"
#include "pwm.h"
#include "parser.h"
#include "circular_buffer.h"
#include "usart.h"

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

static Stepper stepper1(PORTC, PINC, DDRC, 0, 1, 2, 3, true);
static Stepper stepper2(PORTD, PIND, DDRD, 2, 3, 4, 5, true);
static Stepper *steppers[2] = {&stepper1, &stepper2};

static Laser laser;

static Rasterizer rasterizer(steppers, &laser, Timer::PERIOD_US);
static Parser parser;
static Usart usart;

void tic()
{
	rasterizer.Tic();
}

void receive(uint8_t data)
{
	parser.Received(data);
}

void welcome()
{
	// Help message
	static const char buf[512] PROGMEM =
R"(Welcome to cnclaser
Commands:
M1 - Laser on
M2 - Laser off
G0 X (float) Y (float) - Fast move
G1 X (float) Y (float) F (int) - Linear move
G2 X (float) Y (float) I (float) J (float) F (int) - Clock wise arc
G3 X (float) Y (float) I (float) J (float) F (int) - Counter clock wise arc
)";

	MainUsart.SendP(buf);
}

void ready()
{
	MainUsart.SendP(PSTR("Ready\n"));
}

void setup()
{
	MainTimer.Init(tic);
	MainPwm.Init();
	MainUsart.Init(receive);

	FOREACH_AXIS {
		Stepper *stepper = steppers[i];
		stepper->Init();
		stepper->Calibrate();
	}

	sei();
	welcome();
	ready();
}

float feedRateToSpeed(int feed)
{
	// Convert from min to second.
	return float(feed) / 60.f;
}

void loop()
{
	const Parser::Command cmd = parser.NextCommand();

	// TODO mediator
	switch (cmd.type) {
		case Parser::Command::LINEAR_MOVE:
		{
			rasterizer.AddLine(cmd.pos, feedRateToSpeed(cmd.feed));
			break;
		}
		case Parser::Command::LINEAR_MOVE_FAST:
		{
			rasterizer.AddLine(cmd.pos, Config::LASER_OFF_SPEED);
			break;
		}
		case Parser::Command::CW_ARC_MOVE:
		{
			rasterizer.AddCircle(cmd.pos, cmd.rel, Rasterizer::ARC_CW, feedRateToSpeed(cmd.feed));
			break;
		}
		case Parser::Command::CCW_ARC_MOVE:
		{
			rasterizer.AddCircle(cmd.pos, cmd.rel, Rasterizer::ARC_CCW, feedRateToSpeed(cmd.feed));
			break;
		}
		case Parser::Command::LASER_ON:
		{
			rasterizer.EnableLaser();
			break;
		}
		case Parser::Command::LASER_OFF:
		{
			rasterizer.DisableLaser();
			break;
		}
		default:
		{
			break;
		}
	}
}

int main()
{
	setup();

	while (true) {
		loop();
	}
}
