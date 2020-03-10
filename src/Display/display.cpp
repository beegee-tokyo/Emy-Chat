#include "main.h"

#ifdef HAS_DISPLAY
#ifdef ESP32
#include <SH1106Wire.h>
#else
#include <SH1106WireNRF.h>
#endif
/** Number of message lines */
#define NUM_OF_LINES (OLED_HEIGHT - STATUS_BAR_HEIGHT) / LINE_HEIGHT

/** Line buffer for messages */
char buffer[NUM_OF_LINES][32] = {0};

/** Current line used */
uint8_t currentLine = 0;

/** Number of elements in the mesh map */
extern uint8_t numElements;

/** Singleton of the display class */
#ifdef NRF52
SH1106Wire display(0x3c, OLED_SDA, OLED_SCL);
#else
SH1106Wire display(0x3c, OLED_SDA, OLED_SCL);
#endif
/** Task to control the LEDs */
TaskHandle_t displayHandler = NULL;

/**
 * Initialize the display
 */
void initDisplay(void)
{
	myLog_d("Display init");
	display.init();
	display.displayOn();
	display.flipScreenVertically();
	display.setContrast(255);
	display.setFont(ArialMT_Plain_10);

	dispWriteHeader();
	display.display();

	currentLine = 0;
}

/**
 * Write the top line of the display
 */
void dispWriteHeader(void)
{
	display.setFont(ArialMT_Plain_10);

	// clear the status bar
	display.setColor(BLACK);
	display.fillRect(0, 0, OLED_WIDTH, STATUS_BAR_HEIGHT);

	// draw MAC
	display.setColor(WHITE);
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	char lineString[64] = {0};
	sprintf(lineString, "EmyChat %08X #n %d", deviceID, numOfNodes() + 1);
	display.drawString(0, 0, lineString);

	// draw divider line
	display.drawLine(0, 12, 128, 12);
	display.display();
}

/**
 * Add a line to the display buffer
 * @param newLine
 * 		Pointer to char array with the new line
 */
void dispAddLine(char * line)
{
	myLog_d("Adding line %d", currentLine);

	if (currentLine == NUM_OF_LINES)
	{
		myLog_d("Shifting buffer up");
		// Display is full, shift text one line up
		for (int idx = 0; idx < NUM_OF_LINES; idx++)
		{
			memcpy(buffer[idx], buffer[idx + 1], 32);
		}
		currentLine--;
	}
	snprintf(buffer[currentLine], 32, "%s", line);

	if (currentLine != NUM_OF_LINES)
	{
		currentLine++;
	}
}

/**
 * Update display messages
 */
void dispShow(void)
{
	display.setColor(BLACK);
	display.fillRect(0, STATUS_BAR_HEIGHT + 1, OLED_WIDTH, OLED_HEIGHT);

	display.setFont(ArialMT_Plain_10);
	display.setColor(WHITE);
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	for (int line = 0; line < currentLine; line++)
	{
		myLog_v("Writing %s to line %d which starts at %d",
				buffer[line],
				line,
				(line * LINE_HEIGHT) + STATUS_BAR_HEIGHT + 1);
		display.drawString(0, (line * LINE_HEIGHT) + STATUS_BAR_HEIGHT + 1, String(buffer[line]));
	}
	display.display();
}

/**
 * Write text to the display
 * @param text
 * 		Text to be written
 * @param x
 * 		X position where text starts
 * @param y
 * 		y position where text starts
 */
void dispWrite(String text, int x, int y)
{
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.drawString(x, y, text);
}

/**
 * Update the display content
 */
void dispUpdate(void)
{
	display.display();
}
#endif