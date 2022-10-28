#include <Arduino.h>

/* ************************************************************************** */

/* Debug */
#define NDEBUG

/* Network ID */
#define DEVICE_ID 0
#define NUMBER_OF_DEVICES 2

/* Feature Constants */
#define CLOCK_PCF85063TP 1
#define CLOCK_DS1307 2
#define CLOCK_DS3231 3
#define BATTERY_GAUGE_DFROBOT 1
#define BATTERY_GAUGE_LC709203F 2

/* Features */
//	#define ENABLE_LED
//	#define ENABLE_COM_OUTPUT
//	#define ENABLE_OLED_OUTPUT
//	#define ENABLE_CLOCK CLOCK_DS3231
//	#define ENABLE_SD_CARD
//	#define ENABLE_SLEEP
//	#define ENABLE_BATTERY_GAUGE BATTERY_GAUGE_DFROBOT
//	#define ENABLE_DALLAS
//	#define ENABLE_BME280
//	#define ENABLE_LTR390

/* Software Parameters */
#define WIFI_SSID "SSID"
#define WIFI_PASS "PASSWORD"
#define HTTP_UPLOAD_FORMAT "http://www.example.com/upload?device=%1$u&serial=%2$u&time=%3$s"
#define HTTP_UPLOAD_LENGTH 256
#define HTTP_AUTHORIZATION_TYPE ""
#define HTTP_AUTHORIZATION_CODE ""
#define NTP_SERVER "stdtime.gov.hk"
#define SECRET_KEY "This is secret!"
#define DATA_FILE_PATH "/data.csv"
#define CLEANUP_FILE_PATH "/cleanup.csv"
#define SYNCHONIZE_INTERVAL 7654321UL /* milliseconds */
#define SYNCHONIZE_MARGIN 1234UL /* milliseconds */
#define RESEND_TIMES 3
#define ACK_TIMEOUT 1000UL /* milliseconds */
#define UPLOAD_INTERVAL 6000UL /* milliseconds */
#define CLEANLOG_INTERVAL 86400000UL /* milliseconds */
#define MEASURE_INTERVAL 60000UL /* milliseconds */ /* MUST: > UPLOAD_INTERVAL */
#define SLEEP_MARGIN 1000 /* milliseconds */
#define ROUTER_TOPOLOGY {}

/* Hardware Parameters */
#define CPU_FREQUENCY 20
#define COM_BAUD 115200
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_I2C_ADDR 0x3C
#define DALLAS_PIN 3
#define LORA_BAND 868000000

/* ************************************************************************** */

#include "config.h"

#include <stdlib.h>
#include <time.h>
#include <cstring>
#include <memory>
#include <vector>

#include <RNG.h>
#include <AES.h>
#include <GCM.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <Adafruit_SSD1306.h>

typedef uint8_t PacketType;
typedef unsigned long int Time;
typedef uint8_t Device;
typedef uint32_t SerialNumber;
typedef GCM<AES128> AuthCipher;

static Device const router_topology[][2] = ROUTER_TOPOLOGY;
static char const secret_key[16] PROGMEM = SECRET_KEY;
static char const data_file_path[] PROGMEM = DATA_FILE_PATH;
static char const cleanup_file_path[] PROGMEM = CLEANUP_FILE_PATH;

template <typename TYPE>
uint8_t rand_int(void) {
	TYPE x;
	RNG.rand((uint8_t *)&x, sizeof x);
	return x;
}

#ifdef ENABLE_COM_OUTPUT
	inline static void Serial_initialize(void) {
		#ifdef CPU_FREQUENCY
			#if CPU_FREQUENCY < 80
				Serial.begin(COM_BAUD * 80 / CPU_FREQUENCY);
			#else
				Serial.begin(COM_BAUD);
			#endif
		#else
			Serial.begin(COM_BAUD);
		#endif
	}

	template <typename TYPE>
	inline void Serial_print(TYPE const x) {
		Serial.print(x);
	}

	template <typename TYPE>
	inline void Serial_println(TYPE x) {
		Serial.println(x);
	}

	template <typename TYPE>
	inline void Serial_println(TYPE const x, int option) {
		Serial.println(x, option);
	}
#else
	inline static void Serial_initialize(void) {
		Serial.end();
	}

	template <typename TYPE> inline void Serial_print(TYPE x) {}
	template <typename TYPE> inline void Serial_println(TYPE x) {}
	template <typename TYPE> inline void Serial_print(TYPE x, int option) {}
	template <typename TYPE> inline void Serial_println(TYPE x, int option) {}
#endif

static Adafruit_SSD1306 OLED(OLED_WIDTH, OLED_HEIGHT);

static void OLED_turn_off(void) {
	OLED.ssd1306_command(SSD1306_CHARGEPUMP);
	OLED.ssd1306_command(0x10);
	OLED.ssd1306_command(SSD1306_DISPLAYOFF);
}

static void OLED_turn_on(void) {
	OLED.ssd1306_command(SSD1306_CHARGEPUMP);
	OLED.ssd1306_command(0x14);
	OLED.ssd1306_command(SSD1306_DISPLAYON);
}

#ifdef ENABLE_OLED_OUTPUT
	static class String OLED_message;

	static void OLED_initialize(void) {
		OLED.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
		OLED.invertDisplay(false);
		OLED.setRotation(2);
		OLED.setTextSize(1);
		OLED.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
		OLED.clearDisplay();
		OLED.setCursor(0, 0);
	}

	inline static void OLED_home(void) {
		OLED.clearDisplay();
		OLED.setCursor(0, 0);
	}

	template <typename TYPE>
	inline void OLED_print(TYPE x) {
		OLED.print(x);
	}

	template <typename TYPE>
	inline void OLED_println(TYPE x) {
		OLED.println(x);
	}

	template <typename TYPE>
	inline void OLED_println(TYPE const x, int const option) {
		OLED.println(x, option);
	}

	inline static void OLED_display(void) {
		OLED.display();
	}
