#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include <HardwareSerial.h>

size_t const MAX_OBIS_CODE_LENGTH = 16;
size_t const MAX_IDENTIFICATION_LENGTH = 5 + 16 + 1; /* /AAAbi...i\r */
size_t const MAX_VALUE_LENGTH = 32 + 1 + 16 + 1;	 /* value: 32, *, unit: 16 */
size_t const MAX_LINE_LENGTH = 78;
uint32_t const INITIAL_BAUD_RATE = 300;

uint32_t const SERIAL_TIMEOUT = 2000; // How long to wait for the meter to send responses to our requests. [ms]

unsigned long const MAX_METER_READ_TIME = 30; // How long it should take to read all the data/lines [seconds]

class MeterReader
{
public:
	enum class Status : uint8_t
	{
		Ready,
		Busy,
		Ok,
		TimeoutError,
		IdentificationError,
		IdentificationError_Id_Mismatch,
		ProtocolError,
		ChecksumError,
	};

	explicit MeterReader(HardwareSerial &serial, uint8_t rx, uint8_t tx, const char *identifierChars) : serial_(serial)
	{
		rx_ = rx;
		tx_ = tx;
		identifierChars_ = identifierChars;
	}

	MeterReader(MeterReader const &) = delete;

	MeterReader(MeterReader &&) = delete;

	/* Start monitoring an object. Returns true if monitoring was just started
	 * for the specified object, false otherwise */
	bool start_monitoring(std::string obis);

	/* Stop monitoring an object. Returns true if monitoring was just stopped
	 * for the specified object, false otherwise */
	bool stop_monitoring(std::string obis);

	void start_reading();

	/* Must be called frequently to advance the reading process */
	void loop();

	Status status() const { return status_; }

	/* Call this after status() returns Ok or an error to reset it to Ready */
	void acknowledge()
	{
		if (status_ != Status::Busy)
			status_ = Status::Ready;
	}

	std::string lastReadChars()
	{
		return lastReadChars_;
	}

	size_t errors() const { return errors_; }

	size_t checksum_errors() const { return checksum_errors_; }

	size_t successes() const { return successes_; }

	std::map<std::string, std::string> const &values() const { return values_; }

private:
	enum class Step : uint8_t;

	void send_request();
	void read_identification();
	void switch_baud();
	void read_line();
	void handle_object(std::string obis, std::string value);

	void verify_checksum();

	void change_status(Status to);

	HardwareSerial &serial_;
	Step step_;
	Status status_ = Status::Ready;
	uint8_t baud_char_, checksum_, rx_, tx_;
	std::map<std::string, std::string> values_;
	size_t errors_ = 0, checksum_errors_ = 0, successes_ = 0;
	unsigned long startTime_;
	const char *identifierChars_;
	std::string lastReadChars_;
};
