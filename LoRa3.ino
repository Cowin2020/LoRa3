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

/* Protocol Constants */
#define PACKET_TIME    0
#define PACKET_ASKTIME 1
#define PACKET_ACK     2
#define PACKET_SEND    3

#define CIPHER_IV_LENGTH 12
#define CIPHER_TAG_SIZE 4

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

namespace LED {
	#ifdef ENABLE_LED
		static void initialize(void) {
			pinMode(LED_BUILTIN, OUTPUT);
			digitalWrite(LED_BUILTIN, LOW);
		}

		static void flash(void) {
			digitalWrite(LED_BUILTIN, HIGH);
			delay(200);
			digitalWrite(LED_BUILTIN, LOW);
			delay(200);
		}
	#else
		inline static void initialize(void) {}
		inline static void flash(void) {}
	#endif
}

namespace COM {
	#ifdef ENABLE_COM_OUTPUT
		inline static void initialize(void) {
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
		inline void print(TYPE const x) {
			Serial.print(x);
		}

		template <typename TYPE>
		inline void println(TYPE x) {
			Serial.println(x);
		}

		template <typename TYPE>
		inline void println(TYPE const x, int option) {
			Serial.println(x, option);
		}

		void dump(char const *const label, void const *const memory, size_t const size) {
			Serial.printf("%s (%04X)", label, size);
			for (size_t i = 0; i < size; ++i) {
				unsigned char const c = i[(unsigned char const *)memory];
				Serial.printf(" %02X", c);
			}
			Serial.write('\n');
		}
	#else
		inline static void initialize(void) {
			Serial.end();
		}

		template <typename TYPE> inline void print(TYPE x) {}
		template <typename TYPE> inline void println(TYPE x) {}
		template <typename TYPE> inline void print(TYPE x, int option) {}
		template <typename TYPE> inline void println(TYPE x, int option) {}
		inline static void dump(void *const memory, size_t const size) {}
	#endif
}

namespace OLED {
	static Adafruit_SSD1306 SSD1306(OLED_WIDTH, OLED_HEIGHT);

	static void turn_off(void) {
		SSD1306.ssd1306_command(SSD1306_CHARGEPUMP);
		SSD1306.ssd1306_command(0x10);
		SSD1306.ssd1306_command(SSD1306_DISPLAYOFF);
	}

	static void turn_on(void) {
		SSD1306.ssd1306_command(SSD1306_CHARGEPUMP);
		SSD1306.ssd1306_command(0x14);
		SSD1306.ssd1306_command(SSD1306_DISPLAYON);
	}

	#ifdef ENABLE_OLED_OUTPUT
		static class String message;

		static void initialize(void) {
			SSD1306.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
			SSD1306.invertDisplay(false);
			SSD1306.setRotation(2);
			SSD1306.setTextSize(1);
			SSD1306.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
			SSD1306.clearDisplay();
			SSD1306.setCursor(0, 0);
		}

		inline static void home(void) {
			SSD1306.clearDisplay();
			SSD1306.setCursor(0, 0);
		}

		template <typename TYPE>
		inline void print(TYPE x) {
			SSD1306.print(x);
		}

		template <typename TYPE>
		inline void println(TYPE x) {
			SSD1306.println(x);
		}

		template <typename TYPE>
		inline void println(TYPE const x, int const option) {
			SSD1306.println(x, option);
		}