#else
	inline static void OLED_initialize(void) {
		OLED.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
		OLED_turn_off();
	}
	inline static void OLED_home(void) {}
	template <typename TYPE> inline void OLED_print(TYPE x) {}
	template <typename TYPE> inline void OLED_println(TYPE x) {}
	template <typename TYPE> inline void OLED_println(TYPE x, int option) {}
	inline static void OLED_display(void) {}
#endif

template <typename TYPE>
inline void any_print(TYPE x) {
	Serial_print(x);
	OLED_print(x);
}

template <typename TYPE>
inline void any_println(TYPE x) {
	Serial_println(x);
	OLED_println(x);
}

template <typename TYPE>
inline void any_println(TYPE x, int option) {
	Serial_println(x, option);
	OLED_println(x, option);
}

#ifdef ENABLE_COM_OUTPUT
void Serial_dump(char const *const label, void const *const memory, size_t const size) {
	Serial.printf("%s (%04X)", label, size);
	for (size_t i = 0; i < size; ++i) {
		unsigned char const c = i[(unsigned char const *)memory];
		Serial.printf(" %02X", c);
	}
	Serial.write('\n');
}
#else
inline static void Serial_dump(void *const memory, size_t const size) {}
#endif

#ifdef ENABLE_LED
	static void LED_initialize(void) {
		pinMode(LED_BUILTIN, OUTPUT);
		digitalWrite(LED_BUILTIN, LOW);
	}

	static void LED_flash(void) {
		digitalWrite(LED_BUILTIN, HIGH);
		delay(200);
		digitalWrite(LED_BUILTIN, LOW);
		delay(200);
	}
#else
	inline static void LED_initialize(void) {}
	inline static void LED_flash(void) {}
#endif

struct [[gnu::packed]] FullTime {
	unsigned short int year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
};

static class String String_from_FullTime(struct FullTime const *const fulltime) {
	char buffer[48];
	snprintf(
		buffer, sizeof buffer,
		"%04u-%02u-%02uT%02u:%02u:%02uZ",
		fulltime->year, fulltime->month, fulltime->day,
		fulltime->hour, fulltime->minute, fulltime->second
	);
	return String(buffer);
}

#ifdef ENABLE_CLOCK
	#if ENABLE_CLOCK == CLOCK_PCF85063TP
		#include <PCF85063TP.h>

		namespace RTC {
			class PCD85063TP external_clock;

			static bool initialize(void) {
				external_clock.begin();
				external_clock.startClock();
				return true;
			}

			static void set(struct FullTime const *const fulltime) {
				external_clock.stopClock();
				external_clock.fillByYMD(fulltime->year, fulltime->month, fulltime->day);
				external_clock.fillByHMS(fulltime->hour, fulltime->minute, fulltime->second);
				external_clock.setTime();
				external_clock.startClock();
			}

			static bool ready(void) {
				static bool available = false;
				if (available) return true;
				external_clock.getTime();
				available =
					1 <= external_clock.year       && external_clock.year       <= 99 &&
					1 <= external_clock.month      && external_clock.month      <= 12 &&
					1 <= external_clock.dayOfMonth && external_clock.dayOfMonth <= 30 &&
					0 <= external_clock.hour       && external_clock.hour       <= 23 &&
					0 <= external_clock.minute     && external_clock.minute     <= 59 &&
					0 <= external_clock.second     && external_clock.second     <= 59;
				return available;
			}

			static struct FullTime now(void) {
				external_clock.getTime();
				return (struct FullTime){
					.year = (unsigned short int)(2000U + external_clock.year),
					.month = external_clock.month,
					.day = external_clock.dayOfMonth,
					.hour = external_clock.hour,
					.minute = external_clock.minute,
					.second = external_clock.second
				};
			}
		}
	#elif ENABLE_CLOCK == CLOCK_DS1307 || ENABLE_CLOCK == CLOCK_DS3231
		#include <RTClib.h>

		namespace RTC {
			#if ENABLE_CLOCK == CLOCK_DS1307
				class RTC_DS1307 external_clock;
			#elif ENABLE_CLOCK == CLOCK_DS3231
				class RTC_DS3231 external_clock;
			#endif

			static bool initialize(void) {
				if (!external_clock.begin()) {
					any_println("Clock not found");
					return false;
				}
				#if ENABLE_CLOCK == CLOCK_DS1307
					if (!external_clock.isrunning()) {
						any_println("DS1307 not running");
						return false;
					}
				#endif
				return true;
			}

			static void set(struct FullTime const *const fulltime) {
				class DateTime const datetime(
					fulltime->year, fulltime->month, fulltime->day,
					fulltime->hour, fulltime->minute, fulltime->second
				);
				external_clock.adjust(datetime);
			}

			static bool ready(void) {
				class DateTime const datetime = external_clock.now();
				return datetime.isValid();
			}

			static struct FullTime now(void) {
				class DateTime const datetime = external_clock.now();
				return (struct FullTime){
					.year = datetime.year(),
					.month = datetime.month(),
					.day = datetime.day(),
					.hour = datetime.hour(),
					.minute = datetime.minute(),
					.second = datetime.second()
				};
			}
		}
	#endif
#endif

/* ************************************************************************** */

void setup(void) {
}

void loop(void) {
}

/* ************************************************************************** */
