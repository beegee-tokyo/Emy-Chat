void initDisplay(void);
void dispWriteHeader(void);
void dispAddLine(char *line);
void dispShow(void);
void dispWrite(String text, int x, int y);
void dispUpdate(void);
// #define OLED_SDA 23
// #define OLED_SCL 22
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define STATUS_BAR_HEIGHT 12
#define LINE_HEIGHT 10
#define MARGIN 3