		inline static void display(void) {
			SSD1306.display();
		}
	#else
		inline static void initialize(void) {
			SSD1306.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
			turn_off();
		}
		inline static void home(void) {}
		template <typename TYPE> inline void print(TYPE x) {}
		template <typename TYPE> inline void println(TYPE x) {}
		template <typename TYPE> inline void println(TYPE x, int option) {}
		inline static void display(void) {}
	#endif
}

template <typename TYPE>
inline void any_print(TYPE x) {
	COM::print(x);
	OLED::print(x);
}

template <typename TYPE>
inline void any_println(TYPE x) {
	COM::println(x);
	OLED::println(x);
}

template <typename TYPE>
inline void any_println(TYPE x, int option) {
	COM::println(x, option);
	OLED::println(x, option);
}

struct [[gnu::packed]] FullTime {
	unsigned short int year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	explicit operator String(void) const;
};

FullTime::operator String(void) const {
	char buffer[48];
	snprintf(
		buffer, sizeof buffer,
		"%04u-%02u-%02uT%02u:%02u:%02uZ",
		this->year, this->month, this->day,
		this->hour, this->minute, this->second
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

namespace Sleep {
	class Unsleep {
	public:
		Unsleep(void) {}
		virtual bool awake(void) = 0;
	};

	#ifdef ENABLE_SLEEP
		static Time const MAXIMUM_SLEEP_LENGTH = 24 * 60 * 60 * 1000; /* milliseconds */
		static bool enabled = false;
		static Time wake_time = 0;
		static std::vector<class Unsleep *> waiting_list;
		static bool waiting_ack = false;
		static bool waiting_synchronization = false;

		inline static bool in_range(Time const period) {
			return 0 < period && period < MAXIMUM_SLEEP_LENGTH;
		}

		static void alarm(Time const wake) {
			if (enabled) {
				Time const now = millis();
				Time const period_0 = wake_time - now;
				Time const period_1 = wake - now;
				if (!in_range(period_0)) return;
				if (!in_range(period_1) || period_0 <= period_1) return;
			}
			enabled = true;
			wake_time = wake;
		}

		static void sleep(void) {
			if (!enabled || waiting_ack || waiting_synchronization) return;
			for (class Unsleep *const u: waiting_list)
				if (u->awake())
					return;
			Time const now = millis();
			Time const milliseconds = wake_time - now - SLEEP_MARGIN;
			if (milliseconds < MAXIMUM_SLEEP_LENGTH) {
				LoRa.sleep();
				esp_sleep_enable_timer_wakeup(1000 * milliseconds);
				esp_light_sleep_start();
			}
			enabled = false;
		}

		inline static void wait_ack(bool const value) {
			waiting_ack = value;
		}

		inline static void wait_synchronization(bool const value) {
			waiting_synchronization = value;
		}
	#else
		inline static void alarm(Time const wake) {}
		inline static void sleep(void) {}
		inline static void wait_ack(bool value) {}
		inline static void wait_synchronization(bool value) {}
	#endif
}

class Schedule {
protected:
	bool enable;
	Time head;
	Time period;
	Time margin;
public:
	Schedule(Time initial_period);
	bool enabled(void) const;
	Time next_run(Time now) const;
	void start(Time now, Time addition_period = 0);
	void stop(void);
	virtual bool tick(Time now);
	virtual void run(Time now);
};

inline Schedule::Schedule(Time const initial_period) :
	enable(false), head(0), period(initial_period), margin(0)
{
	/* do nothing */
}

inline bool Schedule::enabled(void) const {
	return enable;
}

inline Time Schedule::next_run(Time const now) const {
	return head + period + margin;
}

inline void Schedule::start(Time const now, Time const addition) {
	enable = true;
	head = now;
	margin = addition;
}

inline void Schedule::stop(void) {
	enable = false;
}

bool Schedule::tick(Time const now) {
	bool const need_run = enable && now - head >= period + margin;
	if (need_run) run(now);
	if (enable) Sleep::alarm(next_run(now));
	return need_run;
}

void Schedule::run(Time const now) {
	head = now;
}

namespace Schedules {
	static class std::vector<class Schedule *> list;

	inline static void add(class Schedule *const schedule) {
		/* Add [schedule] into the list */
		list.push_back(schedule);
	}

	static void remove(class Schedule *const schedule) {
		/* Remove [schedule] from the list */
		size_t const N = list.size();
		for (size_t i = 0; i < N; ++i) {
			class Schedule *const p = list[i];
			if (p == schedule) {
				list[i] = list.back();
				list.pop_back();
				break;
			}
		}
	}

	static void tick(void) {
		/* Run a schedule on time */
		Time const now = millis();
		for (class Schedule *const schedule: list)
			if (schedule->tick(now))
				return;
		/* Sleep if no schedule to run */
		Sleep::sleep();
	}
}

/* ************************************************************************** */

struct [[gnu::packed]] Data {
	struct FullTime time;
	#ifdef ENABLE_BATTERY_GAUGE
		float battery_voltage;
		float battery_percentage;
	#endif
	#ifdef ENABLE_DALLAS
		float dallas_temperature;
	#endif
	#ifdef ENABLE_BME280
		float bme280_temperature;
		float bme280_pressure;
		float bme280_humidity;
	#endif
	#ifdef ENABLE_LTR390
		float ltr390_ultraviolet;
	#endif
	void writeln(class Print *print) const;
	bool readln(class Stream *stream);
};

void Data::writeln(class Print *const print) const {
	print->printf(
		"%04u-%02u-%02uT%02u:%02u:%02uZ",
		this->time.year, this->time.month, this->time.day,
		this->time.hour, this->time.minute, this->time.second
	);
	#ifdef ENABLE_BATTERY_GAUGE
		print->printf(",%f,%f", this->battery_voltage, this->battery_percentage);
	#endif
	#ifdef ENABLE_DALLAS
		print->printf(",%f", this->dallas_temperature);
	#endif
	#ifdef ENABLE_BME280
		print->printf(
			",%f,%f,%f",
			this->bme280_temperature, this->bme280_pressure, this->bme280_humidity
		);
	#endif
	#ifdef ENABLE_LTR390
		print->printf(",%f", this->ltr390_ultraviolet);
	#endif
	print->write('\n');
}

bool Data::readln(class Stream *const stream) {
	/* Time */
	{
		class String const s = stream->readStringUntil(
			#if defined(ENABLE_DALLAS) || defined(ENABLE_BME280) || defined(ENABLE_LTR390)
				','
			#else
				'\n'
			#endif
		);
		if (
			sscanf(
				s.c_str(),
				"%4hu-%2hhu-%2hhuT%2hhu:%2hhu:%2hhuZ",
				&this->time.year, &this->time.month, &this->time.day,
				&this->time.hour, &this->time.minute, &this->time.second
			) != 6
		) return false;
	}

	/* Battery gauge */
	#ifdef ENABLE_BATTERY_GAUGE
		{
			class String const s = stream->readStringUntil(',');
			if (sscanf(s.c_str(), "%f", &this->battery_voltage) != 1) return false;
		}
		{
			class String const s = stream->readStringUntil(
				#if defined(ENABLE_DALLAS) || defined(ENABLE_BME280) || defined(ENABLE_LTR390)
					','
				#else
					'\n'
				#endif
			);
			if (sscanf(s.c_str(), "%f", &this->battery_percentage) != 1) return false;
		}
	#endif

	/* Dallas thermometer */
	#ifdef ENABLE_DALLAS
		{
			class String const s = stream->readStringUntil(
				#if defined(ENABLE_BME280) || defined(ENABLE_LTR390)
					','
				#else
					'\n'
				#endif
			);
			if (sscanf(s.c_str(), "%f", &this->dallas_temperature) != 1) return false;
		}
	#endif

	/* BME280 sensor */
	#ifdef ENABLE_BME280
		{
			class String const s = stream->readStringUntil(',');
			if (sscanf(s.c_str(), "%f", &this->bme280_temperature) != 1) return false;
		}
		{
			class String const s = stream->readStringUntil(',');
			if (sscanf(s.c_str(), "%f", &this->bme280_pressure) != 1) return false;
		}
		{
			class String const s = stream->readStringUntil(
				#if defined(ENABLE_LTR390)
					','
				#else
					'\n'
				#endif
			);
			if (sscanf(s.c_str(), "%f", &this->bme280_humidity) != 1) return false;
		}
	#endif

	/* LTR390 sensor */
	#ifdef ENABLE_LTR390
		{
			class String const s = stream->readStringUntil('\n');
			if (sscanf(s.c_str(), "%f", &this->ltr390_ultraviolet) != 1) return false;
		}
	#endif

	return true;
}

/* ************************************************************************** */

namespace LORA {
	static void receive_TIME(signed int const packet_size) {
		/* TODO */
	}

	static void receive_SEND(signed int const packet_size) {
		/* TODO */
	}

	static void receive_ACK(signed int const packet_size) {
		/* TODO */
	}

	static void receive(void) {
		signed int const packet_size = LoRa.parsePacket();
		if (packet_size < 1) return;
		Device packet_type;
		if (LoRa.readBytes(&packet_type, sizeof packet_type) != sizeof packet_type) return;
		switch (packet_type) {
		case PACKET_TIME:
			receive_TIME(packet_size);
			break;
		case PACKET_SEND:
			receive_SEND(packet_size);
			break;
		case PACKET_ACK:
			receive_ACK(packet_size);
			break;
		default:
			COM::print("LoRa: incorrect packet type: ");
			COM::println(packet_type);
		}

		/* add entropy to RNG */
		unsigned long int const microseconds = micros();
		RNG.stir((uint8_t const *)&microseconds, sizeof microseconds, 8);
	}
}

/* ************************************************************************** */

static bool setup_error = false;

void setup(void) {
	setup_error = false;
	LED::initialize();
	COM::initialize();
	OLED::initialize();
}

void loop(void) {
	if (setup_error) {
		LED::flash();
		return;
	}
	RNG.loop();
	LORA::receive();
	Schedules::tick();
}

/* ************************************************************************** */
